#pragma once

#include <string_view>

namespace casacore_mini {

/// Canonical source version from `project(... VERSION X.Y.Z)`.
[[nodiscard]] std::string_view project_version() noexcept;

/// Build version with git-derived metadata when not on an exact release tag.
[[nodiscard]] std::string_view build_version() noexcept;

/// Raw `git describe` string captured at configure time.
[[nodiscard]] std::string_view git_describe() noexcept;

/// Short git revision captured at configure time.
[[nodiscard]] std::string_view git_revision() noexcept;

/// Alias for `build_version()`.
[[nodiscard]] std::string_view version() noexcept;

} // namespace casacore_mini
