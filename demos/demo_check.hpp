#pragma once

#include <stdexcept>
#include <string>

namespace casacore_mini::demo_check {

inline void require(bool condition, const char* expression, const char* file, int line) {
    if (condition) {
        return;
    }
    throw std::runtime_error(std::string("demo check failed at ") + file + ":" +
                             std::to_string(line) + " (" + expression + ")");
}

} // namespace casacore_mini::demo_check

#define DEMO_CHECK(expr) ::casacore_mini::demo_check::require((expr), #expr, __FILE__, __LINE__)
