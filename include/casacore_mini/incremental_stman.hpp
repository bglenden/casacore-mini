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

/// Read-only ISM reader for a single table directory.
///
/// ISM stores values incrementally: a value is only stored when it changes
/// from the previous row. This reader reconstructs full cell values by
/// walking the value chain for each column.
class IsmReader {
  public:
    /// Open an ISM data file for reading.
    void open(std::string_view table_dir, std::size_t sm_index, const TableDatFull& table_dat);

    /// Read a scalar cell value.
    [[nodiscard]] CellValue read_cell(std::string_view col_name, std::uint64_t row) const;

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

/// Write-only ISM writer for producing a complete ISM table.f0 file.
///
/// This writer stores values incrementally. Consecutive identical values
/// are collapsed into a single entry.
class IsmWriter {
  public:
    /// Set up the writer with column descriptors and row count.
    void setup(const std::vector<ColumnDesc>& columns, std::uint64_t row_count, bool big_endian,
               std::string_view dm_name = "ISMData");

    /// Write a scalar cell value.
    void write_cell(std::size_t col_index, std::uint64_t row, const CellValue& value);

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

} // namespace casacore_mini
