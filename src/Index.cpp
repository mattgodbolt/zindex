#include "Index.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <tuple>
#include <vector>

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
    uint32_t numAccessPoints;
    uint64_t numIndices;
    Header()
    : magic(HeaderMagic), version(Version), windowSize(WindowSize),
      numAccessPoints(0), numIndices(0) {}
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

struct KeyValue {
    uint64_t key;
    uint64_t value;

    friend bool operator <(const KeyValue &lhs, const KeyValue & rhs) {
        return std::tie(lhs.key, lhs.value) < std::tie(rhs.key, rhs.value);
    }
};

class LineIndexer {
    std::vector<KeyValue> keyValues_;
    uint64_t currentLineOffset_;
    uint64_t index_;
    uint64_t curLineLength_;
public:
    LineIndexer()
    : currentLineOffset_(0), index_(1), curLineLength_(0) {}

    void add(const uint8_t *data, uint64_t length, bool last) {
        while (length) {
            auto end = static_cast<const uint8_t *>(memchr(data, '\n', length));
            if (end) {
                keyValues_.emplace_back(KeyValue{index_, currentLineOffset_});
                index_++;
                auto extraLength = (end - data) + 1;
                curLineLength_ += extraLength;
                length -= extraLength;
                data = end + 1;
                currentLineOffset_ += curLineLength_;
            } else {
                curLineLength_ += length;
                length = 0;
            }
        }
        if (last)
            keyValues_.emplace_back(KeyValue{index_, currentLineOffset_});
    }

    void output(File &out) {
        std::sort(keyValues_.begin(), keyValues_.end());
        auto written = ::fwrite(&keyValues_[0], sizeof(KeyValue),
                keyValues_.size(), out.get());
        if (written < 0) {
            throw std::runtime_error("Error writing to file"); // todo errno
        }
        if (written != keyValues_.size()) {
            throw std::runtime_error("Error writing to file: write truncated");
        }
    }

    uint64_t size() const {
        return keyValues_.size();
    }
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
    bool first = true;
    LineIndexer indexer;

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
                if (!first) {
                    indexer.add(window, WindowSize, false);
                }
                first = false;
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
                header.numAccessPoints++;
            }
        } while (zs.stream.avail_in);
    } while (ret != Z_STREAM_END);

    indexer.add(window, WindowSize - zs.stream.avail_out, true);
    indexer.output(to);
    header.numIndices = indexer.size();

    write(to, Footer());
    seek(to, 0);
    write(to, header);
    return Index();
}
