#include "File.h"
#include "Index.h"
#include "RegExpIndexer.h"
#include "ConsoleLog.h"
#include "FieldIndexer.h"
#include "IndexParser.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <stdexcept>
#include <limits.h>
#include "ExternalIndexer.h"

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
    SwitchArg sparse("s", "sparse", "Sparse - only save line offsets for rows"
                             " that have ids. Merges all rows that have no id to the most recent"
                             "id. Useful if your file is one id row followed by n data rows.",
                     cmd);
    SwitchArg warnings("w", "warnings", "Log warnings at info level", cmd);
    ValueArg<uint64_t> checkpointEvery(
            "", "checkpoint-every",
            "Create a compression checkpoint every <bytes>", false,
            0, "bytes", cmd);
    ValueArg<string> regex("", "regex", "Create an index using <regex>", false,
                           "", "regex", cmd);
    ValueArg<uint> capture("", "capture",
                           "Determines which capture group in an regex to use",
                           false, 0, "capture", cmd);
    ValueArg<uint64_t> skipFirst("", "skip-first", "Skip the first <num> lines",
                                 false, 0, "num", cmd);
    ValueArg<int> field("f", "field", "Create an index using field <num> "
                                "(delimited by -d/--delimiter, 1-based)",
                        false, 0, "num", cmd);
    ValueArg<string> configFile("c", "config", "Create indexes using json "
            "config file <file>", false, "", "indexes", cmd);
    ValueArg<string> delimiterArg(
            "d", "delimiter", "Use <delim> as the field delimiter", false, " ",
            "delim", cmd);
    SwitchArg tabDelimiterArg(
            "", "tab-delimiter", "Use a tab character as the field delimiter",
            cmd);
    ValueArg<string> externalIndexer(
            "p", "pipe",
            "Create indices by piping output through <CMD> which should output "
                    "a single line for each input line. "
                    "Multiple keys should be separated by the --delimiter "
                    "character (which defaults to a space). "
                    "The CMD should be unbuffered "
                    "(man stdbuf(1) for one way of doing this).\n"
                    "Example:  --pipe 'jq --raw-output --unbuffered .eventId')",
            false, "", "CMD", cmd);
    ValueArg<string> indexFilename("", "index-file",
                                   "Store index in <index-file> "
                                           "(default <file>.zindex)", false, "",
                                   "index-file", cmd);
    cmd.parse(argc, argv);

    ConsoleLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet(), warnings.isSet());

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
        if (skipFirst.isSet())
            builder.skipFirst(skipFirst.getValue());

        Index::IndexConfig config{};
        config.numeric = numeric.isSet();
        config.unique = unique.isSet();
        config.sparse = sparse.isSet();
        //config.indexLineOffsets = // TODO - add command line flag if desired

        auto delimiter = delimiterArg.getValue();
        if (tabDelimiterArg.isSet() && delimiterArg.isSet()) {
            log.error("Cannot set both --delimiter and --tab-delimiter");
            return 1;
        }
        if (tabDelimiterArg.isSet())
            delimiter = "\t";

        if (configFile.isSet()) {
            auto indexParser = IndexParser(configFile.getValue());
            indexParser.buildIndexes(&builder, log);
        } else {
            if (regex.isSet() && field.isSet()) {
                throw std::runtime_error(
                        "Sorry; multiple indices must be defined by an "
                                "indexes file - see '-i' option");
            }
            if (regex.isSet()) {
                auto regexIndexer = new RegExpIndexer(regex.getValue(),
                                                      capture.getValue());
                builder.addIndexer("default", regex.getValue(), config,
                                   std::unique_ptr<LineIndexer>(regexIndexer));
            }
            if (field.isSet()) {
                ostringstream name;
                name << "Field " << field.getValue() << " delimited by '"
                     << delimiter << "'";
                builder.addIndexer("default", name.str(), config,
                                   std::unique_ptr<LineIndexer>(
                                           new FieldIndexer(
                                                   delimiter,
                                                   field.getValue())));
            }
            if (externalIndexer.isSet()) {
                auto indexer = std::unique_ptr<LineIndexer>(
                        new ExternalIndexer(log,
                                            externalIndexer.getValue(),
                                            delimiter));
                builder.addIndexer("default", externalIndexer.getValue(),
                                   config, std::move(indexer));
            }
        }
        if (checkpointEvery.isSet())
            builder.indexEvery(checkpointEvery.getValue());
        builder.build();
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
