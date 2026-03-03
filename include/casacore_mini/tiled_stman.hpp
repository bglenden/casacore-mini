// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table_desc.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

class Table; // forward declaration for friend access

/// @file
/// @brief Read/write access to Tiled Storage Manager (TSM) data files.
///
/// Supports TiledColumnStMan, TiledCellStMan, TiledShapeStMan, and
/// TiledDataStMan for fixed-shape array columns with a single hypercube.

/// <summary>
/// Read-only TSM reader for a single table directory.
/// </summary>
///
/// <use visibility=local/>
///
/// <synopsis>
/// The Tiled Storage Manager stores multidimensional array data in tile-based
/// hypercubes.  Each hypercube is described by a `.f0` AipsIO header file
/// that specifies the tile shape, cube shape, and column layout.  The actual
/// data lives in one or more `_TSM<N>` data files.
///
/// `TiledStManReader` reads these files using the hypercube geometry to
/// locate the tile that contains each requested row, then extracts the
/// per-column slice from within that tile.
///
/// Variants supported:
/// - `TiledColumnStMan` — fixed cell shape, one or more columns per hypercube.
/// - `TiledCellStMan` — each row is a separate sub-cube.
/// - `TiledShapeStMan` / `TiledDataStMan` — variable or fixed per-row shapes
///   with an auxiliary shape-row interval map.
///
/// Usage:
/// 1. Call `open()` with the table directory, SM index, and parsed table.dat.
/// 2. Call the typed `read_*_cell()` methods by column name and row.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   TiledStManReader reader;
///   reader.open("my_image.image", 0, table_dat);
///   auto pixels = reader.read_float_cell("map", 0);
/// </srcblock>
/// </example>
///
/// <motivation>
/// The TSM is the dominant storage manager for CASA image data and for
/// visibility data columns (DATA, FLAG, WEIGHT_SPECTRUM) in large measurement
/// sets.  Its tile layout enables efficient I/O for both sequential and random
/// multi-dimensional access patterns.
/// </motivation>
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

    /// Read the cell shape for a row (for variable-shape TSM layouts).
    [[nodiscard]] std::vector<std::int64_t> cell_shape(std::string_view col_name,
                                                       std::uint64_t row) const;

    /// Number of columns managed by this TSM instance.
    [[nodiscard]] std::size_t column_count() const noexcept;

    /// True if open() has been called successfully.
    [[nodiscard]] bool is_open() const noexcept;

  private:
    friend class Table;

    /// Check if this TSM instance manages a column with the given name.
    [[nodiscard]] bool has_column(std::string_view col_name) const noexcept;
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

    struct CubeLayout {
        std::size_t original_cube_index = 0;
        std::uint64_t row_start = 0;
        std::uint64_t row_count = 0;
        std::int32_t file_nr = 0;
        std::uint64_t file_offset = 0;
        std::vector<std::int64_t> cell_shape;
        std::vector<std::int64_t> tile_shape;
        std::uint64_t tile_pixels = 0;
        std::int64_t tile_rows = 0;
        std::uint64_t bucket_size = 0;
        std::vector<std::uint64_t> col_tile_offsets;
    };

    struct ShapeRowInterval {
        std::uint64_t row_end = 0;
        std::uint32_t cube_index = 0;
        std::uint32_t pos_end = 0;
    };

    struct DataRowChunk {
        std::uint64_t row_start = 0;
        std::uint32_t cube_index = 0;
        std::uint32_t pos_start = 0;
    };

    struct RowLocation {
        const CubeLayout* cube = nullptr;
        std::uint64_t row_in_cube = 0;
    };

    const TsmColumnInfo* find_column(std::string_view col_name, std::size_t& original_index) const;
    [[nodiscard]] const CubeLayout* find_cube_for_row(std::uint64_t row) const;
    [[nodiscard]] const CubeLayout* find_cube_by_index(std::size_t cube_index) const;
    [[nodiscard]] std::optional<RowLocation> locate_row(std::uint64_t row) const;
    [[nodiscard]] const std::vector<std::uint8_t>* find_file_data(std::int32_t file_nr) const;
    [[nodiscard]] std::uint64_t cell_byte_offset(std::size_t original_col_index,
                                                 const TsmColumnInfo& column,
                                                 std::uint64_t row_in_cube,
                                                 const CubeLayout& cube) const;
    [[nodiscard]] std::uint64_t cell_elements_for_row(const TsmColumnInfo& column,
                                                      const CubeLayout& cube) const;

    bool is_open_ = false;
    std::uint64_t row_count_ = 0;
    TileLayout tile_layout_;
    std::vector<TsmColumnInfo> columns_;
    std::vector<CubeLayout> cubes_;
    std::vector<ShapeRowInterval> shape_row_intervals_;
    bool use_shape_row_map_ = false;
    std::vector<DataRowChunk> data_row_chunks_;
    bool use_data_row_map_ = false;
    std::vector<std::int32_t> tsm_file_seqnrs_;
    std::vector<std::vector<std::uint8_t>> tsm_file_data_;
};

/// <summary>
/// Write-only TSM writer for producing a complete TSM table directory.
/// </summary>
///
/// <use visibility=local/>
///
/// <synopsis>
/// `TiledStManWriter` writes the `.f0` AipsIO header and the `_TSM0` data
/// file for a single hypercube.  It supports `TiledColumnStMan` and
/// `TiledCellStMan` layouts for fixed-shape array columns.
///
/// All cell data is accumulated in a flat per-column buffer during the write
/// phase.  `write_files()` tiles the data according to the tile shape and
/// writes both the header and data files to disk.
///
/// Usage:
/// 1. Call `setup()` with SM type, data manager name, column descriptors,
///    row count, and byte order.
/// 2. Call `write_*_cell()` for each cell.
/// 3. Call `make_blob()` to produce the TSM blob for `table.dat`.
/// 4. Call `write_files()` to write the `.f0` and `_TSM0` files.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   TiledStManWriter writer;
///   writer.setup("TiledColumnStMan", "TiledData", columns, nrow, false);
///   for (uint64_t r = 0; r < nrow; ++r) {
///       writer.write_float_cell(0, data_for_row(r), r);
///   }
///   writer.write_files("my_image.image", 0);
/// </srcblock>
/// </example>
class TiledStManWriter {
  public:
    /// Set up the writer with column descriptors and row count.
    /// @param sm_type  SM type string (e.g. "TiledColumnStMan")
    /// @param dm_name  Data manager name (e.g. "TiledCol")
    /// @param columns  Column descriptors (must have shape set)
    /// @param row_count Number of rows
    /// @param big_endian  If true, flag data as big-endian (casacore default).
    void setup(std::string_view sm_type, std::string_view dm_name,
               const std::vector<ColumnDesc>& columns, std::uint64_t row_count,
               bool big_endian = true);

    /// Write all array elements for a cell.
    void write_float_cell(std::size_t col_index, const std::vector<float>& values,
                          std::uint64_t row);

    /// Write all array elements for a cell (Double).
    void write_double_cell(std::size_t col_index, const std::vector<double>& values,
                           std::uint64_t row);

    /// Write all array elements for a cell (Complex = pairs of float).
    void write_complex_cell(std::size_t col_index,
                            const std::vector<std::complex<float>>& values,
                            std::uint64_t row);

    /// Write all array elements for a cell (Bool = 1 byte each).
    void write_bool_cell(std::size_t col_index, const std::vector<bool>& values,
                         std::uint64_t row);

    /// Write all array elements for a cell (Int).
    void write_int_cell(std::size_t col_index, const std::vector<std::int32_t>& values,
                        std::uint64_t row);

    /// Write raw cell bytes (works for any data type including tp_complex).
    /// @p data must be exactly element_size * cell_elements bytes.
    void write_raw_cell(std::size_t col_index, const std::vector<std::uint8_t>& data,
                        std::uint64_t row);

    /// Read back float data from writer buffer (for read-after-write consistency).
    [[nodiscard]] std::vector<float> read_float_cell(std::size_t col_index,
                                                     std::uint64_t row) const;

    /// Read back double data from writer buffer.
    [[nodiscard]] std::vector<double> read_double_cell(std::size_t col_index,
                                                       std::uint64_t row) const;

    /// Read back raw bytes from writer buffer.
    [[nodiscard]] std::vector<std::uint8_t> read_raw_cell(std::size_t col_index,
                                                          std::uint64_t row) const;

    /// Check if this writer has a column by name.
    [[nodiscard]] bool has_column(std::string_view name) const;

    /// Find column index by name; returns SIZE_MAX if not found.
    [[nodiscard]] std::size_t find_column(std::string_view name) const;

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
    bool big_endian_ = true;
    std::string sm_type_;
    std::string dm_name_;
    std::uint64_t row_count_ = 0;
    std::vector<WriterColumnInfo> columns_;
};

} // namespace casacore_mini
