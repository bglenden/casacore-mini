#include "casacore_mini/version.hpp"

#include <iostream>

int main() {
    const auto version = casacore_mini::version();

    if (version.empty()) {
        std::cerr << "version() must not be empty\n";
        return 1;
    }

    if (version.find('.') == std::string_view::npos) {
        std::cerr << "version() should use dotted semver-like format\n";
        return 1;
    }

    return 0;
}
