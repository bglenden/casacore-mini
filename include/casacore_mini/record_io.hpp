// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"
#include "casacore_mini/record.hpp"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Deterministic binary serialization for `Record`.

/// Write a `Record` using the project-local deterministic binary encoding.
///
///
/// This format is internal scaffolding for current phases and is **not**
/// casacore `AipsIO` wire-compatible.  It is used where a stable, round-
/// trippable binary encoding is needed independently of the casacore wire
/// format.
///
/// Wire format layout:
/// - magic bytes: `CCMR` (4 bytes)
/// - format version: `1` (4-byte little-endian `uint32`)
/// - payload: recursive tagged value tree
///
/// Encoding details:
/// - Integer and floating-point payloads are encoded little-endian.
/// - Arrays encode rank, shape (one `int64` per dimension), and flattened
///   Fortran-order elements.
/// - Nested list/record depth is bounded to prevent stack overflow on
///   pathologically deep inputs.
///
/// The four free functions `write_record_binary`, `read_record_binary`,
/// `serialize_record_binary`, and `deserialize_record_binary` cover both
/// stream-based and byte-span based access patterns.
///
///
/// @par Example
/// @code{.cpp}
///   Record rec;
///   rec.fields["PI"] = RecordValue(3.14159);
///   auto bytes = serialize_record_binary(rec);
///   Record copy = deserialize_record_binary(bytes);
/// @endcode
///
///
/// @throws std::runtime_error on stream write errors, unsupported/invalid value
/// state, shape/count mismatches, or depth-limit violations.
void write_record_binary(const Record& value, std::ostream& output);

/// Read one `Record` from the project-local deterministic binary encoding.
///
/// @throws std::runtime_error on invalid magic/version, malformed/truncated
/// payload, shape/count mismatches, unsupported value tags, or depth-limit
/// violations.
[[nodiscard]] Record read_record_binary(std::istream& input);

/// Serialize a record to a byte vector using `write_record_binary`.
///
/// @throws std::runtime_error with the same conditions as
/// `write_record_binary`.
[[nodiscard]] std::vector<std::uint8_t> serialize_record_binary(const Record& value);

/// Deserialize bytes produced by `serialize_record_binary`.
///
/// @throws std::runtime_error with the same conditions as
/// `read_record_binary`, and also when trailing bytes remain after one record
/// is decoded.
[[nodiscard]] Record deserialize_record_binary(std::span<const std::uint8_t> bytes);

} // namespace casacore_mini
