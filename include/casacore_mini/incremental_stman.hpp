// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_desc.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Read/write access to IncrementalStMan (ISM) data files.
/// @ingroup tables
/// @addtogroup tables
/// @{

///
/// Read-only ISM reader for a single table directory.
///
///
///
///
///
/// The IncrementalStMan (ISM) stores values incrementally: a cell value is
/// only written when it differs from the previous row.  For columns where many
/// consecutive rows share the same value (e.g. `FIELD_ID` in a measurement
/// set), this can be significantly more compact than StandardStMan.
///
/// `IsmReader` reconstructs full cell values by building a sorted list of
/// `(start_row, value)` entries for each column.  Reading row `r` returns
/// the value of the most recent entry whose `start_row <= r`.
///
/// Open via `open()`, then call `read_cell()` to obtain typed `CellValue`
/// variants for any managed column.
///
///
/// @par Example
/// @code{.cpp}
///   IsmReader reader;
///   reader.open("my_vis.ms", 1, table_dat);
///   auto field_id = std::get<int32_t>(reader.read_cell("FIELD_ID", 42));
/// @endcode
///
///
/// @par Motivation
/// ISM is commonly used for slowly-changing columns such as `FIELD_ID`,
/// `DATA_DESC_ID`, and `SCAN_NUMBER` in measurement sets.  Keeping a
/// lightweight in-memory value chain avoids scanning the entire file for
/// each row access.
///
class IsmReader {
  public:
    /// Open an ISM data file for reading.
    void open(std::string_view table_dir, std::size_t sm_index, const TableDatFull& table_dat);

    /// Read a scalar cell value.
    [[nodiscard]] CellValue read_cell(std::string_view col_name, std::uint64_t row) const;

    /// Check if this ISM instance manages the given column.
    [[nodiscard]] bool has_column(std::string_view col_name) const noexcept;

    /// Number of columns managed by this ISM instance.
    [[nodiscard]] std::size_t column_count() const noexcept;

    /// True if open() has been called successfully.
    [[nodiscard]] bool is_open() const noexcept;

  private:
    struct IsmColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_int;
        /// (row, value) pairs, sorted by row. The value applies from that row
        /// onward until the next entry.
        struct Entry {
            std::uint64_t start_row = 0;
            CellValue value;
        };
        std::vector<Entry> entries;
    };

    bool is_open_ = false;
    bool big_endian_ = false;
    std::uint64_t row_count_ = 0;
    std::vector<IsmColumnInfo> columns_;
};

///
/// Write-only ISM writer for producing a complete ISM `table.f0` file.
///
///
///
///
///
/// `IsmWriter` is the write-path counterpart to `IsmReader`.  It stores
/// cell values in a row-indexed buffer during the write phase, then on
/// `flush()` collapses consecutive identical values into a single run
/// (incremental encoding) before serializing the complete `.f0` file.
///
/// Usage:
/// 1. Call `setup()` with column descriptors, row count, and endianness.
/// 2. Call `write_cell()` for each cell in any order.
/// 3. Call `flush()` to produce the `.f0` binary.
/// 4. Call `make_blob()` to produce the AipsIO blob for `table.dat`.
/// 5. Call `write_file()` to write the `.f0` file to disk.
///
///
/// @par Example
/// @code{.cpp}
///   IsmWriter writer;
///   writer.setup(columns, nrow, /*big_endian=*/false);
///   for (uint64_t r = 0; r < nrow; ++r) {
///       writer.write_cell(0, CellValue(field_id_for_row(r)), r);
///   }
///   writer.write_file("my_table", 1);
/// @endcode
///
class IsmWriter {
  public:
    /// Set up the writer with column descriptors and row count.
    void setup(const std::vector<ColumnDesc>& columns, std::uint64_t row_count, bool big_endian,
               std::string_view dm_name = "ISMData");

    /// Write a scalar cell value.
    void write_cell(std::size_t col_index, const CellValue& value, std::uint64_t row);

    /// Produce the complete .f0 file bytes.
    [[nodiscard]] std::vector<std::uint8_t> flush() const;

    /// Produce the ISM blob for inclusion in table.dat.
    [[nodiscard]] std::vector<std::uint8_t> make_blob() const;

    /// Write the .f0 file to disk.
    void write_file(std::string_view table_dir, std::uint32_t sequence_number) const;

  private:
    struct WriterColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_int;
        /// Cell values indexed by row.
        std::vector<CellValue> row_values;
    };

    bool is_setup_ = false;
    bool big_endian_ = false;
    std::string dm_name_;
    std::uint64_t row_count_ = 0;
    std::vector<WriterColumnInfo> columns_;
};

/// @}
} // namespace casacore_mini
