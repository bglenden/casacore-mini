#include "casacore_mini/version.hpp"

#include <iostream>

int main() {
    const auto project_version = casacore_mini::project_version();
    const auto build_version = casacore_mini::build_version();
    const auto describe = casacore_mini::git_describe();
    const auto revision = casacore_mini::git_revision();
    const auto version = casacore_mini::version();

    if (project_version.empty()) {
        std::cerr << "project_version() must not be empty\n";
        return 1;
    }

    if (project_version.find('.') == std::string_view::npos) {
        std::cerr << "project_version() should use dotted semver-like format\n";
        return 1;
    }

    if (build_version.empty()) {
        std::cerr << "build_version() must not be empty\n";
        return 1;
    }

    if (build_version.find(project_version) != 0) {
        std::cerr << "build_version() should begin with project_version()\n";
        return 1;
    }

    if (describe.empty() || revision.empty()) {
        std::cerr << "git metadata values must not be empty\n";
        return 1;
    }

    if (version != build_version) {
        std::cerr << "version() must equal build_version()\n";
        return 1;
    }

    return 0;
}
