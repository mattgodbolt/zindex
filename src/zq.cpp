#include "File.h"
#include "Index.h"
#include "LineSink.h"
#include "ConsoleLog.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <stdexcept>
#include <utility>
#include "RangeFetcher.h"

using namespace std;
using namespace TCLAP;

namespace {

struct PrintSink : LineSink {
    bool printLineNum;

    explicit PrintSink(bool printLineNum) : printLineNum(printLineNum) { }

    bool onLine(size_t l, size_t, const char *line, size_t length) override {
        if (printLineNum) cout << l << ":";
        cout << string(line, length) << endl;
        return true;
    }
};

struct PrintHandler : RangeFetcher::Handler {
    Index &index;
    LineSink &sink;
    const bool printSep;
    const std::string sep;

    PrintHandler(Index &index, LineSink &sink, bool printSep,
                 std::string sep)
            : index(index), sink(sink), printSep(printSep),
              sep(std::move(sep)) { }

    void onLine(uint64_t line) override {
        index.getLine(line, sink);
    }

    void onSeparator() override {
        if (printSep)
            cout << sep << endl;
    }
};

uint64_t toInt(const string &s) {
    char *endP;
    auto res = strtoull(&s[0], &endP, 10);
    if (*endP != '\0') throw runtime_error("Non-numeric value: '" + s + "'");
    return res;
}

}

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Lookup indices in a compressed text file");
    UnlabeledValueArg<string> inputFile(
            "input-file", "Read input from <file>", true, "", "file", cmd);
    UnlabeledMultiArg<string> query(
            "query", "Query for <query>", false, "<query>", cmd);
    SwitchArg lineMode("l", "line",
                       "Treat query as a series of line numbers to print", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg debug("", "debug", "Be even more verbose", cmd);
    SwitchArg forceColour("", "colour", "Use colour even on non-TTY", cmd);
    SwitchArg forceColor("", "color", "Use color even on non-TTY", cmd);
    SwitchArg forceLoad("", "force", "Load index even if it appears "
            "inconsistent with the index", cmd);
    SwitchArg lineNum("n", "line-number",
                      "Prefix each line of output with its line number", cmd);
    SwitchArg warnings("w", "warnings", "Log warnings at info level", cmd);
    ValueArg<uint64_t> afterArg("A", "after-context",
                                "Print NUM lines of context after each match",
                                false, 0, "NUM", cmd);
    ValueArg<uint64_t> beforeArg("B", "before-context",
                                 "Print NUM lines of context before each match",
                                 false, 0, "NUM", cmd);
    ValueArg<uint64_t> contextArg("C", "context",
                                  "Print NUM lines of context around each match",
                                  false, 0, "NUM", cmd);
    SwitchArg noSepArg("", "no-separator",
                       "Don't print a separator between non-overlapping contexts",
                       cmd);
    ValueArg<string> sepArg("S", "separator",
                            "Print SEPARATOR between non-overlapping contexts "
                                    "(if -A, -B or -C specified)",
                            false, "--", "SEPARATOR", cmd);
    ValueArg<string> indexArg("", "index-file", "Use index from <index-file> "
            "(default <file>.zindex)", false, "", "index", cmd);

    ValueArg<string> queryIndexArg("i", "index", "Use specified index for searching"
                            , false, "", "index", cmd);

    ValueArg<string> rawSqlQueryArg("", "raw", "Expert Mode - Since zindex is "
            "a sqlite3 database under the covers, this flag lets you run a custom "
            "query for use cases not supported by command line args."
            " example: (--raw \"select a.line from index_default a, "
            "index_secondary b where a.line == b.line and a.key == '2' "
            "and b.key == 'KEY_2';\")"
                            , false, "", "raw", cmd);

    cmd.parse(argc, argv);

    ConsoleLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet(), warnings.isSet());

    try {
        auto compressedFile = inputFile.getValue();
        File in(fopen(compressedFile.c_str(), "rb"));
        if (in.get() == nullptr) {
            log.error("Could not open ", compressedFile, " for reading");
            return 1;
        }

        auto indexFile = indexArg.isSet() ? indexArg.getValue() :
                         inputFile.getValue() + ".zindex";
        auto index = Index::load(log, move(in), indexFile.c_str(),
                                 forceLoad.isSet());
        auto queryIndex = queryIndexArg.isSet() ? queryIndexArg.getValue() : "default";

        uint64_t before = 0u;
        uint64_t after = 0u;
        if (beforeArg.isSet()) before = beforeArg.getValue();
        if (afterArg.isSet()) after = afterArg.getValue();
        if (contextArg.isSet()) before = after = contextArg.getValue();
        log.debug("Fetching context of ", before, " lines before and ", after,
                  " lines after");
        PrintSink sink(lineNum.isSet());
        PrintHandler ph(index, sink, (before || after) && !noSepArg.isSet(),
                        sepArg.getValue());
        RangeFetcher rangeFetcher(ph, before, after);
        if (lineMode.isSet()) {
            for (auto &q : query.getValue())
                rangeFetcher(toInt(q));
        } else if (rawSqlQueryArg.isSet()) {
            index.queryCustom(rawSqlQueryArg.getValue(), rangeFetcher);
        } else {
            index.queryIndexMulti(queryIndex, query.getValue(), rangeFetcher);
        }
    } catch (const exception &e) {
        log.error(e.what());
        return 1;
    }

    return 0;
}

int main(int argc, const char *argv[]) {
    try {
        return Main(argc, argv);
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
}
