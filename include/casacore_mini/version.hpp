// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Build and source version/provenance accessors.
/// @ingroup platform
/// @addtogroup platform
/// @{

///
/// This header exposes version strings captured at CMake configure time.  All
/// functions return `string_view` values pointing to static storage; callers
/// must not store the pointer beyond the lifetime of the library.
///
/// Version strings are populated by the CMake `configure_file` step from
/// `version.cpp.in`.  They reflect the state of the source tree at
/// configuration time.
///
/// - `project_version()` — semantic version from the `project(VERSION ...)` call.
/// - `build_version()` — semantic version with git-derived pre-release or
///   build-metadata suffix when the build is not on an exact release tag.
/// - `git_describe()` — raw `git describe --tags --dirty` output.
/// - `git_revision()` — short git commit hash.
/// - `version()` — alias for `build_version()`.
///
///
/// @par Example
/// @code{.cpp}
///   std::cout << "casacore-mini " << casacore_mini::version() << "\n";
/// @endcode
///

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

/// @}
} // namespace casacore_mini
