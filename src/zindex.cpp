#include "File.h"
#include "Index.h"
#include "RegExpSink.h"

#include <tclap/CmdLine.h>

#include <iostream>

using namespace std;
using namespace TCLAP;

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Create indices in a compressed text file");
    UnlabeledValueArg<string> inputFile(
            "input-file", "Read input from <file>", true, "", "file", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg numeric("n", "numeric", "Assume the index is numeric", cmd);
    ValueArg<string> regex("", "regex", "Create an index using <regex>", true,
            "", "regex", cmd);

    cmd.parse(argc, argv);

    File in(fopen(inputFile.getValue().c_str(), "rb"));
    if (in.get() == nullptr) {
        cerr << "could not open " << inputFile.getValue() << " for reading"
                << endl;
        return 1;
    }

    std::function<std::unique_ptr<LineSink>(Sqlite &)> indexerFactory;
    if (regex.isSet()) {
        indexerFactory = [ & ](Sqlite &db) -> std::unique_ptr<LineSink> {
            return std::unique_ptr<LineSink>(new RegExpSink(db, regex.getValue()));
        };
    } else {
        // Not possible at the moment.
        cerr << "Regex must be set" << std::endl;
    }

    auto outputFile = inputFile.getValue() + ".zindex";
    Index::build(move(in), outputFile.c_str(), indexerFactory);

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
