#include "File.h"
#include "Index.h"

#include <tclap/CmdLine.h>

#include <cstdio>
#include <iostream>

using namespace std;
using namespace TCLAP;

int Main(int argc, const char *argv[]) {
    CmdLine cmd("Create indices in a compressed text file");
    UnlabeledValueArg<string> inputFile(
            "input-file", "Read input from <file>", true, "", "<file>", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);

    cmd.parse(argc, argv);

    File in(fopen(inputFile.getValue().c_str(), "rb"));
    if (in.get() == nullptr) {
        cerr << "could not open " << inputFile.getValue() << " for reading"
                << endl;
        return 1;
    }

    auto outputFile = inputFile.getValue() + ".zindex";
    File out(fopen(outputFile.c_str(), "wb"));
    if (out.get() == nullptr) {
        cerr << "could not open " << outputFile << " for reading" << endl;
        return 1;
    }
    Index::build(move(in), move(out));

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
