// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <bit>

namespace casacore_mini {

/// @file
/// @brief Compile-time platform assumptions used by `casacore-mini`.
/// @ingroup platform
/// @addtogroup platform
/// @{

///
/// This header documents and enforces the host-platform constraints that
/// casacore-mini currently requires.
///
/// `kHostLittleEndian` is a compile-time constant that is `true` when the
/// host byte order is little-endian (as determined by `std::endian::native`).
///
/// A `static_assert` hard-fails the build on big-endian hosts because the
/// byte-swap logic in `AipsIoReader` and `AipsIoWriter` is written to convert
/// from big-endian wire format to little-endian native order and back.
/// Supporting big-endian hosts would require inverting or conditionalising
/// those conversions.
///
/// All other headers that depend on host byte order include this header first.
///

/// True when host byte order is little-endian.
inline constexpr bool kHostLittleEndian = std::endian::native == std::endian::little;

static_assert(kHostLittleEndian,
              "casacore-mini currently supports only little-endian host systems; "
              "big-endian host support requires dedicated development.");

/// @}
} // namespace casacore_mini
