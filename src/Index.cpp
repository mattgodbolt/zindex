#include "Index.h"

#include "KeyValue.h"
#include "LineIndexer.h"
#include "LineSink.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
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
    uint64_t numLinesPlusOne;
    Header()
    : magic(HeaderMagic), version(Version), windowSize(WindowSize),
      numAccessPoints(0), numLinesPlusOne(0) {
    }
    void throwIfBroken() const {
        if (magic != HeaderMagic)
            throw std::runtime_error(
                    "Invalid or corrupt index file (bad magic)");
        if (version != Version)
            throw std::runtime_error("Unsupported version of index");
        if (windowSize != WindowSize)
            throw std::runtime_error("Unsupported window size");
    }
}__attribute__((packed));

struct Footer {
    uint64_t magic;
    Footer() :
            magic(FooterMagic) {
    }
    void throwIfBroken() const {
        if (magic != FooterMagic)
            throw std::runtime_error("Invalid or corrupt index file (at end)");
    }
}__attribute__((packed));

struct AccessPoint {
    uint64_t compressedOffset;
    uint8_t bitOffset;
    uint8_t padding[7];
    uint8_t window[WindowSize];
    AccessPoint(uint64_t compressedOffset, uint8_t bitOffset, uint64_t left,
            const uint8_t *window) :
            compressedOffset(compressedOffset), bitOffset(bitOffset) {
        memset(padding, 0, sizeof(padding));
        if (left)
            memcpy(this->window, window + WindowSize - left, left);
        if (left < WindowSize)
            memcpy(this->window + left, window, WindowSize - left);
    }
}__attribute__((packed));

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

template<typename T>
void writeArray(File &f, const T &array) {
    auto written = ::fwrite(&array[0], sizeof(array[0]), array.size(), f.get());
    if (written < 0) {
        throw std::runtime_error("Error writing to file"); // todo errno
    }
    if (written != array.size()) {
        throw std::runtime_error("Error writing to file: write truncated");
    }
}

template<typename T>
T read(File &f) {
    uint8_t data[sizeof(T)];
    auto nRead = ::fread(&data, 1, sizeof(T), f.get());
    if (nRead < 0) {
        throw std::runtime_error("Error reading from file"); // todo errno
    }
    if (nRead != sizeof(T)) {
        throw std::runtime_error("Error reading from file: truncated");
    }
    return *reinterpret_cast<const T *>(data);
}

template<typename T>
void readArray(File &f, uint64_t num, T &array) {
    array.resize(num);
    auto nRead = ::fread(&array[0], sizeof(array[0]), num, f.get());
    if (nRead < 0)
        throw std::runtime_error("Error while reading index");

    if (nRead != num)
        throw std::runtime_error("Error reading from file: truncated");
}

void seek(File &f, uint64_t pos) {
    auto err = ::fseek(f.get(), pos, SEEK_SET);
    if (err != 0)
        throw std::runtime_error("Error seeking in file"); // todo errno
}

struct ZlibError: std::runtime_error {
    ZlibError(int result) :
            std::runtime_error(
                    std::string("Error from zlib : ") + zError(result)) {
    }
};

struct ZStream {
    z_stream stream;
    enum class Type
        : int {
            ZlibOrGzip = 47, Raw = -15,
    };
    explicit ZStream(Type type) {
        memset(&stream, 0, sizeof(stream));
        int ret = inflateInit2(&stream, (int )type);
        if (ret != Z_OK)
            throw ZlibError(ret);
    }
    ~ZStream() {
        (void) inflateEnd(&stream);
    }
    ZStream(ZStream &) = delete;
    ZStream &operator=(ZStream &) = delete;
};

struct NullSink: LineSink {
    void onLine(size_t, size_t, const char *, size_t) override {
    }
};

}

struct Index::Impl {
    Header header;
    File compressed;
    File index;
    std::vector<uint64_t> uncompressedOffsets;
    std::vector<uint64_t> lines;

    Impl(const Header &h, File &&fromCompressed, File &&fromIndex)
    : header(h), compressed(std::move(fromCompressed)),
      index(std::move(fromIndex)) {
        seek(index,
                sizeof(Header) + header.numAccessPoints * sizeof(AccessPoint));

        readArray(index, header.numAccessPoints, uncompressedOffsets);
        readArray(index, header.numLinesPlusOne, lines);
        read<Footer>(index).throwIfBroken();
    }

    uint32_t accessPointFor(uint64_t offset) const {
        auto it = std::lower_bound(uncompressedOffsets.begin(),
                uncompressedOffsets.end(), offset);
        if (it == uncompressedOffsets.end())
            return -1;
        if (*it > offset && it != uncompressedOffsets.begin())
            --it;
        return it - uncompressedOffsets.begin();
    }

    void getLine(uint64_t line, LineSink &sink) {
        // Line is 1-based, lines is zero-based. lines also contains an extra
        // entry at the end of the file.
        if (line >= lines.size())
            return;
        if (line == 0) return;
        auto offset = lines[line - 1];
        auto length = lines[line] - offset;
        auto apNum = accessPointFor(offset);

        seek(index, sizeof(Header) + apNum * sizeof(AccessPoint));
        auto ap = read<AccessPoint>(index);

        ZStream zs(ZStream::Type::Raw);
        uint8_t input[ChunkSize];
        uint8_t window[WindowSize];

        std::cout << "ap " << apNum << " at offset " << ap.compressedOffset << " with unc lenth " << length << std::endl;

        seek(compressed, ap.bitOffset ? ap.compressedOffset - 1 : ap.compressedOffset);
        if (ap.bitOffset) {
            auto c = fgetc(compressed.get());
            std::cout << " at " << offset << " - " << +c << std::endl;
            if (c == -1)
                throw ZlibError(ferror(compressed.get()) ?
                        Z_ERRNO : Z_DATA_ERROR);
            auto ret = inflatePrime(
                    &zs.stream, ap.bitOffset, c >> (8 - ap.bitOffset));
            if (ret != Z_OK) throw ZlibError(ret);
        }
        auto ret = inflateSetDictionary(&zs.stream, ap.window,
                WindowSize);
        if (ret != Z_OK) throw ZlibError(ret);
    }
};

Index::Index() {
}
Index::~Index() {
}
Index::Index(std::unique_ptr<Impl> &&imp) :
        impl_(std::move(imp)) {
}

void Index::build(File &&from, File &&to) {
    Header header;
    write(to, header);

    ZStream zs(ZStream::Type::ZlibOrGzip);
    uint8_t input[ChunkSize];
    uint8_t window[WindowSize];

    int ret = 0;
    uint64_t totalIn = 0;
    uint64_t totalOut = 0;
    uint64_t last = 0;
    bool first = true;
    NullSink sink;
    LineIndexer indexer(sink);
    std::vector<uint64_t> uncompressedOffsets;

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
                AccessPoint ap(totalIn, numUnusedBits, zs.stream.avail_out,
                        window);
                std::cout << totalIn << std::endl;
                uncompressedOffsets.emplace_back(totalOut);
                write(to, ap);
                header.numAccessPoints++;
            }
        } while (zs.stream.avail_in);
    } while (ret != Z_STREAM_END);

    indexer.add(window, WindowSize - zs.stream.avail_out, true);
    writeArray(to, uncompressedOffsets);
    const auto &lineOffsets = indexer.lineOffsets();
    writeArray(to, lineOffsets);
    header.numLinesPlusOne = lineOffsets.size();

    write(to, Footer());
    seek(to, 0);
    write(to, header);
}

Index Index::load(File &&fromCompressed, File &&fromIndex) {
    auto header = read<Header>(fromIndex);
    header.throwIfBroken();

    std::unique_ptr<Impl> impl(new Impl(header, std::move(fromCompressed),
            std::move(fromIndex)));

    return Index(std::move(impl));
}

uint64_t Index::lineOffset(uint64_t line) const {
    if (line >= impl_->lines.size())
        return -1;
    return impl_->lines[line];
}

void Index::getLine(uint64_t line, LineSink &sink) {
    impl_->getLine(line, sink);
}
