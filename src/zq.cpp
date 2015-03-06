#include "File.h"
#include "Index.h"

#include <tclap/CmdLine.h>

#include <cstdio>
#include <iostream>

using namespace std;
namespace tc = TCLAP;

int Main(int argc, const char *argv[]) {
    tc::CmdLine cmd("Lookup indices in a compressed text file");
    tc::UnlabeledValueArg<string> inputFile(
            "input-file",  "Read input from <file>", true, "", "<file>", cmd);
    tc::UnlabeledMultiArg<uint64_t> query(
            "query",  "Query for <query>", false, "<query>", cmd);
    tc::SwitchArg verbose("v", "verbose", "Be more verbose", cmd);

    cmd.parse(argc, argv);

    auto indexFile = inputFile.getValue() + ".zindex";
    File in(fopen(indexFile.c_str(), "rb"));
    if (in.get() == nullptr) {
        cerr << "could not open " << indexFile << " for reading" << endl;
        return 1;
    }
    auto index = Index::load(move(in));

    for (auto &q: query.getValue()) {
        cout << "Index: " << q << " = " << index.lineOffset(q) << endl;
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
