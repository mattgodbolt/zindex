#include "File.h"
#include "Index.h"
#include "LineSink.h"

#include <tclap/CmdLine.h>

#include <iostream>
#include <stdexcept>

using namespace std;
using namespace TCLAP;

namespace {

struct PrintSink : LineSink {
    bool printLineNum;
    PrintSink(bool printLineNum) : printLineNum(printLineNum) {}
    void onLine(size_t l, size_t, const char *line, size_t length) override {
        if (printLineNum) cout << l << ":";
        cout << string(line, length) << endl;
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
    SwitchArg lineMode("l", "line", "Treat query as a series of line numbers to print", cmd);
    SwitchArg verbose("v", "verbose", "Be more verbose", cmd);
    SwitchArg lineNum("n", "line-number", "Prefix each line of output with its line number");

    cmd.parse(argc, argv);

    auto compressedFile = inputFile.getValue();
    File in(fopen(compressedFile.c_str(), "rb"));
    if (in.get() == nullptr) {
        cerr << "could not open " << compressedFile << " for reading" << endl;
        return 1;
    }

    auto indexFile = inputFile.getValue() + ".zindex";
    auto index = Index::load(move(in), indexFile.c_str());

    PrintSink sink(lineNum.isSet());
    if (lineMode.isSet()) {
        vector<uint64_t> lines;
        for (auto &q : query.getValue())
            lines.emplace_back(toInt(q));
        index.getLines(lines, sink);
    } else {
        index.queryIndexMulti("default", query.getValue(), sink);
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
