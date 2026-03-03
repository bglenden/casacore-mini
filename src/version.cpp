// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/version.hpp"
#include "casacore_mini/version_generated.hpp"

namespace casacore_mini {

std::string_view project_version() noexcept {
    return build::kProjectVersion;
}

std::string_view build_version() noexcept {
    return build::kBuildVersion;
}

std::string_view git_describe() noexcept {
    return build::kGitDescribe;
}

std::string_view git_revision() noexcept {
    return build::kGitRevision;
}

std::string_view version() noexcept {
    return build_version();
}

} // namespace casacore_mini
