#include "casacore_mini/table_dat_writer.hpp"

#include "casacore_mini/aipsio_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {
namespace {

[[nodiscard]] std::uint32_t checked_u32(const std::uint64_t value, std::string_view field_name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t checked_u32(const std::size_t value, std::string_view field_name) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

} // namespace

void write_table_dat_header(AipsIoWriter& writer, const TableDatMetadata& metadata) {
    // Always write version 2 format (u32 row count).
    constexpr std::uint32_t kTableVersion = 2U;

    // Compute object length: we need to know the total body size.
    // Body = row_count(4) + endian_flag(4) + string_len(4) + table_type_chars.
    // Object header body = type_string_len(4) + "Table"(5) + version(4) + body.
    // Use begin/end pattern with patch_u32.
    writer.write_u32(kAipsIoMagic);
    const auto length_offset = writer.size();
    writer.write_u32(0); // placeholder for object_length
    writer.write_string("Table");
    writer.write_u32(kTableVersion);

    writer.write_u32(checked_u32(metadata.row_count, "table.dat row_count"));
    writer.write_u32(metadata.big_endian ? 0U : 1U);
    writer.write_string(metadata.table_type);

    // Patch object length.
    const auto length = writer.size() - length_offset - sizeof(std::uint32_t);
    writer.patch_u32(length_offset, checked_u32(length, "table.dat object length"));
}

std::vector<std::uint8_t> serialize_table_dat_header(const TableDatMetadata& metadata) {
    AipsIoWriter writer;
    write_table_dat_header(writer, metadata);
    return writer.take_bytes();
}

} // namespace casacore_mini
