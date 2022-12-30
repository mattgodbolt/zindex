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
#include "Log.h"
#include "StringView.h"
#include "PrettyBytes.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

constexpr auto DefaultIndexEvery = 32 * 1024 * 1024u;
constexpr auto WindowSize = 32768u;
constexpr auto ChunkSize = 16384u;
constexpr auto Version = 1;

void seek(File &f, uint64_t pos) {
    auto err = ::fseek(f.get(), pos, SEEK_SET);
    if (err != 0)
        throw std::runtime_error("Error seeking in file"); // todo errno
}

struct ZlibError : std::runtime_error {
    explicit ZlibError(int result) :
            std::runtime_error(
                    std::string("Error from zlib : ") + zError(result)) {}
};

void X(int zlibErr) {
    if (zlibErr != Z_OK) throw ZlibError(zlibErr);
}

struct Progress {
    time_t nextProgress = 0;
    Log &log;
    static constexpr auto LogProgressEverySecs = 20u;

    Progress(Log &log) : log(log) {}

    template<typename Printer>
    void update(size_t progress, size_t of) {
        auto now = time(nullptr);
        if (now < nextProgress) return;
        char pc[16];
        snprintf(pc, sizeof(pc) - 1, "%.2f",
                 (progress * 100.0) / of);
        log.info("Progress: ", Printer(progress), " of ",
                 Printer(of), " (", pc, "%)");
        nextProgress = now + LogProgressEverySecs;
    }
};


struct OneLineSink : LineSink {
    bool complete = false;
    const size_t lineNum;
    LineSink &delegate;
    const char *lastLine;

    OneLineSink(size_t lineNum, LineSink &delegate) : lineNum(lineNum), delegate(delegate) {}

    bool onLine(
            size_t lineNumber,
            size_t fileOffset,
            const char *line, size_t length) override {
        lastLine = line;
        if (lineNum == lineNumber) {
            delegate.onLine(lineNumber, fileOffset, line, length);
            complete = true;
        }

        return true;
    }
    
};

struct DelegatinhPrintSink : LineSink {
    bool enabled;
    LineSink &sink;
    DelegatinhPrintSink(bool enabled, LineSink &sink) : enabled(enabled), sink(sink) { }

    bool onLine(size_t l, size_t size, const char *line, size_t length) override {
        if (enabled) {
            std::cout << l << ":" << std::string(line, length) << std::endl;
        }
        
        return sink.onLine(l, size, line, length);
    }
};

size_t makeWindow(uint8_t *out, size_t outSize, const uint8_t *in,
                  uint64_t left) {
    uint8_t temp[WindowSize];
    // Could compress directly into out if I wasn't so lazy.
    if (left)
        memcpy(temp, in + WindowSize - left, left);
    if (left < WindowSize)
        memcpy(temp + left, in, WindowSize - left);
    uLongf destLen = outSize;
    X(compress2(out, &destLen, temp, WindowSize, 9));
    return destLen;
}

void uncompress(const std::vector<uint8_t> &compressed, uint8_t *to,
                size_t len) {
    uLongf destLen = len;
    X(::uncompress(to, &len, &compressed[0], compressed.size()));
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
        X(inflateInit2(&stream, (int)type));
    }

    void reset() {
        X(inflateReset(&stream));
    }

    ~ZStream() {
        (void)inflateEnd(&stream);
    }

    ZStream(ZStream &) = delete;

    ZStream &operator=(ZStream &) = delete;
};

struct IndexHandler : IndexSink {
    Log &log;
    std::unique_ptr<LineIndexer> indexer;
    uint64_t currentLine;
    bool indexed = false;

    IndexHandler(Log &log, std::unique_ptr<LineIndexer> indexer) :
            log(log), indexer(std::move(indexer)), currentLine(0) {}

    ~IndexHandler() override = default;

    bool onLine(uint64_t lineNumber, const char *line, size_t length) {
        try {
            indexed = false;
            currentLine = lineNumber;
            StringView stringView(line, length);
            log.debug("Indexing line '", stringView, "'");
            indexer->index(*this, stringView);
            return indexed;
        } catch (const std::exception &e) {
            throw std::runtime_error(
                    "Failed to index line " + std::to_string(currentLine)
                    + ": '" + std::string(line, length) +
                    "' - " + e.what());
        }
    }
};

struct AlphaHandler : IndexHandler {
    Sqlite::Statement insert;

    AlphaHandler(Log &log, std::unique_ptr<LineIndexer> indexer,
                 Sqlite::Statement &&insert)
            : IndexHandler(log, std::move(indexer)),
              insert(std::move(insert)) {}

    void add(StringView key, size_t offset) override {
        indexed = true;
        log.debug("Found key '", key, "'");
        insert
                .reset()
                .bindString(":key", key)
                .bindInt64(":line", currentLine)
                .bindInt64(":offset", offset)
                .step();
    }
};

struct NumericHandler : IndexHandler {
    Sqlite::Statement insert;

    NumericHandler(Log &log, std::unique_ptr<LineIndexer> indexer,
                   Sqlite::Statement &&insert)
            : IndexHandler(log, std::move(indexer)),
              insert(std::move(insert)) {}

    void add(StringView key, size_t offset) override {
        indexed = true;
        auto index = key.begin();
        auto initIndex = key.begin();
        auto indexLength = key.length();
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
        log.debug("Found key ", val);
        insert
                .reset()
                .bindInt64(":key", val)
                .bindInt64(":line", currentLine)
                .bindInt64(":offset", offset)
                .step();
    }
};

struct CachedContext {
    size_t uncompressedOffset_;
    size_t blockSize_;
    uint8_t input_[ChunkSize];
    ZStream zs_;

    explicit CachedContext(size_t uncompressedOffset, size_t blockSize)
            : uncompressedOffset_(uncompressedOffset),
              blockSize_(blockSize),
              zs_(ZStream::Type::Raw) {}

    bool offsetWithinRange(size_t offset) const {
        if (offset < uncompressedOffset_) return false; // can't seek backwards
        size_t jumpAhead = offset - uncompressedOffset_;
        return jumpAhead < blockSize_;
    }
};

}

struct Index::Impl {
    Log &log_;
    File compressed_;
    Sqlite db_;
    Sqlite::Statement lineQuery_;
    Sqlite::Statement sparseLineOffsetsQuery_;
    Index::Metadata metadata_;
    size_t blockSize_;
    std::unique_ptr<CachedContext> cachedContext_;
    bool sparseLineOffsets_;

    Impl(Log &log, File &&fromCompressed, Sqlite &&db)
            : log_(log), compressed_(std::move(fromCompressed)),
              db_(std::move(db)),
              lineQuery_(db_.prepare(R"(
SELECT lo.line, lo.offset, ap.compressedOffset, ap.uncompressedOffset, lo.length, ap.bitOffset, ap.window, ap.lineNum
FROM LineOffsets lo, AccessPoints ap
WHERE lo.offset >= ap.uncompressedOffset AND offset <= ap.uncompressedEndOffset
AND lo.line = :line
LIMIT 1)")), 
            sparseLineOffsetsQuery_(db_.prepare(R"(
SELECT lineNum, compressedOffset, uncompressedOffset, bitOffset, window
FROM AccessPoints where lineNum <= :line order by lineNum desc
LIMIT 1)")) {
        try {
            auto queryMeta = db_.prepare("SELECT key, value FROM Metadata");
            for (;;) {
                if (queryMeta.step()) break;
                auto key = queryMeta.columnString(0);
                auto value = queryMeta.columnString(1);
                log_.debug("Metadata: ", key, " = ", value);
                metadata_.emplace(key, value);
                
            }
        } catch (const std::exception &e) {
            log.warn("Caught exception reading metadata: ", e.what());
        }
        
        sparseLineOffsets_ = metadata_.find("sparseLineOffsets") != metadata_.end() && metadata_.at("sparseLineOffsets") == "1";
    
        auto stmt = db_.prepare(
                "SELECT MAX(uncompressedEndOffset)/COUNT(*) FROM AccessPoints");
        if (stmt.step()) {
            blockSize_ = 32 * 1024 * 1024;
        } else {
            blockSize_ = stmt.columnInt64(0);
        }
        log_.debug("Average block size ", PrettyBytes(blockSize_));
    }

    void init(bool force) {
        struct stat stats;
        if (fstat(fileno(compressed_.get()), &stats) != 0) {
            throw std::runtime_error("Unable to get file stats"); // todo errno
        }
        auto sizeStr = std::to_string(stats.st_size);
        auto timeStr = std::to_string(stats.st_mtime);
        log_.debug("Opened compressed file of size ", sizeStr, " mtime ",
                   timeStr);
        if (metadata_.find("compressedSize") != metadata_.end()
            && sizeStr != metadata_.at("compressedSize")) {
            if (force) {
                log_.warn("Expected compressed size mismatched, "
                                  "continuing anyway (", stats.st_size,
                          " vs expected ",
                          metadata_.at("compressedSize"), ")");
            } else {
                throw std::runtime_error(
                        "Compressed size changed since index was built");
            }
        }
        if (metadata_.find("compressedModTime") != metadata_.end()
            && timeStr != metadata_.at("compressedModTime")) {
            if (force) {
                log_.warn("Expected compressed timestamp, continuing anyway");
            } else {
                throw std::runtime_error(
                        "Compressed file has been modified since index was built");
            }
        }

       
    }

    bool getLine(uint64_t line, LineSink &sink) {
        lineQuery_.reset();
        lineQuery_.bindInt64(":line", line);
        auto noResults = lineQuery_.step();
        if (noResults) {
            if (sparseLineOffsets_) {
                
                sparseLineOffsetsQuery_.reset();
                sparseLineOffsetsQuery_.bindInt64(":line", line);
                if (sparseLineOffsetsQuery_.step()) {
                    return false;
                }                
                auto lineNum = static_cast<size_t>(sparseLineOffsetsQuery_.columnInt64(0));
                auto compressedOffset = static_cast<size_t>(sparseLineOffsetsQuery_.columnInt64(1));
                auto uncompressedOffset = static_cast<size_t>(sparseLineOffsetsQuery_.columnInt64(2));
                auto bitOffset = static_cast<int>(sparseLineOffsetsQuery_.columnInt64(3));
                auto windowData = sparseLineOffsetsQuery_.columnBlob(4);
                print(lineNum, line, 0, compressedOffset, uncompressedOffset, bitOffset, 0, windowData, sink);
            }
            return false;
        }
        

        print(lineQuery_, sink);
        return true;
    }

    size_t queryIndex(const std::string &index, const std::string &query,
                      LineFunction lineFunc) {
        auto stmt = db_.prepare(R"(
SELECT line FROM index_)" + index + R"(
WHERE key = :query
)");
        stmt.bindString(":query", query);
        size_t matches = 0;
        for (;;) {
            if (stmt.step()) return matches;
            lineFunc(stmt.columnInt64(0));
            ++matches;
        }
    }

    size_t customQuery(const std::string &customQuery, LineFunction lineFunc) {
        auto stmt = db_.prepare(customQuery);
        size_t matches = 0;
        for (;;) {
            if (stmt.step()) return matches;
            lineFunc(stmt.columnInt64(0));
            ++matches;
        }
        return matches;
    }

    size_t indexSize(const std::string &index) const {
        return tableSize("index_" + index);
    }

    size_t tableSize(const std::string &name) const {
        auto stmt = db_.prepare("SELECT COUNT(*) FROM " + name);
        if (stmt.step()) return 0;
        return static_cast<size_t>(stmt.columnInt64(0));
    }

    void print(Sqlite::Statement &q, LineSink &sink) {
        auto line = static_cast<size_t>(q.columnInt64(0));
        auto offset = static_cast<size_t>(q.columnInt64(1));
        auto compressedOffset = static_cast<size_t>(q.columnInt64(2));
        auto uncompressedOffset = static_cast<size_t>(q.columnInt64(3));
        auto bitOffset = static_cast<int>(q.columnInt64(5));
        auto length = q.columnInt64(4);
        auto windowData = q.columnBlob(6);
        print(0, line, offset, compressedOffset, uncompressedOffset, bitOffset, length, windowData, sink);
    }

    std::unique_ptr<CachedContext> resetContext(size_t offset, size_t compressedOffset, size_t uncompressedOffset, int bitOffset, const std::vector<uint8_t> windowData) {
        std::unique_ptr<CachedContext> context;
        if (cachedContext_ && cachedContext_->offsetWithinRange(offset)) {
            // We can reuse the previous context.
            log_.debug("Reusing previous context");
            context = std::move(cachedContext_);
        } else {
            log_.debug("Creating new context at offset ", compressedOffset, ":",
                       bitOffset);
            context.reset(new CachedContext(uncompressedOffset, blockSize_));
            uint8_t window[WindowSize];
            uncompress(windowData, window, WindowSize);

            seek(compressed_, bitOffset ? compressedOffset - 1
                                        : compressedOffset);
            context->zs_.stream.avail_in = 0;
            if (bitOffset) {
                auto c = fgetc(compressed_.get());
                if (c == -1)
                    throw ZlibError(ferror(compressed_.get()) ?
                                    Z_ERRNO : Z_DATA_ERROR);
                X(inflatePrime(&context->zs_.stream,
                               bitOffset, c >> (8 - bitOffset)));
            }
            X(inflateSetDictionary(&context->zs_.stream,
                                   &window[0], WindowSize));
        }
        return context;
    }

    void print(size_t lineNum, size_t line, size_t offset, size_t compressedOffset, size_t uncompressedOffset, int bitOffset, size_t length, const std::vector<uint8_t> windowData, LineSink &sink) {
        // We use and update context while in here. Only if we successfully
        // decode a line do we save it in the cachedContext_ for a subsequent
        // call.
        constexpr auto MaxLength = (64u * 1024 * 1024) - 1;
        std::unique_ptr<CachedContext> context = resetContext(offset != 0 ? offset : uncompressedOffset - 1, compressedOffset, uncompressedOffset, bitOffset, windowData);
        uint8_t discardBuffer[WindowSize];        
        auto &zs = context->zs_;
        if (lineNum != 0) {
            //pass the data to linefinder until we pass the desired line

            OneLineSink oneLiner(line - lineNum, sink);
            DelegatinhPrintSink delegating(false, oneLiner);
            LineFinder finder(delegating);

            bool first = true;
            auto ret = 0;
            uint8_t window[WindowSize];


            do {
                if (zs.stream.avail_in == 0) {
                    zs.stream.avail_in = ::fread(context->input_, 1,
                                                 sizeof(context->input_),
                                                 compressed_.get());
                    if (ferror(compressed_.get())) throw ZlibError(Z_ERRNO);
                    if (zs.stream.avail_in == 0) throw ZlibError(Z_DATA_ERROR);
                    zs.stream.next_in = context->input_;
                }

                if (zs.stream.avail_out == 0) {
                    zs.stream.avail_out = WindowSize;
                    zs.stream.next_out = window;
                    if (!first) {
                        finder.add(window, WindowSize, false);
                    }
                    first = false;
                }
                
                auto availBefore = zs.stream.avail_out;
                ret = inflate(&zs.stream, Z_NO_FLUSH);
                if (ret == Z_NEED_DICT) throw ZlibError(Z_DATA_ERROR);
                if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                    throw ZlibError(ret);
                auto numUncompressed = availBefore - zs.stream.avail_out;
                context->uncompressedOffset_ += numUncompressed;
                if (ret == Z_STREAM_END) {
                    // The end of the stream; but is it the end of the file?
                    if (zs.stream.avail_in == 0 && feof(compressed_.get()))
                        break; // yes: we're done
                    // Else, we're going to restart the decompressor: gzip
                    // allows for multiple gzip files to be concatenated
                    // together and says they should be treated as a single
                    // file.
                    log_.debug("Resetting stream at EOS at offset ",
                               context->uncompressedOffset_);
                    // As we opened the stream in RAW mode we can't continue
                    // decoding from here: the decoder doesn't know if there's
                    // a trailing CRC or similar here. So we have to throw away
                    // state at this point. We know the indexer always stops us
                    // from crossing a divide here. We check we weren't skipping
                    // still (which we ought not to be...) and then chuck the
                    // whole context out.  Ideally we'd skip the (optional?)
                    // trailer and reset the zlib state back in normal, non-raw
                    // mode and play on.
                    context.reset();
                }
            } while (ret != Z_STREAM_END);
            finder.add(window, WindowSize - zs.stream.avail_out, true);

            return;
        }


        if (length > MaxLength) throw std::runtime_error("Line too long!");
        std::unique_ptr<uint8_t[]> lineBuf(new uint8_t[length]);

        auto numToSkip = offset - context->uncompressedOffset_;
        bool skipping = true;

        do {
            if (numToSkip == 0 && skipping) {
                zs.stream.avail_out = static_cast<uInt>(length);
                zs.stream.next_out = lineBuf.get();
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
                    zs.stream.avail_in = ::fread(context->input_, 1,
                                                 sizeof(context->input_),
                                                 compressed_.get());
                    if (ferror(compressed_.get())) throw ZlibError(Z_ERRNO);
                    if (zs.stream.avail_in == 0) throw ZlibError(Z_DATA_ERROR);
                    zs.stream.next_in = context->input_;
                }
                auto availBefore = zs.stream.avail_out;
                auto ret = inflate(&zs.stream, Z_NO_FLUSH);
                if (ret == Z_NEED_DICT) throw ZlibError(Z_DATA_ERROR);
                if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR)
                    throw ZlibError(ret);
                auto numUncompressed = availBefore - zs.stream.avail_out;
                context->uncompressedOffset_ += numUncompressed;
                if (ret == Z_STREAM_END) {
                    // The end of the stream; but is it the end of the file?
                    if (zs.stream.avail_in == 0 && feof(compressed_.get()))
                        break; // yes: we're done
                    // Else, we're going to restart the decompressor: gzip
                    // allows for multiple gzip files to be concatenated
                    // together and says they should be treated as a single
                    // file.
                    log_.debug("Resetting stream at EOS at offset ",
                               context->uncompressedOffset_);
                    // As we opened the stream in RAW mode we can't continue
                    // decoding from here: the decoder doesn't know if there's
                    // a trailing CRC or similar here. So we have to throw away
                    // state at this point. We know the indexer always stops us
                    // from crossing a divide here. We check we weren't skipping
                    // still (which we ought not to be...) and then chuck the
                    // whole context out.  Ideally we'd skip the (optional?)
                    // trailer and reset the zlib state back in normal, non-raw
                    // mode and play on.
                    if (skipping)
                        throw std::runtime_error(
                                "Tried to cross a gzip stream boundary");
                    context.reset();
                }
            } while (zs.stream.avail_out);
        } while (skipping);
        // Save the context for next time.
        cachedContext_ = std::move(context);
        log_.debug("lineNum:" + lineNum);
        sink.onLine(line, offset, reinterpret_cast<const char *>(lineBuf.get()),
                    length - 1);
    }
};
Index::Index() { }

Index::~Index() {}

Index::Index(Index &&other) : impl_(std::move(other.impl_)) {}

Index::Index(std::unique_ptr<Impl> &&imp) : impl_(std::move(imp)) {}

struct Index::Builder::Impl : LineSink {
    Log &log;
    File from;
    std::string fromPath;
    std::string indexFilename;
    uint64_t skipFirst;
    Sqlite db;
    Sqlite::Statement addIndexSql;
    Sqlite::Statement addMetaSql;
    uint64_t indexEvery = DefaultIndexEvery;
    std::unordered_map<std::string, std::unique_ptr<IndexHandler>> indexers;
    bool saveAllLines_;
    bool sparseLineOffsets_;

    Impl(Log &log, File &&from, const std::string &fromPath,
         const std::string &indexFilename)
            : log(log), from(std::move(from)), fromPath(fromPath),
              indexFilename(indexFilename), skipFirst(0), db(log),
              addIndexSql(log), addMetaSql(log), saveAllLines_{false}, sparseLineOffsets_{false} {}

    void init() {
        auto file = db.toFile(indexFilename);
        if (unlink(file.c_str()) == 0) {
            log.warn("Rebuilding existing index ", indexFilename);
        }
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
    window BLOB,
    lineNum INTEGER
))");

        db.exec(R"(
CREATE TABLE Metadata(
    key TEXT PRIMARY KEY,
    value TEXT
))");
        addMetaSql = db.prepare("INSERT INTO Metadata VALUES(:key, :value)");
        addMeta("version", std::to_string(Version));
        addMeta("compressedFile", fromPath);
        struct stat stats;
        if (fstat(fileno(from.get()), &stats) != 0) {
            throw std::runtime_error("Unable to get file stats"); // todo errno
        }
        addMeta("compressedSize", std::to_string(stats.st_size));
        addMeta("compressedModTime", std::to_string(stats.st_mtime));

        db.exec(R"(
CREATE TABLE LineOffsets(
    line INTEGER PRIMARY KEY,
    offset INTEGER,
    length INTEGER
))");

        db.exec(R"(
CREATE TABLE Indexes(
    name TEXT PRIMARY KEY,
    creationString TEXT,
    isNumeric INTEGER
))");
        addIndexSql = db.prepare(R"(
INSERT INTO Indexes VALUES(:name, :creationString, :isNumeric)
)");
    }

    void build() {
        addMeta("sparse", std::to_string(!saveAllLines_));
        addMeta("sparseLineOffsets", std::to_string(sparseLineOffsets_));
        log.info("Building index, generating a checkpoint every ",
                 PrettyBytes(indexEvery));
        struct stat compressedStat;
        if (fstat(fileno(from.get()), &compressedStat) != 0)
            throw ZlibError(Z_DATA_ERROR);

        db.exec(R"(BEGIN TRANSACTION)");

        auto addIndex = db.prepare(R"(
INSERT INTO AccessPoints VALUES(
:uncompressedOffset, :uncompressedEndOffset,
:compressedOffset, :bitOffset, :window, :lineNum))");
        auto addLine = db.prepare(R"(
INSERT INTO LineOffsets VALUES(:line, :offset, :length))");

        ZStream zs(ZStream::Type::ZlibOrGzip);
        uint8_t input[ChunkSize];
        uint8_t window[WindowSize];
        auto clearWindow = [&] { /*memset(window, 0, WindowSize); */};
        clearWindow();

        int ret = 0;
        Progress progress(log);
        uint64_t totalIn = 0;
        uint64_t totalOut = 0;
        uint64_t last = 0;
        bool first = true;
        bool emitInitialAccessPoint = true;
        LineFinder finder(*this);

        log.info("Indexing...");
        do {
            if (zs.stream.avail_in == 0) {
                zs.stream.avail_in = fread(input, 1, ChunkSize, from.get());
                if (emitInitialAccessPoint && zs.stream.avail_in == 0 && feof(from.get())) {
                    break;
                }
                if (ferror(from.get()))
                    throw ZlibError(Z_ERRNO);
                if (zs.stream.avail_in == 0)
                    throw ZlibError(Z_DATA_ERROR);
                zs.stream.next_in = input;
            }
            do {
                
                if (zs.stream.avail_out == 0 ) {
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
                bool needsIndex = sinceLast > indexEvery
                                  || emitInitialAccessPoint;
                bool endOfBlock = zs.stream.data_type & 0x80;
                bool lastBlockInStream = zs.stream.data_type & 0x40;
                if (endOfBlock && !lastBlockInStream && needsIndex) {
                    log.debug("Creating checkpoint at ", PrettyBytes(totalOut),
                              " (compressed offset ", PrettyBytes(totalIn),
                              ")");
                    auto currentLineNumber = finder.lineOffsets().size();
                    if (WindowSize != zs.stream.avail_out) {
                        for (unsigned int i = 0; i < WindowSize - zs.stream.avail_out; i++)
                            if (window[i] == '\n') currentLineNumber++;
                    }
                    if (totalOut != 0) {
                        // Flush previous information.
                        addIndex
                                .bindInt64(":uncompressedEndOffset",
                                           totalOut - 1)
                                .step();
                        addIndex.reset();
                    }
                    uint8_t apWindow[compressBound(WindowSize)];
                    auto size = makeWindow(apWindow, sizeof(apWindow), window,
                                           zs.stream.avail_out);
                    addIndex
                            .bindInt64(":uncompressedOffset", totalOut)
                            .bindInt64(":compressedOffset", totalIn)
                            .bindInt64(":bitOffset", zs.stream.data_type & 0x7)
                            .bindBlob(":window", apWindow, size)
                            .bindInt64(":lineNum", currentLineNumber);
                    last = totalOut;
                    emitInitialAccessPoint = false;
                }
                progress.update<PrettyBytes>(totalIn, compressedStat.st_size);
            } while (zs.stream.avail_in);
            if (ret == Z_STREAM_END &&
                (zs.stream.avail_in || !feof(from.get()))) {
                // We hit the end of the stream, but there's still more to come.
                // This is a set of concatenated gzip files, possibly a bgzip
                // file. There may yet be some trailing data after this block
                // (e.g. the gzip CRC/adler trailer), but in order to skip it,
                // and the header of the next gzip block, we force an access
                // point to be created here. That way the decoder doesn't need
                // to have to know how to skip it at the end of each gzip block,
                // and it can use RAW mode in all cases.
                ret = 0;
                zs.reset();
                emitInitialAccessPoint = true;
                clearWindow();
            }
        } while (ret != Z_STREAM_END);

        if (totalOut != 0) {
            // Flush last block.
            addIndex
                    .bindInt64(":uncompressedEndOffset", totalOut - 1)
                    .step();
        }

        log.info("Index building complete; creating line index");

        finder.add(window, WindowSize - zs.stream.avail_out, true);
        const auto &lineOffsets = finder.lineOffsets();
        for (size_t line = 0; line < lineOffsets.size() - 1; ++line) {
            addLine
                    .reset()
                    .bindInt64(":line", line + 1)
                    .bindInt64(":offset", lineOffsets[line])
                    .bindInt64(":length",
                               lineOffsets[line + 1] - lineOffsets[line])
                    .step();
            progress.update<size_t>(line, lineOffsets.size() - 1);
        }
        if (sparseLineOffsets_) {
             std::string sql("");
             if (indexers.size() > 0) {
                for (auto &&pair : indexers) {
                    if (sql.size() != 0) {
                        sql += " UNION ";
                    }
                    sql += ("select line from " + pair.first);
                }
                db.exec("DELETE FROM LineOffsets where line not in (" + sql + ")");
             }
        }
        log.info("Flushing");
        db.exec(R"(END TRANSACTION)");
        log.info("Done");
    }

    void addMeta(const std::string &key, const std::string &value) {
        log.debug("Adding metadata ", key, " = ", value);
        addMetaSql
                .reset()
                .bindString(":key", key)
                .bindString(":value", value)
                .step();
    }

    void addIndexer(const std::string &name, const std::string &creation,
                    Index::IndexConfig config,
                    std::unique_ptr<LineIndexer> indexer) {
        //these flags must be consistent across all indexes, only set to true once
        saveAllLines_ |= !config.sparse;
        sparseLineOffsets_ |= config.sparseLineOffsets;
        auto table = "index_" + name;
        std::string type = config.numeric ? "INTEGER" : "TEXT";
        if (config.unique) type += " PRIMARY KEY";
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
                .bindInt64(":isNumeric", config.numeric ? 1 : 0)
                .step();

        auto inserter = db.prepare(R"(
INSERT INTO )" + table + R"( VALUES(:key, :line, :offset)
)");
        if (config.indexLineOffsets) {
            db.exec(R"(CREATE INDEX )" + table + R"(_line_index ON )"
                    + table + R"((line))");
        }
        if (!config.unique) {
            db.exec(R"(CREATE INDEX )" + table + R"(_key_index ON )"
                    + table + R"((key))");
        }
        if (config.numeric) {
            indexers.emplace(table, std::unique_ptr<IndexHandler>(
                    new NumericHandler(log, std::move(indexer),
                                       std::move(inserter))));
        } else {
            indexers.emplace(table, std::unique_ptr<IndexHandler>(
                    new AlphaHandler(log, std::move(indexer),
                                     std::move(inserter))));
        }
    }

    bool onLine(
            size_t lineNumber,
            size_t /*fileOffset*/,
            const char *line, size_t length) override {
        if (lineNumber <= skipFirst) return true;
        bool consumed = false;
        for (auto &&pair : indexers) {
            consumed |= pair.second->onLine(lineNumber, line, length);
        }

        return consumed || saveAllLines_;
    }
};

Index::Builder::Builder(Log &log, File &&from, const std::string &fromPath,
                        const std::string &indexFilename)
        : impl_(new Impl(log, std::move(from), fromPath, indexFilename)) {
    impl_->init();
}

Index::Builder &Index::Builder::addIndexer(
        const std::string &name,
        const std::string &creation,
        IndexConfig config,
        std::unique_ptr<LineIndexer> indexer) {
    impl_->addIndexer(name, creation, config, std::move(indexer));
    return *this;
}

Index::Builder &Index::Builder::indexEvery(uint64_t bytes) {
    impl_->indexEvery = bytes;
    return *this;
}

Index::Builder &Index::Builder::skipFirst(uint64_t skipFirst) {
    impl_->skipFirst = skipFirst;
    return *this;
}

void Index::Builder::build() {
    impl_->build();
}

Index Index::load(Log &log, File &&fromCompressed,
                  const std::string &indexFilename,
                  bool forceLoad) {
    Sqlite db(log);
    db.open(indexFilename.c_str(), true);

    std::unique_ptr<Impl> impl(new Impl(log,
                                        std::move(fromCompressed),
                                        std::move(db)));
    impl->init(forceLoad);
    return Index(std::move(impl));
}

bool Index::getLine(uint64_t line, LineSink &sink) {
    return impl_->getLine(line, sink);
}

size_t Index::getLines(const std::vector<uint64_t> &lines, LineSink &sink) {
    size_t matched = 0u;
    for (auto line : lines) if (impl_->getLine(line, sink)) ++matched;
    return matched;
}

size_t Index::queryIndex(const std::string &index, const std::string &query,
                         LineFunction lineFunction) {
    return impl_->queryIndex(index, query, lineFunction);
}

size_t Index::queryIndexMulti(const std::string &index,
                              const std::vector<std::string> &queries,
                              LineFunction lineFunction) {
    size_t result = 0;
    for (auto query : queries)
        result += impl_->queryIndex(index, query, lineFunction);
    return result;
}

size_t
Index::queryCustom(const std::string &customQuery, LineFunction lineFunc) {
    return impl_->customQuery(customQuery, lineFunc);
}

Index::Builder::~Builder() {
}

size_t Index::indexSize(const std::string &index) const {
    return impl_->indexSize(index);
}

size_t Index::tableSize(const std::string &name) const {
    return impl_->tableSize(name);
}

const Index::Metadata &Index::getMetadata() const {
    return impl_->metadata_;
}

Index::LineFunction Index::sinkFetch(LineSink &sink) {
    return std::function<void(size_t)>([this, &sink](size_t line) {
        this->getLine(line, sink);
    });
}
