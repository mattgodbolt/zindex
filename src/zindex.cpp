#include "File.h"
#include "Index.h"
#include "RegExpIndexer.h"
#include "ConsoleLog.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <limits.h>

using namespace std;
using namespace TCLAP;

namespace {

string getRealPath(const string &relPath) {
    char realPathBuf[PATH_MAX];
    auto result = realpath(relPath.c_str(), realPathBuf);
    if (result == nullptr) return relPath;
    return string(relPath);
}

}

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Create indices in a compressed text file");
    UnlabeledValueArg<string> inputFile(
            "input-file", "Read input from <file>", true, "", "file", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg debug("", "debug", "Be even more verbose", cmd);
    SwitchArg forceColour("", "colour", "Use colour even on non-TTY", cmd);
    SwitchArg forceColor("", "color", "Use color even on non-TTY", cmd);
    SwitchArg numeric("n", "numeric", "Assume the index is numeric", cmd);
    SwitchArg unique("u", "unique", "Assume each line's index is unique", cmd);
    ValueArg<uint64_t> checkpointEvery(
            "", "checkpoint-every",
            "Create an compression checkpoint every <bytes>", false,
            0, "bytes", cmd);
    ValueArg<string> regex("", "regex", "Create an index using <regex>", true,
                           "", "regex", cmd);
    ValueArg<string> indexFilename("", "index-file",
                                   "Store index in <index-file> "
                                           "(default <file>.zindex)", false, "",
                                   "index-file", cmd);
    cmd.parse(argc, argv);

    ConsoleLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet());

    try {
        auto realPath = getRealPath(inputFile.getValue());
        File in(fopen(realPath.c_str(), "rb"));
        if (in.get() == nullptr) {
            log.debug("Unable to open ", inputFile.getValue(), " (as ",
                      realPath,
                      ")");
            log.error("Could not open ", inputFile.getValue(), " for reading");
            return 1;
        }

        auto outputFile = indexFilename.isSet() ? indexFilename.getValue() :
                          inputFile.getValue() + ".zindex";
        Index::Builder builder(log, move(in), realPath, outputFile);
        if (regex.isSet()) {
            RegExpIndexer indexer(
                    regex.getValue()); // arguably we should pass uniq_ptr<> to builder?
            builder.addIndexer("default", regex.getValue(), numeric.isSet(),
                               unique.isSet(), indexer);
            if (checkpointEvery.isSet())
                builder.indexEvery(checkpointEvery.getValue());
            builder.build();
        } else {
            // Not possible at the moment.
            log.error("Regex must be set");
        }
    } catch (const exception &e) {
        log.error(e.what());
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
