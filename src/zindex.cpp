#include "File.h"
#include "Index.h"
#include "RegExpIndexer.h"
#include "StdoutLog.h"

#include <tclap/CmdLine.h>

#include <iostream>

using namespace std;
using namespace TCLAP;

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
    ValueArg<string> regex("", "regex", "Create an index using <regex>", true,
                           "", "regex", cmd);
    cmd.parse(argc, argv);

    StdoutLog log(
            debug.isSet() ? Log::Severity::Debug : verbose.isSet()
                                                   ? Log::Severity::Info
                                                   : Log::Severity::Warning,
            forceColour.isSet() || forceColor.isSet());

    File in(fopen(inputFile.getValue().c_str(), "rb"));
    if (in.get() == nullptr) {
        cerr << "could not open " << inputFile.getValue() << " for reading"
        << endl;
        return 1;
    }

    auto outputFile = inputFile.getValue() + ".zindex";
    Index::Builder builder(log, move(in), outputFile);
    if (regex.isSet()) {
        RegExpIndexer indexer(
                regex.getValue()); // arguably we should pass uniq_ptr<> to builder?
        builder.addIndexer("default", regex.getValue(), numeric.isSet(),
                           unique.isSet(), indexer);
        builder.build();
    } else {
        // Not possible at the moment.
        cerr << "Regex must be set" << std::endl;
    }

    return 0;
}

int main(int argc, const char *argv[]) {
    try {
        return Main(argc, argv);
    } catch (const std::exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
}
