// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_dat.hpp"

#include "casacore_mini/aipsio_reader.hpp"

#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

namespace casacore_mini {

TableDatMetadata parse_table_dat_metadata(const std::span<const std::uint8_t> bytes) {
    AipsIoReader reader(bytes);
    const auto header = reader.read_object_header();
    if (header.object_type != "Table") {
        throw std::runtime_error("table.dat root object is not Table");
    }

    TableDatMetadata metadata;
    metadata.table_version = header.object_version;

    if (metadata.table_version <= 2U) {
        metadata.row_count = reader.read_u32();
    } else {
        metadata.row_count = reader.read_u64();
    }

    const auto endian_flag = reader.read_u32();
    if (endian_flag > 1U) {
        throw std::runtime_error("table.dat endian flag is not 0/1");
    }
    metadata.big_endian = endian_flag == 0U;
    metadata.table_type = reader.read_string();
    return metadata;
}

TableDatMetadata read_table_dat_metadata(const std::string_view table_dat_path) {
    std::ifstream input(std::string(table_dat_path), std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open table.dat file: " + std::string(table_dat_path));
    }

    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),
                                    std::istreambuf_iterator<char>());
    return parse_table_dat_metadata(bytes);
}

} // namespace casacore_mini
