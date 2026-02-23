#pragma once

#include <bit>

namespace casacore_mini {

inline constexpr bool kHostLittleEndian = std::endian::native == std::endian::little;

static_assert(kHostLittleEndian,
              "casacore-mini currently supports only little-endian host systems; "
              "big-endian host support requires dedicated development.");

} // namespace casacore_mini
