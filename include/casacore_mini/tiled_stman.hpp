#pragma once

#include "casacore_mini/table_desc.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Read/write access to Tiled Storage Manager (TSM) data files.
///
/// Supports TiledColumnStMan, TiledCellStMan, TiledShapeStMan, and
/// TiledDataStMan for fixed-shape array columns with a single hypercube.

/// Read-only TSM reader for a single table directory.
///
/// Reads array cell values from the `_TSM0` data file using the hypercube
/// layout described in the `.f0` header file.
class TiledStManReader {
  public:
    /// Open a TSM data file pair (.f0 header + .f0_TSM0 data) for reading.
    void open(std::string_view table_dir, std::size_t sm_index, const TableDatFull& table_dat);

    /// Read all array elements for a cell as a flat vector of floats.
    /// Only tp_float arrays are currently supported.
    [[nodiscard]] std::vector<float> read_float_cell(std::string_view col_name,
                                                     std::uint64_t row) const;

    /// Read all array elements for a cell as a flat vector of ints.
    [[nodiscard]] std::vector<std::int32_t> read_int_cell(std::string_view col_name,
                                                          std::uint64_t row) const;

    /// Read raw cell bytes (works for any data type including tp_complex).
    /// Returns element_size * cell_elements bytes.
    [[nodiscard]] std::vector<std::uint8_t> read_raw_cell(std::string_view col_name,
                                                          std::uint64_t row) const;

    /// Number of columns managed by this TSM instance.
    [[nodiscard]] std::size_t column_count() const noexcept;

    /// True if open() has been called successfully.
    [[nodiscard]] bool is_open() const noexcept;

  private:
    struct TsmColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_float;
        std::uint32_t element_size = 0;  // bytes per element
        std::vector<std::int64_t> shape; // cell shape
        std::uint64_t cell_elements = 0; // product of shape
    };

    /// Tile layout for the shared-bucket model.
    struct TileLayout {
        std::vector<std::int64_t> tile_shape;
        std::uint64_t tile_pixels = 0;
        std::int64_t tile_rows = 0;
        std::uint64_t bucket_size = 0;
        std::uint64_t tiles_along_row = 0;
        std::vector<std::size_t> sorted_col_indices;
        std::vector<std::uint64_t> col_tile_offsets;
    };

    const TsmColumnInfo* find_column(std::string_view col_name, std::size_t& original_index) const;
    [[nodiscard]] std::uint64_t cell_byte_offset(std::size_t original_col_index,
                                                 const TsmColumnInfo& column,
                                                 std::uint64_t row) const;

    bool is_open_ = false;
    std::uint64_t row_count_ = 0;
    TileLayout tile_layout_;
    std::vector<TsmColumnInfo> columns_;
    /// Raw TSM0 data file contents.
    std::vector<std::uint8_t> tsm_data_;
};

/// Write-only TSM writer for producing a complete TSM table directory.
///
/// Writes a .f0 header and .f0_TSM0 data file for a single hypercube.
/// Supports TiledColumnStMan and TiledCellStMan for fixed-shape arrays.
class TiledStManWriter {
  public:
    /// Set up the writer with column descriptors and row count.
    /// @param sm_type  SM type string (e.g. "TiledColumnStMan")
    /// @param dm_name  Data manager name (e.g. "TiledCol")
    /// @param columns  Column descriptors (must have shape set)
    /// @param row_count Number of rows
    void setup(std::string_view sm_type, std::string_view dm_name,
               const std::vector<ColumnDesc>& columns, std::uint64_t row_count);

    /// Write all array elements for a cell.
    void write_float_cell(std::size_t col_index, const std::vector<float>& values,
                          std::uint64_t row);

    /// Write all array elements for a cell (Int).
    void write_int_cell(std::size_t col_index, const std::vector<std::int32_t>& values,
                        std::uint64_t row);

    /// Write raw cell bytes (works for any data type including tp_complex).
    /// @p data must be exactly element_size * cell_elements bytes.
    void write_raw_cell(std::size_t col_index, const std::vector<std::uint8_t>& data,
                        std::uint64_t row);

    /// Produce the TSM blob for inclusion in table.dat.
    [[nodiscard]] std::vector<std::uint8_t> make_blob() const;

    /// Write the .f0 header and .f0_TSM0 data files to disk.
    void write_files(std::string_view table_dir, std::uint32_t sequence_number) const;

  private:
    struct WriterColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_float;
        std::uint32_t element_size = 0;
        std::vector<std::int64_t> shape;
        std::uint64_t cell_elements = 0;
        /// Flat data buffer for all rows (row-major: row0_data, row1_data, ...).
        std::vector<std::uint8_t> data;
    };

    bool is_setup_ = false;
    std::string sm_type_;
    std::string dm_name_;
    std::uint64_t row_count_ = 0;
    std::vector<WriterColumnInfo> columns_;
};

} // namespace casacore_mini
