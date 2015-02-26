#include "Index.h"

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr auto IndexEvery = 1024 * 1024;
constexpr auto WindowSize = 32768;
constexpr auto ChunkSize = 16384;
constexpr auto HeaderMagic = 0x1f0a5845444e495aull;
constexpr auto FooterMagic = 0x1f0a5a494e444558ull;
constexpr auto Version = 1;

struct Header {
    uint64_t magic;
    uint16_t version;
    uint16_t windowSize;
    uint32_t numIndices;
    Header()
    : magic(HeaderMagic), version(Version), windowSize(WindowSize),
      numIndices(0) {}
} __attribute((packed));

struct Footer {
    uint64_t magic;
    Footer()
    : magic(FooterMagic) {}
} __attribute((packed));

struct AccessPoint {
    uint64_t uncompressedOffset;
    uint64_t compressedOffset;
    uint8_t bitOffset;
    uint8_t padding[7];
    uint8_t window[WindowSize];
    AccessPoint(uint64_t uncompressedOffset, uint64_t compressedOffset,
            uint8_t bitOffset, uint64_t left, const uint8_t *window)
    : uncompressedOffset(uncompressedOffset),
      compressedOffset(compressedOffset), bitOffset(bitOffset) {
        memset(padding, 0, sizeof(padding));
        if (left)
            memcpy(this->window, window + WindowSize - left, left);
        if (left < WindowSize)
            memcpy(this->window + left, window, WindowSize - left);
    }
} __attribute((packed));

template<typename T>
void write(File &f, const T &obj) {
    auto written = ::fwrite(&obj, 1, sizeof(obj), f.get());
    if (written < 0) {
        throw std::runtime_error("Error writing to file"); // todo errno
    }
    if (written != sizeof(obj)) {
        throw std::runtime_error("Error writing to file: write truncated");
    }
}

void seek(File &f, uint64_t pos) {
    auto err = ::fseek(f.get(), pos, SEEK_SET);
    if (err != 0)
        throw std::runtime_error("Error seeking in file"); // todo errno
}

struct ZlibError : std::runtime_error {
    ZlibError(int result)
    : std::runtime_error(std::string("Error from zlib : ") + zError(result)) {}
};

struct ZStream {
    z_stream stream;
    ZStream() {
        memset(&stream, 0, sizeof(stream));
        int ret = inflateInit2(&stream, 47);  // 47 is the magic value to handle zlib or gzip decoding
        if (ret != Z_OK) throw ZlibError(ret);
    }
    ~ZStream() {
        (void)inflateEnd(&stream);
    }
    ZStream(ZStream &) = delete;
    ZStream &operator=(ZStream &) = delete;
};

}

struct Index::Impl {};

Index::Index() {}
Index::~Index() {}
Index::Index(std::unique_ptr<Impl> &&imp)
: impl_(std::move(imp)) {}

Index Index::build(File &&from, File &&to) {
    Header header;
    write(to, header);

    ZStream zs;
    uint8_t input[ChunkSize];
    uint8_t window[WindowSize];

    int ret = 0;
    uint64_t totalIn = 0;
    uint64_t totalOut = 0;
    uint64_t last = 0;

    do {
        zs.stream.avail_in = fread(input, 1, ChunkSize, from.get());
        if (ferror(from.get()))
            throw ZlibError(Z_ERRNO);
        if (zs.stream.avail_in == 0)
            throw ZlibError(Z_DATA_ERROR);
        zs.stream.next_in = input;
        do {
            if (zs.stream.avail_out == 0) {
                zs.stream.avail_out = WindowSize;
                zs.stream.next_out = window;
            }
            totalIn += zs.stream.avail_in;
            totalOut += zs.stream.avail_out;
            ret = inflate(&zs.stream, Z_BLOCK);
            totalIn -= zs.stream.avail_in;
            totalOut -= zs.stream.avail_out;
            if (ret == Z_NEED_DICT)
                throw ZlibError(Z_DATA_ERROR);
            if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                throw ZlibError(ret);
            if (ret == Z_STREAM_END)
                break;
            auto sinceLast = totalOut - last;
            bool needsIndex = sinceLast > IndexEvery || totalOut == 0;
            bool endOfBlock = zs.stream.data_type & 0x80;
            bool lastBlockInStream = zs.stream.data_type & 0x40;
            if (endOfBlock && !lastBlockInStream && needsIndex) {
                auto numUnusedBits = zs.stream.data_type & 0x7;
                AccessPoint ap(numUnusedBits, totalIn, totalOut,
                        zs.stream.avail_out, window);
                write(to, ap);
                header.numIndices++;
            }
        } while (zs.stream.avail_in);
    } while (ret != Z_STREAM_END);

    write(to, Footer());
    seek(to, 0);
    write(to, header);
    return Index();
}
