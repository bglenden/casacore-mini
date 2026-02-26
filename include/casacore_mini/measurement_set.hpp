#pragma once

#include "casacore_mini/ms_enums.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_desc.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MeasurementSet: the high-level radio astronomy data format.
///
/// Wraps a table directory tree (main table + subtable directories) and
/// provides lifecycle management, subtable access, and schema introspection.

/// MeasurementSet implementation using composition over Table.
///
/// A MeasurementSet is a casacore table directory with:
/// - A main table whose `table.info` type is "Measurement Set"
/// - Table-level keywords referencing subtable directories
/// - 12 required subtable directories (ANTENNA, DATA_DESCRIPTION, FEED, FIELD,
///   FLAG_CMD, HISTORY, OBSERVATION, POINTING, POLARIZATION, PROCESSOR,
///   SPECTRAL_WINDOW, STATE) plus 5 optional (DOPPLER, FREQ_OFFSET, SOURCE,
///   SYSCAL, WEATHER)
class MeasurementSet {
  public:
    /// Create a new empty MeasurementSet at the given path.
    ///
    /// Creates the root directory, writes the main table with 21 required
    /// columns and 0 rows, creates all 12 required subtable directories
    /// with correct schemas, and wires subtable keyword references.
    ///
    /// @param path  Directory path for the new MS.
    /// @param include_data  If true, include the DATA column (complex visibility).
    /// @throws std::runtime_error if path already exists or write fails.
    [[nodiscard]] static MeasurementSet create(const std::filesystem::path& path,
                                                bool include_data = true);

    /// Open an existing MeasurementSet from disk.
    ///
    /// @param path  Directory path to the existing MS.
    /// @throws std::runtime_error if path is not a valid MS directory.
    [[nodiscard]] static MeasurementSet open(const std::filesystem::path& path);

    /// Access the main table.
    [[nodiscard]] const Table& main_table() const noexcept { return main_table_; }

    /// Access the main table (mutable).
    [[nodiscard]] Table& main_table() noexcept { return main_table_; }

    /// Access a subtable by name (lazy-open from disk).
    ///
    /// @param name  Subtable name (e.g. "ANTENNA", "SPECTRAL_WINDOW").
    /// @throws std::runtime_error if subtable does not exist.
    [[nodiscard]] const Table& subtable(std::string_view name) const;

    /// Access a subtable by name (mutable, lazy-open).
    [[nodiscard]] Table& subtable(std::string_view name);

    /// Main table row count.
    [[nodiscard]] std::uint64_t row_count() const noexcept { return main_table_.nrow(); }

    /// List names of all subtables present on disk.
    [[nodiscard]] std::vector<std::string> subtable_names() const;

    /// Check if a subtable exists on disk.
    [[nodiscard]] bool has_subtable(std::string_view name) const;

    /// Root path of the MeasurementSet.
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return root_path_; }

    /// Flush pending writes (re-writes table.dat for main table).
    void flush();

  private:
    MeasurementSet() = default;

    std::filesystem::path root_path_;
    Table main_table_;
    mutable std::map<std::string, Table> subtables_;
};

/// Build a TableDatFull for a new empty MS main table.
///
/// @param include_data  Whether to include the DATA column.
/// @return Ready-to-write TableDatFull with 0 rows.
[[nodiscard]] TableDatFull make_ms_main_table_dat(bool include_data);

/// Build a minimal TableDatFull for a subtable with the given columns.
///
/// @param columns  Column descriptors for the subtable.
/// @return Ready-to-write TableDatFull with 0 rows.
[[nodiscard]] TableDatFull make_subtable_dat(const std::vector<ColumnDesc>& columns);

/// Write a table.info file.
///
/// @param dir  Table directory path.
/// @param type_string  The "Type = ..." value (e.g. "Measurement Set").
/// @param sub_type  The "SubType = ..." value (usually empty).
void write_table_info(const std::filesystem::path& dir, std::string_view type_string,
                      std::string_view sub_type = "");

/// Read the type string from a table.info file.
///
/// @param dir  Table directory path.
/// @return  The type string, or empty if file is missing.
[[nodiscard]] std::string read_table_info_type(const std::filesystem::path& dir);

} // namespace casacore_mini
