// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table_desc.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Read-only access to StandardStMan (SSM) data files.
/// @ingroup tables
/// @addtogroup tables
/// @{

///
/// Row-to-bucket index for one column group within an SSM file.
///
///
///
///
///
/// The SSM keeps one or more row-to-bucket indices embedded in index buckets
/// within the `.f0` file.  Each `SsmIndex` covers a column group (set of
/// columns stored together at the same bucket offset).  The `last_row` and
/// `bucket_number` arrays are parallel: `last_row[i]` is the highest row
/// stored in bucket `bucket_number[i]`.
///
struct SsmIndex {
    std::uint32_t rows_per_bucket = 0;
    std::int32_t nr_columns = 0;
    /// last_row[i] = last row number stored in bucket i.
    std::vector<std::uint64_t> last_row;
    /// bucket_number[i] = bucket number for the i-th index entry.
    std::vector<std::uint32_t> bucket_number;
};

///
/// Parsed SSM file header from the first 512 bytes of a `.f0` file.
///
///
///
///
///
/// The SSM file begins with a 512-byte header containing layout parameters
/// that describe the bucket pool, the free-list chain, and the location and
/// size of the index data.  `ssm_big_endian` reflects the actual byte order
/// used by bucket and indirect data; for files older than version 3 it is
/// taken from the `table.dat` endian flag passed into the parser.
///
struct SsmFileHeader {
    std::uint32_t bucket_size = 0;
    std::uint32_t nr_buckets = 0;
    std::uint32_t pers_cache_size = 0;
    std::uint32_t free_buckets_nr = 0;
    std::int32_t first_free_bucket = 0;
    std::uint32_t nr_idx_buckets = 0;
    std::int32_t first_idx_bucket = 0;
    std::uint32_t idx_bucket_offset = 0;
    std::int32_t last_string_bucket = 0;
    std::uint32_t index_length = 0;
    std::uint32_t nr_indices = 0;
    /// Actual endianness of bucket/indirect data (from SSM header v>=3 flag).
    /// For v<3 files, this is set to the table_dat big_endian value passed in.
    bool ssm_big_endian = false;
};

///
/// Parsed SSM data blob extracted from `table.dat`.
///
///
///
///
///
/// The `table.dat` file contains an AipsIO blob for each storage manager that
/// describes how columns map onto the SSM bucket layout.  `SsmTableDatBlob`
/// captures the two arrays that are essential for cell reads:
/// - `column_offset[i]` — byte offset of column `i` within each bucket row.
/// - `col_index_map[i]` — which `SsmIndex` entry governs column `i`.
///
struct SsmTableDatBlob {
    std::string data_man_name;
    /// Per-column byte offset within a bucket.
    std::vector<std::uint32_t> column_offset;
    /// Maps column number to SSMIndex number.
    std::vector<std::uint32_t> col_index_map;
};

/// A typed cell value read from SSM storage.
using CellValue = std::variant<bool, std::int32_t, std::uint32_t, std::int64_t, float, double,
                               std::complex<float>, std::complex<double>, std::string>;

///
/// Read-only SSM reader for a single table directory.
///
///
///
///
///
/// `SsmReader` decodes cell values from the StandardStMan binary layout used
/// by casacore.  The SSM partitions table rows into fixed-size buckets that
/// are packed end-to-end in the `.f0` data file (after the 512-byte header).
/// Within each bucket, each column occupies a fixed byte range determined by
/// the column's external size and the bucket row offset.
///
/// Variable-shape (indirect) columns store a 64-bit file offset in the bucket
/// instead of the data itself; the actual array data lives in a companion
/// `.f0i` file (StManArrayFile format) that is loaded lazily on first access.
///
/// Usage:
/// 1. Call `open()` with the table directory, SM index, and parsed table.dat.
/// 2. Call `read_cell()` to read scalar values, or the typed `read_array_*`
///    variants for array columns.
/// 3. Call `read_indirect_*` for variable-shape columns.
///
///
/// @par Example
/// @code{.cpp}
///   SsmReader reader;
///   reader.open("my_vis.ms", 0, table_dat);
///   CellValue v = reader.read_cell("ANTENNA1", 0);
///   auto uvw = reader.read_indirect_double("UVW", 0);
/// @endcode
///
///
/// @par Motivation
/// StandardStMan is the most common storage manager in real-world casacore
/// tables.  Reading it requires understanding the bucket pool layout, the
/// per-column offset table, and the indirect array companion file — all details
/// that are encapsulated here to keep the higher-level `Table` API clean.
///
class SsmReader {
  public:
    /// Open an SSM data file for reading.
    ///
    /// @param table_dir Path to the table directory.
    /// @param sm_index Index into TableDatFull::storage_managers for this SSM.
    /// @param table_dat Parsed table.dat metadata.
    /// @throws std::runtime_error on I/O or format errors.
    void open(std::string_view table_dir, std::size_t sm_index, const TableDatFull& table_dat);

    /// Read a scalar cell value.
    ///
    /// @param col_name Column name.
    /// @param row Row number (0-based).
    /// @returns The cell value.
    /// @throws std::runtime_error if column not found or row out of range.
    [[nodiscard]] CellValue read_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read a fixed-shape array cell as raw bytes.
    ///
    /// @param col_name Column name.
    /// @param row Row number (0-based).
    /// @returns Raw bytes (element_size * n_elements).
    /// @throws std::runtime_error if column not found, not an array, or row out of range.
    [[nodiscard]] std::vector<std::uint8_t> read_array_raw(std::string_view col_name,
                                                           std::uint64_t row) const;

    /// Read a fixed-shape double array cell.
    [[nodiscard]] std::vector<double> read_array_double(std::string_view col_name,
                                                        std::uint64_t row) const;

    /// Read a fixed-shape float array cell.
    [[nodiscard]] std::vector<float> read_array_float(std::string_view col_name,
                                                      std::uint64_t row) const;

    /// Read an indirect (variable-shape) float array cell from the .f0i file.
    [[nodiscard]] std::vector<float> read_indirect_float(std::string_view col_name,
                                                         std::uint64_t row) const;

    /// Read an indirect (variable-shape) double array cell from the .f0i file.
    [[nodiscard]] std::vector<double> read_indirect_double(std::string_view col_name,
                                                           std::uint64_t row) const;

    /// Read an indirect (variable-shape) bool array cell from the .f0i file.
    [[nodiscard]] std::vector<bool> read_indirect_bool(std::string_view col_name,
                                                       std::uint64_t row) const;

    /// Read an indirect (variable-shape) int32 array cell from the .f0i file.
    [[nodiscard]] std::vector<std::int32_t> read_indirect_int(std::string_view col_name,
                                                              std::uint64_t row) const;

    /// Read an indirect (variable-shape) complex<float> array from the .f0i file.
    [[nodiscard]] std::vector<std::complex<float>> read_indirect_complex(std::string_view col_name,
                                                                         std::uint64_t row) const;

    /// Read an indirect (variable-shape) complex<double> array from the .f0i file.
    [[nodiscard]] std::vector<std::complex<double>>
    read_indirect_dcomplex(std::string_view col_name, std::uint64_t row) const;

    /// Read an indirect (variable-shape) string array from the .f0i file.
    [[nodiscard]] std::vector<std::string> read_indirect_string(std::string_view col_name,
                                                                std::uint64_t row) const;

    /// Read the shape of an indirect (variable-shape) array cell.
    [[nodiscard]] std::vector<std::int64_t> read_indirect_shape(std::string_view col_name,
                                                                std::uint64_t row) const;

    /// Check if a column is stored indirectly (variable-shape SSMIndColumn).
    [[nodiscard]] bool is_indirect(std::string_view col_name) const;

    /// Read the Int64 file offset for an indirect column at a given row.
    [[nodiscard]] std::int64_t read_indirect_offset(std::string_view col_name,
                                                    std::uint64_t row) const;

    /// Check if this SSM instance manages the given column.
    [[nodiscard]] bool has_column(std::string_view col_name) const noexcept;

    /// Number of columns managed by this SSM instance.
    [[nodiscard]] std::size_t column_count() const noexcept;

    /// True if open() has been called successfully.
    [[nodiscard]] bool is_open() const noexcept;

  private:
    /// Find the column index (in table_dat column order) for a column name.
    [[nodiscard]] std::size_t find_column(std::string_view col_name) const;

    /// Find the bucket data range for a given row within an index.
    struct BucketLookup {
        std::uint32_t bucket_nr = 0;
        std::uint64_t start_row = 0;
        std::uint64_t end_row = 0;
    };
    [[nodiscard]] BucketLookup find_bucket(const SsmIndex& index, std::uint64_t row) const;

    /// Compute external size in bytes for a column data type.
    [[nodiscard]] static std::uint32_t external_size_bytes(DataType dt, bool big_endian);

    bool is_open_ = false;
    bool big_endian_ = false;
    std::uint64_t row_count_ = 0;
    SsmFileHeader file_header_;
    SsmTableDatBlob blob_;
    std::vector<SsmIndex> indices_;

    /// Raw bucket data from .f0 (everything after the 512-byte header).
    std::vector<std::uint8_t> bucket_data_;

    /// Column metadata needed for cell reads.
    struct ColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_int;
        ColumnKind kind = ColumnKind::scalar;
        std::uint32_t col_offset = 0;          // byte offset in bucket
        std::uint32_t index_nr = 0;            // which SsmIndex
        std::uint32_t ext_size = 0;            // external size per element in bytes
        std::uint64_t n_elements = 1;          // product of shape (1 for scalars)
        std::vector<std::int64_t> fixed_shape; // fixed array shape, if any
        std::uint32_t row_size = 0;            // total bytes per row (ext_size * n_elements)
        bool indirect = false;                 // true for variable-shape (SSMIndColumn)
        bool indirect_string = false;          // true for TpString SSMIndStringColumn
    };
    std::vector<ColumnInfo> columns_;

    /// Read the Int64 file offset for an indirect column at a given row.
    [[nodiscard]] std::int64_t read_indirect_offset(const ColumnInfo& col, std::uint64_t row) const;

    /// Pointer to the full table metadata for reference.
    const TableDatFull* table_dat_ = nullptr;

    /// Path to table directory (needed for lazy .f0i loading).
    std::string table_dir_;
    std::uint32_t sm_seq_nr_ = 0;

    /// Indirect array file data (.f0i), loaded lazily.
    mutable std::vector<std::uint8_t> indirect_data_;
    mutable bool indirect_loaded_ = false;
    mutable std::uint32_t indirect_version_ = 0;
    void ensure_indirect_loaded() const;
};

///
/// Write-only SSM writer for producing a complete SSM `table.f0` file.
///
///
///
///
///
/// `SsmWriter` is the write-path counterpart to `SsmReader`.  It allocates a
/// flat bucket pool in memory and fills in cell values row by row.  When all
/// cells have been written, `flush()` serializes the complete `.f0` file
/// (512-byte header + packed bucket data), and `make_blob()` produces the
/// AipsIO blob for the `table.dat` storage manager descriptor.
///
/// For indirect (variable-shape) columns, `write_indirect_array` appends
/// data to an in-memory StManArrayFile buffer that `flush_indirect()` and
/// `write_indirect_file()` persist as the `.f0i` file.
///
/// Usage:
/// 1. Call `setup()` with column descriptors, row count, endianness, and
///    data manager name.
/// 2. Call `write_cell()` or `write_array_*()` for each cell.
/// 3. Call `flush()` and `make_blob()` to get the binary output.
/// 4. Call `write_file()` (and optionally `write_indirect_file()`) to write
///    the `.f0` (and `.f0i`) files to disk.
///
///
/// @par Example
/// @code{.cpp}
///   SsmWriter writer;
///   writer.setup(columns, nrow, /*big_endian=*/false);
///   for (uint64_t r = 0; r < nrow; ++r) {
///       writer.write_cell(0, CellValue(std::int32_t(r)), r);
///   }
///   writer.write_file("my_table", 0);
///   auto blob = writer.make_blob();
/// @endcode
///
class SsmWriter {
  public:
    /// Set up the writer with column descriptors and row count.
    ///
    /// @param columns Column descriptors for columns managed by this SSM.
    /// @param row_count Number of rows in the table.
    /// @param big_endian Whether to write in big-endian byte order.
    /// @param dm_name Data manager name (e.g. "StandardStMan").
    void setup(const std::vector<ColumnDesc>& columns, std::uint64_t row_count, bool big_endian,
               std::string_view dm_name = "StandardStMan");

    /// Write a scalar cell value.
    ///
    /// @param col_index Column index (0-based, in setup order).
    /// @param value The cell value to write.
    /// @param row Row number (0-based).
    /// @throws std::runtime_error if col_index or row out of range.
    void write_cell(std::size_t col_index, const CellValue& value, std::uint64_t row);

    /// Write a fixed-shape array cell as raw bytes (for direct SSM columns).
    ///
    /// @param col_index Column index (0-based, in setup order).
    /// @param data Raw bytes (must be ext_size * n_elements).
    /// @param row Row number (0-based).
    void write_array_raw(std::size_t col_index, std::span<const std::uint8_t> data,
                         std::uint64_t row);

    /// Write a fixed-shape float array cell.
    void write_array_float(std::size_t col_index, const std::vector<float>& values,
                           std::uint64_t row);

    /// Write a fixed-shape double array cell.
    void write_array_double(std::size_t col_index, const std::vector<double>& values,
                            std::uint64_t row);

    /// Write a raw Int64 offset into an indirect column's bucket slot.
    /// Used by Table::open_rw() to preserve existing indirect offsets
    /// without modifying the .f0i file.
    void write_indirect_offset(std::size_t col_index, std::uint64_t row, std::int64_t offset);

    /// Write a variable-shape array to indirect storage (.f0i file).
    /// Stores the file offset in the SSM bucket and appends array data to
    /// the indirect buffer.
    ///
    /// @param col_index Column index.
    /// @param shape Array shape dimensions.
    /// @param data Raw element bytes in canonical format.
    /// @param row Row number.
    void write_indirect_array(std::size_t col_index, const std::vector<std::int32_t>& shape,
                              std::span<const std::uint8_t> data, std::uint64_t row);

    /// Produce the complete .f0 file bytes (header + buckets).
    ///
    /// @returns Raw bytes for the .f0 file.
    [[nodiscard]] std::vector<std::uint8_t> flush() const;

    /// Produce the .f0i indirect array file bytes (StManArrayFile format).
    /// Returns empty if no indirect arrays were written.
    [[nodiscard]] std::vector<std::uint8_t> flush_indirect() const;

    /// Write the .f0i file to disk (if indirect arrays exist).
    void write_indirect_file(std::string_view table_dir, std::uint32_t sequence_number) const;

    /// Produce the SSM blob for inclusion in table.dat
    /// StorageManagerSetup::data_blob.
    ///
    /// @returns Raw bytes for the SSM blob (big-endian AipsIO format).
    [[nodiscard]] std::vector<std::uint8_t> make_blob() const;

    /// Write the .f0 file to disk.
    ///
    /// @param table_dir Path to the table directory.
    /// @param sequence_number SM sequence number (file is table.f<N>).
    void write_file(std::string_view table_dir, std::uint32_t sequence_number) const;

  private:
    struct WriterColumnInfo {
        std::string name;
        DataType data_type = DataType::tp_int;
        ColumnKind kind = ColumnKind::scalar;
        std::uint32_t ext_size = 0;   // per-element size in bytes
        std::uint32_t col_offset = 0; // byte offset in bucket
        std::uint64_t n_elements = 1; // product of shape (1 for scalars)
        std::uint32_t row_size = 0;   // bytes per row in bucket
        bool indirect = false;        // true for variable-shape arrays (SSMIndColumn)
    };

    bool is_setup_ = false;
    bool big_endian_ = false;
    std::string dm_name_;
    std::uint64_t row_count_ = 0;
    std::uint32_t bucket_size_ = 0;
    std::uint32_t rows_per_bucket_ = 0;
    std::vector<WriterColumnInfo> columns_;

    /// Flat bucket data storage.
    std::vector<std::uint8_t> bucket_data_;
    /// Number of data buckets allocated.
    std::uint32_t nr_data_buckets_ = 0;
    /// String bucket for strings > 8 bytes.
    std::vector<std::uint8_t> string_bucket_data_;
    std::int32_t last_string_bucket_ = -1;
    std::uint32_t string_bucket_offset_ = 0;
    std::uint32_t nr_string_buckets_ = 0;

    /// Indirect array file data (StManArrayFile format, excluding header).
    /// Header (16 bytes) is prepended by flush_indirect().
    std::vector<std::uint8_t> indirect_data_;
};

/// Parse an SSM data blob from table.dat.
///
/// The blob contains: AipsIO "SSM" object with data_man_name,
/// column_offset Block<uInt>, and col_index_map Block<uInt>.
///
/// @param blob Raw bytes from StorageManagerSetup::data_blob.
/// @returns Parsed SSM blob.
/// @throws std::runtime_error on malformed data.
[[nodiscard]] SsmTableDatBlob parse_ssm_blob(std::span<const std::uint8_t> blob);

/// Parse the SSM file header from the first 512 bytes of a .f0 file.
///
/// @param header_bytes The first 512 bytes of the .f0 file.
/// @param big_endian Whether the table uses big-endian storage.
/// @returns Parsed file header.
/// @throws std::runtime_error on malformed header.
[[nodiscard]] SsmFileHeader parse_ssm_file_header(std::span<const std::uint8_t> header_bytes,
                                                  bool big_endian);

/// Parse SSM indices from index bucket data.
///
/// @param index_data Raw index data (assembled from index buckets).
/// @param nr_indices Number of indices to parse.
/// @param big_endian Whether the table uses big-endian storage.
/// @returns Vector of parsed SSM indices.
/// @throws std::runtime_error on malformed data.
[[nodiscard]] std::vector<SsmIndex> parse_ssm_indices(std::span<const std::uint8_t> index_data,
                                                      std::uint32_t nr_indices, bool big_endian);

/// @}
} // namespace casacore_mini
