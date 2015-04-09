#include "Index.h"

#include "LineFinder.h"
#include "LineSink.h"
#include "LineIndexer.h"
#include "Sqlite.h"

#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <unordered_map>
#include "IndexSink.h"

namespace {

constexpr auto IndexEvery = 32 * 1024 * 1024;
constexpr auto WindowSize = 32768;
constexpr auto ChunkSize = 16384;

void seek(File &f, uint64_t pos) {
    auto err = ::fseek(f.get(), pos, SEEK_SET);
    if (err != 0)
        throw std::runtime_error("Error seeking in file"); // todo errno
}

struct ZlibError : std::runtime_error {
    ZlibError(int result) :
            std::runtime_error(
                    std::string("Error from zlib : ") + zError(result)) { }
};

void R(int zlibErr) {
    if (zlibErr != Z_OK) throw ZlibError(zlibErr);
}

size_t makeWindow(uint8_t *out, size_t outSize, const uint8_t *in,
                  uint64_t left) {
    uint8_t temp[WindowSize];
    // Could compress directly into out if I wasn't so lazy.
    if (left)
        memcpy(temp, in + WindowSize - left, left);
    if (left < WindowSize)
        memcpy(temp + left, in, WindowSize - left);
    uLongf destLen = outSize;
    R(compress2(out, &destLen, temp, WindowSize, 9));
    return destLen;
}

void uncompress(const std::vector<uint8_t> &compressed, uint8_t *to,
                size_t len) {
    uLongf destLen = len;
    R(::uncompress(to, &len, &compressed[0], compressed.size()));
    if (destLen != len)
        throw std::runtime_error("Unable to decompress a full window");
}

struct ZStream {
    z_stream stream;
    enum class Type : int {
        ZlibOrGzip = 47, Raw = -15,
    };

    explicit ZStream(Type type) {
        memset(&stream, 0, sizeof(stream));
        R(inflateInit2(&stream, (int)type));
    }

    ~ZStream() {
        (void)inflateEnd(&stream);
    }

    ZStream(ZStream &) = delete;

    ZStream &operator=(ZStream &) = delete;
};

struct IndexHandler : IndexSink {
    LineIndexer &indexer;
    uint64_t currentLine;

    IndexHandler(LineIndexer &indexer) : indexer(indexer), currentLine(0) { }

    virtual ~IndexHandler() { }

    void onLine(uint64_t lineNumber, const char *line, size_t length) {
        currentLine = lineNumber;
        indexer.index(*this, line, length);
    }
};

struct NumericHandler : IndexHandler {
    Sqlite::Statement insert;

    NumericHandler(LineIndexer &indexer, Sqlite::Statement &&insert)
            : IndexHandler(indexer), insert(std::move(insert)) { }

    void add(const char *index, size_t indexLength, size_t offset) override {
        auto initIndex = index;
        auto initLen = indexLength;
        int64_t val = 0;
        bool negative = false;
        if (indexLength > 0 && *index == '-') {
            negative = true;
            indexLength--;
            index++;
        }
        if (indexLength == 0)
            throw std::invalid_argument("Non-numeric: empty string");
        while (indexLength) {
            val *= 10;
            if (*index < '0' || *index > '9')
                throw std::invalid_argument("Non-numeric: '"
                                            + std::string(initIndex, initLen) +
                                            "'");
            val += *index - '0';
            index++;
            indexLength--;
        }
        if (negative) val = -val;
        insert
                .reset()
                .bindInt64(":key", val)
                .bindInt64(":line", currentLine)
                .bindInt64(":offset", offset)
                .step();
    }
};

}

struct Index::Impl {
    File compressed_;
    Sqlite db_;
    Sqlite::Statement lineQuery_;

    Impl(File &&fromCompressed, Sqlite &&db)
            : compressed_(std::move(fromCompressed)), db_(std::move(db)),
              lineQuery_(db_.prepare(R"(
SELECT line, offset, compressedOffset, uncompressedOffset, length, bitOffset, window
FROM LineOffsets, AccessPoints
WHERE offset >= uncompressedOffset AND offset <= uncompressedEndOffset
AND line = :line
LIMIT 1)")) { }

    void getLine(uint64_t line, LineSink &sink) {
        lineQuery_.reset();
        lineQuery_.bindInt64(":line", line);
        if (lineQuery_.step()) return;
        print(lineQuery_, sink);
    }

    void queryIndex(const std::string &index, const std::string &query,
                    LineSink &sink) {
        auto stmt = db_.prepare(R"(
SELECT line FROM index_)" + index + R"(
WHERE key = :query
)");
        stmt.bindString(":query", query);
        for (; ;) {
            if (stmt.step()) return;
            getLine(stmt.columnInt64(0), sink);
        }
    }

    void print(Sqlite::Statement &q, LineSink &sink) {
        auto line = q.columnInt64(0);
        auto offset = q.columnInt64(1);
        auto compressedOffset = q.columnInt64(2);
        auto uncompressedOffset = q.columnInt64(3);
        auto length = q.columnInt64(4);
        auto bitOffset = q.columnInt64(5);
        uint8_t window[WindowSize];
        uncompress(q.columnBlob(6), window, WindowSize);

        ZStream zs(ZStream::Type::Raw);
        uint8_t input[ChunkSize];
        uint8_t discardBuffer[WindowSize];

        seek(compressed_, bitOffset ? compressedOffset - 1 : compressedOffset);
        if (bitOffset) {
            auto c = fgetc(compressed_.get());
            if (c == -1)
                throw ZlibError(ferror(compressed_.get()) ?
                                Z_ERRNO : Z_DATA_ERROR);
            R(inflatePrime(&zs.stream, bitOffset, c >> (8 - bitOffset)));
        }
        R(inflateSetDictionary(&zs.stream, &window[0], WindowSize));

        uint8_t lineBuf[length];
        auto numToSkip = offset - uncompressedOffset;
        bool skipping = true;
        zs.stream.avail_in = 0;
        do {
            if (numToSkip == 0 && skipping) {
                zs.stream.avail_out = length;
                zs.stream.next_out = lineBuf;
                skipping = false;
            }
            if (numToSkip > WindowSize) {
                zs.stream.avail_out = WindowSize;
                zs.stream.next_out = discardBuffer;
                numToSkip -= WindowSize;
            } else if (numToSkip) {
                zs.stream.avail_out = (uInt)numToSkip;
                zs.stream.next_out = discardBuffer;
                numToSkip = 0;
            }
            do {
                if (zs.stream.avail_in == 0) {
                    zs.stream.avail_in = ::fread(input, 1, sizeof(input),
                                                 compressed_.get());
                    if (ferror(compressed_.get())) throw ZlibError(Z_ERRNO);
                    if (zs.stream.avail_in == 0) throw ZlibError(Z_DATA_ERROR);
                    zs.stream.next_in = input;
                }
                auto ret = inflate(&zs.stream, Z_NO_FLUSH);
                if (ret == Z_NEED_DICT) throw ZlibError(Z_DATA_ERROR);
                if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                    throw ZlibError(ret);
                if (ret == Z_STREAM_END) break;
            } while (zs.stream.avail_out);
        } while (skipping);
        sink.onLine(line, offset, reinterpret_cast<const char *>(lineBuf),
                    length - 1);
    }
};

Index::Index() { }

Index::~Index() { }

Index::Index(std::unique_ptr<Impl> &&imp) : impl_(std::move(imp)) { }

struct Index::Builder::Impl : LineSink {
    File from;
    std::string indexFilename;
    Sqlite db;
    Sqlite::Statement addIndexSql;
    std::unordered_map<std::string, std::unique_ptr<IndexHandler>> indexers;

    Impl(File &&from, const std::string &indexFilename)
            : from(std::move(from)), indexFilename(indexFilename) { }

    void init() {
        unlink(indexFilename.c_str());
        db.open(indexFilename, false);

        db.exec(R"(PRAGMA synchronous = OFF)");
        db.exec(R"(PRAGMA journal_mode = MEMORY)");
        db.exec(R"(PRAGMA application_id = 0x5a494458)");

        db.exec(R"(
CREATE TABLE AccessPoints(
    uncompressedOffset INTEGER PRIMARY KEY,
    uncompressedEndOffset INTEGER,
    compressedOffset INTEGER,
    bitOffset INTEGER,
    window BLOB
))");

        db.exec(R"(
CREATE TABLE LineOffsets(
    line INTEGER PRIMARY KEY,
    offset INTEGER,
    length INTEGER
))");

        db.exec(R"(
CREATE TABLE Indexes(
    name STRING PRIMARY KEY,
    creationString STRING,
    isNumeric INTEGER
))");
        addIndexSql = db.prepare(R"(
INSERT INTO Indexes VALUES(:name, :creationString, :isNumeric)
)");
    }

    void build() {
        db.exec(R"(BEGIN TRANSACTION)");

        auto addIndex = db.prepare(R"(
INSERT INTO AccessPoints VALUES(
:uncompressedOffset, :uncompressedEndOffset,
:compressedOffset, :bitOffset, :window))");
        auto addLine = db.prepare(R"(
INSERT INTO LineOffsets VALUES(:line, :offset, :length))");

        ZStream zs(ZStream::Type::ZlibOrGzip);
        uint8_t input[ChunkSize];
        uint8_t window[WindowSize];

        int ret = 0;
        uint64_t totalIn = 0;
        uint64_t totalOut = 0;
        uint64_t last = 0;
        bool first = true;
        LineFinder finder(*this);

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
                        finder.add(window, WindowSize, false);
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
                    if (totalOut != 0) {
                        // Flush previous information.
                        addIndex.bindInt64(":uncompressedEndOffset",
                                           totalOut - 1);
                        addIndex.step();
                        addIndex.reset();
                    }
                    uint8_t apWindow[compressBound(WindowSize)];
                    auto size = makeWindow(apWindow, sizeof(apWindow), window,
                                           zs.stream.avail_out);
                    addIndex.bindInt64(":uncompressedOffset", totalOut);
                    addIndex.bindInt64(":compressedOffset", totalIn);
                    addIndex.bindInt64(":bitOffset", zs.stream.data_type & 0x7);
                    addIndex.bindBlob(":window", apWindow, size);
                    last = totalOut;
                }
            } while (zs.stream.avail_in);
        } while (ret != Z_STREAM_END);

        if (totalOut != 0) {
            // Flush last block.
            addIndex.bindInt64(":uncompressedEndOffset", totalOut - 1);
            addIndex.step();
        }

        finder.add(window, WindowSize - zs.stream.avail_out, true);
        const auto &lineOffsets = finder.lineOffsets();
        for (size_t line = 0; line < lineOffsets.size() - 1; ++line) {
            addLine.reset();
            addLine.bindInt64(":line", line + 1);
            addLine.bindInt64(":offset", lineOffsets[line]);
            addLine.bindInt64(":length",
                              lineOffsets[line + 1] - lineOffsets[line]);
            addLine.step();
        }

        db.exec(R"(END TRANSACTION)");
    }

    void addIndexer(const std::string &name, const std::string &creation,
                    bool numeric, bool unique, LineIndexer &indexer) {
        auto table = "index_" + name;
        std::string type = numeric ? "INTEGER" : "STRING";
        if (unique) type += " PRIMARY KEY";
        db.exec(R"(
CREATE TABLE )" + table + R"((
    key )" + type + R"(,
    line INTEGER,
    offset INTEGER
))");
        addIndexSql
                .reset()
                .bindString(":name", name)
                .bindString(":creationString", creation)
                .bindInt64(":isNumeric", numeric ? 1 : 0)
                .step();

        auto inserter = db.prepare(R"(
INSERT INTO )" + table + R"( VALUES(:key, :line, :offset)
)");
        if (numeric) {
            indexers.emplace(name, std::unique_ptr<IndexHandler>(
                    new NumericHandler(indexer, std::move(inserter))));
        } else {
            // TODO!
//            indexers.emplace(name, indexer);
        }
    }

    void onLine(
            size_t lineNumber,
            size_t fileOffset,
            const char *line, size_t length) override {
        for (auto &&pair : indexers) {
            pair.second->onLine(lineNumber, line, length);
        }
    }
};

Index::Builder::Builder(File &&from, const std::string &indexFilename)
        : impl_(new Impl(std::move(from), indexFilename)) {
    impl_->init();
}

void Index::Builder::addIndexer(
        const std::string &name,
        const std::string &creation,
        bool numeric,
        bool unique,
        LineIndexer &indexer) {
    impl_->addIndexer(name, creation, numeric, unique, indexer);
}

void Index::Builder::build() {
    impl_->build();
}

Index Index::load(File &&fromCompressed, const char *indexFilename) {
    Sqlite db;
    db.open(indexFilename, true);

    std::unique_ptr<Impl> impl(new Impl(std::move(fromCompressed),
                                        std::move(db)));

    return Index(std::move(impl));
}

void Index::getLine(uint64_t line, LineSink &sink) {
    impl_->getLine(line, sink);
}

void Index::getLines(const std::vector<uint64_t> &lines, LineSink &sink) {
    for (auto line : lines) impl_->getLine(line, sink);
}

void Index::queryIndex(const std::string &index, const std::string &query,
                       LineSink &sink) {
    impl_->queryIndex(index, query, sink);
}

void Index::queryIndexMulti(const std::string &index,
                            const std::vector<std::string> &queries,
                            LineSink &sink) {
    // TODO be a little smarter about this.
    for (auto query : queries) impl_->queryIndex(index, query, sink);
}

Index::Builder::~Builder() {
}
