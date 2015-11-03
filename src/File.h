#pragma once

#include <cstdio>
#include <memory>

namespace impl {
struct Closer {
    void operator()(FILE *f) {
        if (f) ::fclose(f);
    }
};
}

// A File is a self-closing FILE *.
using File = std::unique_ptr<FILE, impl::Closer>;
