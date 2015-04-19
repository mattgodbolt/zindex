#include "PrettyBytes.h"

#include <iostream>
#include <iomanip>

using namespace std;

ostream &operator<<(ostream &o, PrettyBytes pb) {
    if (pb.bytes_ == 1) return o << "1 byte";
    if (pb.bytes_ < 1024) return o << pb.bytes_ << " bytes";
    o << setprecision(2) << fixed;
    if (pb.bytes_ < 1024 * 1024)
        return o << (pb.bytes_ / (1024.0)) << " KiB";
    if (pb.bytes_ < 1024 * 1024 * 1024)
        return o << (pb.bytes_ / (1024 * 1024.0)) << " MiB";
    return o << (pb.bytes_ / (1024 * 1024 * 1024.0)) << " GiB";
}
