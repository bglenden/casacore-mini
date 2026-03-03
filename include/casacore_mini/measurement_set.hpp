// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
/// @ingroup ms
/// @addtogroup ms
/// @{

///
/// High-level interface to a CASA MeasurementSet (MS) table directory.
///
///
///
///
/// @par Prerequisites
///   - Table — the underlying table access layer
///   - MSEnums — column and subtable name constants
///
///
///
/// A MeasurementSet is the standard data format for radio interferometry
/// visibility data.  It is a casacore table directory whose main table
/// stores one visibility sample per row, and whose 12 required subtable
/// directories hold instrumental and observational metadata.
///
/// The required subtables are:
///
///   - ANTENNA         — antenna positions and properties
///   - DATA_DESCRIPTION — (SPW, polarisation) pairs
///   - FEED            — feed properties per antenna
///   - FIELD           — source pointing directions
///   - FLAG_CMD        — flagging history
///   - HISTORY         — processing log
///   - OBSERVATION     — observation metadata
///   - POINTING        — time-varying antenna pointings
///   - POLARIZATION    — Stokes product definitions
///   - PROCESSOR       — correlator descriptions
///   - SPECTRAL_WINDOW — frequency axis definitions
///   - STATE           — correlator state (on-source, reference, etc.)
///
///
/// Five optional subtables may also be present: DOPPLER, FREQ_OFFSET, SOURCE,
/// SYSCAL, and WEATHER.
///
/// The main table columns include TIME, ANTENNA1, ANTENNA2, DATA (complex
/// visibilities), FLAG, UVW, and the optional WEIGHT and SIGMA columns.
///
/// Subtables are opened lazily: the first call to `subtable(name)`
/// opens the directory from disk and caches it. Subsequent calls return the
/// cached Table.
///
///
/// @par Example
/// Create a new MeasurementSet and populate its ANTENNA subtable:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::create("my.ms");
///
///   auto& ant = ms.subtable("ANTENNA");
///   ant.add_row(2);
///   ant.write_scalar_cell("NAME", 0, CellValue(std::string("ANT1")));
///   ant.write_scalar_cell("NAME", 1, CellValue(std::string("ANT2")));
///   ms.flush();
/// @endcode
///
///
/// @par Example
/// Open an existing MS and query its row count:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   std::cout << "rows: " << ms.row_count() << "\n";
///   for (auto& name : ms.subtable_names())
///       std::cout << "  " << name << "\n";
/// @endcode
///
///
/// @par Motivation
/// The MeasurementSet class provides a clean lifecycle wrapper around
/// the table directory tree, ensuring all required subtables are created
/// on construction and lazily opened on first access.  Ownership of the
/// root path and all open Table objects lives here so that callers never
/// need to manage subdirectory paths directly.
///
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
    [[nodiscard]] const Table& main_table() const noexcept {
        return main_table_;
    }

    /// Access the main table (mutable).
    [[nodiscard]] Table& main_table() noexcept {
        return main_table_;
    }

    /// Access a subtable by name (lazy-open from disk).
    ///
    /// @param name  Subtable name (e.g. "ANTENNA", "SPECTRAL_WINDOW").
    /// @throws std::runtime_error if subtable does not exist.
    [[nodiscard]] const Table& subtable(std::string_view name) const;

    /// Access a subtable by name (mutable, lazy-open).
    [[nodiscard]] Table& subtable(std::string_view name);

    /// Main table row count.
    [[nodiscard]] std::uint64_t row_count() const noexcept {
        return main_table_.nrow();
    }

    /// List names of all subtables present on disk.
    [[nodiscard]] std::vector<std::string> subtable_names() const;

    /// Check if a subtable exists on disk.
    [[nodiscard]] bool has_subtable(std::string_view name) const;

    /// Root path of the MeasurementSet.
    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return root_path_;
    }

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

/// @}
} // namespace casacore_mini
