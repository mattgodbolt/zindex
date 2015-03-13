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

using File = std::unique_ptr<FILE, impl::Closer>;
