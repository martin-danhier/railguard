#pragma once

#include <cstddef>

namespace rg {
    void *load_binary_file(const char *path, size_t *size);
}