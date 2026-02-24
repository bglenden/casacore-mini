#pragma once

#include "casacore_mini/table_desc.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Read-only access to StandardStMan (SSM) data files.

/// Row-to-bucket index for one column group.
struct SsmIndex {
    std::uint32_t rows_per_bucket = 0;
    std::int32_t nr_columns = 0;
    /// last_row[i] = last row number stored in bucket i.
    std::vector<std::uint64_t> last_row;
    /// bucket_number[i] = bucket number for the i-th index entry.
    std::vector<std::uint32_t> bucket_number;
};

/// Parsed SSM file header from .f0.
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
};

/// Parsed SSM data blob from table.dat.
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

/// Read-only SSM reader for a single table directory.
///
/// Usage:
/// 1. Call `open()` to parse the table.dat SM blob and .f0 file.
/// 2. Call `read_cell()` to read individual cell values.
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
        std::uint32_t col_offset = 0; // byte offset in bucket
        std::uint32_t index_nr = 0;   // which SsmIndex
        std::uint32_t ext_size = 0;   // external size per row in bytes
    };
    std::vector<ColumnInfo> columns_;

    /// Pointer to the full table metadata for reference.
    const TableDatFull* table_dat_ = nullptr;
};

/// Write-only SSM writer for producing a complete SSM table.f0 file.
///
/// Usage:
/// 1. Call `setup()` with column descriptions and row count.
/// 2. Call `write_cell()` for each cell value.
/// 3. Call `flush()` to produce the .f0 file bytes.
/// 4. Call `make_blob()` to produce the SSM blob for table.dat.
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

    /// Produce the complete .f0 file bytes (header + buckets).
    ///
    /// @returns Raw bytes for the .f0 file.
    [[nodiscard]] std::vector<std::uint8_t> flush() const;

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
        std::uint32_t ext_size = 0;
        std::uint32_t col_offset = 0;
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

} // namespace casacore_mini
