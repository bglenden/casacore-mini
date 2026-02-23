#pragma once

#include <string_view>

namespace casacore_mini {

[[nodiscard]] std::string_view project_version() noexcept;
[[nodiscard]] std::string_view build_version() noexcept;
[[nodiscard]] std::string_view git_describe() noexcept;
[[nodiscard]] std::string_view git_revision() noexcept;
[[nodiscard]] std::string_view version() noexcept;

} // namespace casacore_mini
