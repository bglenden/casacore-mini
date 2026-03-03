// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_desc_writer.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

namespace {

void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void copy_file_if_exists(const std::filesystem::path& src, const std::filesystem::path& dst) {
    if (std::filesystem::exists(src)) {
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
    }
}

} // namespace

TableDirectory read_table_directory(std::string_view dir_path) {
    const auto dir = std::filesystem::path(dir_path);
    if (!std::filesystem::is_directory(dir)) {
        throw std::runtime_error("not a directory: " + std::string(dir_path));
    }

    const auto table_dat_path = dir / "table.dat";
    if (!std::filesystem::exists(table_dat_path)) {
        throw std::runtime_error("missing table.dat in: " + std::string(dir_path));
    }

    TableDirectory result;
    result.directory = dir;
    result.table_dat = read_table_dat_full(table_dat_path.string());

    // Catalog SM data files: table.f0, table.f1, ...
    for (std::uint32_t seq = 0; seq < 100U; ++seq) {
        const auto fname = "table.f" + std::to_string(seq);
        const auto fpath = dir / fname;
        if (!std::filesystem::exists(fpath)) {
            continue;
        }
        StorageManagerFile smf;
        smf.filename = fname;
        smf.size = std::filesystem::file_size(fpath);
        smf.sequence_number = seq;

        // Match SM type from table.dat descriptors.
        for (const auto& sm : result.table_dat.storage_managers) {
            if (sm.sequence_number == seq) {
                smf.sm_type = sm.type_name;
                break;
            }
        }

        result.sm_files.push_back(std::move(smf));
    }

    return result;
}

void write_table_directory(std::string_view dir_path, const TableDatFull& table_dat,
                           std::string_view source_dir) {
    const auto dir = std::filesystem::path(dir_path);
    std::filesystem::create_directories(dir);

    // Write table.dat.
    write_file(dir / "table.dat", serialize_table_dat_full(table_dat));

    // Copy SM data files from source directory if provided.
    if (!source_dir.empty()) {
        const auto src = std::filesystem::path(source_dir);
        for (std::uint32_t seq = 0; seq < 100U; ++seq) {
            const auto fname = "table.f" + std::to_string(seq);
            copy_file_if_exists(src / fname, dir / fname);
        }
        // Also copy auxiliary files.
        copy_file_if_exists(src / "table.info", dir / "table.info");
    }
}

std::string compare_table_directories(const TableDirectory& lhs, const TableDirectory& rhs) {
    // Compare table version.
    if (lhs.table_dat.table_version != rhs.table_dat.table_version) {
        return "table_version differs: " + std::to_string(lhs.table_dat.table_version) + " vs " +
               std::to_string(rhs.table_dat.table_version);
    }

    // Compare row count.
    if (lhs.table_dat.row_count != rhs.table_dat.row_count) {
        return "row_count differs: " + std::to_string(lhs.table_dat.row_count) + " vs " +
               std::to_string(rhs.table_dat.row_count);
    }

    // Compare column count.
    const auto& lhs_cols = lhs.table_dat.table_desc.columns;
    const auto& rhs_cols = rhs.table_dat.table_desc.columns;
    if (lhs_cols.size() != rhs_cols.size()) {
        return "column count differs: " + std::to_string(lhs_cols.size()) + " vs " +
               std::to_string(rhs_cols.size());
    }

    // Compare columns.
    for (std::size_t i = 0; i < lhs_cols.size(); ++i) {
        if (lhs_cols[i].name != rhs_cols[i].name) {
            return "column " + std::to_string(i) + " name differs: " + lhs_cols[i].name + " vs " +
                   rhs_cols[i].name;
        }
        if (lhs_cols[i].data_type != rhs_cols[i].data_type) {
            return "column " + std::to_string(i) + " dtype differs";
        }
    }

    // Compare SM file counts.
    if (lhs.sm_files.size() != rhs.sm_files.size()) {
        return "SM file count differs: " + std::to_string(lhs.sm_files.size()) + " vs " +
               std::to_string(rhs.sm_files.size());
    }

    return {};
}

} // namespace casacore_mini
