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
    Index::build(move(in), outputFile.c_str());

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
