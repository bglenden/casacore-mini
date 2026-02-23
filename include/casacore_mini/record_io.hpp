#pragma once

#include "casacore_mini/platform.hpp"
#include "casacore_mini/record.hpp"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace casacore_mini {

/// Write a `Record` using the project-local deterministic binary encoding.
///
/// This format is internal scaffolding for current phases and is not yet
/// casacore `AipsIO` wire-compatible.
void write_record_binary(const Record& value, std::ostream& output);

/// Read one `Record` from the project-local deterministic binary encoding.
[[nodiscard]] Record read_record_binary(std::istream& input);

/// Serialize a record to a byte vector using `write_record_binary`.
[[nodiscard]] std::vector<std::uint8_t> serialize_record_binary(const Record& value);

/// Deserialize bytes produced by `serialize_record_binary`.
[[nodiscard]] Record deserialize_record_binary(std::span<const std::uint8_t> bytes);

} // namespace casacore_mini
