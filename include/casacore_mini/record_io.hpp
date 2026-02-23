#pragma once

#include "casacore_mini/record.hpp"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace casacore_mini {

void write_record_binary(const Record& value, std::ostream& output);
[[nodiscard]] Record read_record_binary(std::istream& input);

[[nodiscard]] std::vector<std::uint8_t> serialize_record_binary(const Record& value);
[[nodiscard]] Record deserialize_record_binary(std::span<const std::uint8_t> bytes);

} // namespace casacore_mini
