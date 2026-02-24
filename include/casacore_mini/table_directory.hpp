#pragma once

#include "casacore_mini/table_desc.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Table directory reader: orchestrates table.dat + data files.

/// Metadata about a storage manager data file within a table directory.
struct StorageManagerFile {
    /// File name (e.g. "table.f0").
    std::string filename;
    /// File size in bytes.
    std::uint64_t size = 0;
    /// Storage manager type (from table.dat SM descriptor).
    std::string sm_type;
    /// Storage manager sequence number.
    std::uint32_t sequence_number = 0;
};

/// Represents a table directory on disk.
///
/// A casacore table directory contains:
/// - `table.dat`: metadata (table header + TableDesc + SM descriptors)
/// - `table.f<N>`: per-SM data files (StandardStMan, IncrementalStMan, etc.)
/// - `table.lock`: lock file (ignored for read-only)
/// - `table.info`: info text (ignored for read-only)
struct TableDirectory {
    /// Path to the table directory.
    std::filesystem::path directory;
    /// Parsed table.dat metadata.
    TableDatFull table_dat;
    /// Storage manager data files present.
    std::vector<StorageManagerFile> sm_files;
};

/// Read a table directory: parses table.dat and catalogs SM data files.
///
/// @param dir_path Path to the table directory.
/// @throws std::runtime_error if table.dat is missing or unparseable.
[[nodiscard]] TableDirectory read_table_directory(std::string_view dir_path);

/// Write a table directory from a TableDatFull and optional data file copies.
///
/// Creates the directory, writes table.dat, and copies any provided SM data
/// files. This is a metadata-level write; cell-level data writing requires
/// storage manager implementations.
///
/// @param dir_path Path to create the table directory at.
/// @param table_dat The full table.dat metadata to write.
/// @param source_dir Optional source directory to copy SM data files from.
void write_table_directory(std::string_view dir_path, const TableDatFull& table_dat,
                           std::string_view source_dir = "");

/// Compare two table directories for structural equivalence.
///
/// Checks table.dat metadata and SM file presence/sizes.
/// Does NOT compare SM data file contents byte-for-byte (use interop verify for that).
///
/// @return Empty string if equivalent, otherwise a description of the first difference.
[[nodiscard]] std::string compare_table_directories(const TableDirectory& lhs,
                                                    const TableDirectory& rhs);

} // namespace casacore_mini
