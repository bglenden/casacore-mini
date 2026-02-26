#include "casacore_mini/incremental_stman.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_writer.hpp"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {
namespace {

// --- Endian helpers (same as standard_stman.cpp; duplicated to avoid cross-TU linkage) ---

void write_le_u32_buf(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8U);
    p[2] = static_cast<std::uint8_t>(v >> 16U);
    p[3] = static_cast<std::uint8_t>(v >> 24U);
}

void write_le_i32_buf(std::uint8_t* p, std::int32_t v) noexcept {
    write_le_u32_buf(p, static_cast<std::uint32_t>(v));
}

[[nodiscard]] std::uint32_t read_le_u32_buf(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U);
}

[[nodiscard]] std::int32_t read_le_i32_buf(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_le_u32_buf(p));
}

[[nodiscard]] std::uint64_t read_le_u64_buf(const std::uint8_t* p) noexcept {
    return static_cast<std::uint64_t>(p[0]) | (static_cast<std::uint64_t>(p[1]) << 8U) |
           (static_cast<std::uint64_t>(p[2]) << 16U) | (static_cast<std::uint64_t>(p[3]) << 24U) |
           (static_cast<std::uint64_t>(p[4]) << 32U) | (static_cast<std::uint64_t>(p[5]) << 40U) |
           (static_cast<std::uint64_t>(p[6]) << 48U) | (static_cast<std::uint64_t>(p[7]) << 56U);
}

[[nodiscard]] double read_le_f64_buf(const std::uint8_t* p) noexcept {
    double v = 0;
    std::memcpy(&v, p, 8);
    return v;
}

// ISM bucket format (LE):
//
// Each bucket stores a packed data area followed by a per-column index.
// Layout:
//   bucket[0..4)            free_offset (u32): bucket-relative offset to index start
//   bucket[4..free_off)     data entries (all column values packed per-row)
//   bucket[free_off..)      column index (stored top-down, column 0 first)
//
// Data entries: each ISM entry stores ALL column values concatenated in column
// order. Each column occupies its canonical size: Bool=1, Int/uInt/Float=4,
// Double/Int64=8 bytes. An entry's total size is the sum of all column sizes.
//
// Column index (per column, sequential):
//   nr_entries (u32)
//   row_numbers[nr_entries] (u32 each)
//   data_offsets[nr_entries] (u32 each, relative to bucket[4])
//
// The data_offsets point to the specific column's value within the packed
// entry record (i.e. for column 1, the offset is entry_start + col0_size).
//
// For an ISM with version >= 5, the file header contains an endian flag byte.

// Read a file into a byte vector.
[[nodiscard]] std::vector<std::uint8_t> read_file_ism(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

/// Parse ISM file header to get bucket_size, nr_buckets, etc.
struct IsmFileHeader {
    std::uint32_t bucket_size = 0;
    std::uint32_t nr_buckets = 0;
    std::uint32_t pers_cache_size = 0;
    bool big_endian = false;
    std::uint32_t version = 0;
    std::uint32_t unique_nr_row = 0;
};

[[nodiscard]] IsmFileHeader parse_ism_file_header(std::span<const std::uint8_t> header_bytes) {
    if (header_bytes.size() < 512) {
        throw std::runtime_error("ISM file header too short");
    }

    // Read AipsIO object header using LE (default for macOS).
    // Magic is 0xBEBEBEBE (same in any endianness).
    const auto magic = read_le_u32_buf(header_bytes.data());
    if (magic != kAipsIoMagic) {
        throw std::runtime_error("bad AipsIO magic in ISM header");
    }

    // Object length.
    static_cast<void>(read_le_u32_buf(header_bytes.data() + 4));
    // String length + string.
    const auto str_len = read_le_u32_buf(header_bytes.data() + 8);
    const auto version_offset = 12 + str_len;

    IsmFileHeader result;
    result.version = read_le_u32_buf(header_bytes.data() + version_offset);

    auto off = version_offset + 4;

    // Version >= 5 has endian flag byte.
    if (result.version >= 5) {
        result.big_endian = header_bytes[off] != 0;
        ++off;
    }

    result.bucket_size = read_le_u32_buf(header_bytes.data() + off);
    off += 4;
    result.nr_buckets = read_le_u32_buf(header_bytes.data() + off);
    off += 4;
    result.pers_cache_size = read_le_u32_buf(header_bytes.data() + off);
    // Additional fields follow but we don't need them all.

    return result;
}

/// Per-column index parsed from an ISM bucket.
struct IsmBucketColIndex {
    std::uint32_t nr_entries = 0;
    std::vector<std::uint32_t> row_numbers;
    std::vector<std::uint32_t> data_offsets; // relative to data origin (bucket + 4)
};

/// Parse the per-column index from an ISM bucket.
///
/// The index is stored top-down starting at bucket[free_offset].
/// Per column (column 0 first):
///   nr_entries (u32)
///   row_numbers[nr_entries] (u32 each)
///   data_offsets[nr_entries] (u32 each)
[[nodiscard]] std::vector<IsmBucketColIndex>
parse_ism_bucket_indices(const std::uint8_t* bucket_data, std::uint32_t /*bucket_size*/,
                         std::uint32_t nr_columns) {

    // free_offset is the first u32 in the bucket. It is a bucket-relative offset
    // to the start of the column index area (right after the data entries).
    // Data origin is bucket[4]; data area spans bucket[4..free_offset).
    const auto free_offset = read_le_u32_buf(bucket_data);
    // Index starts at bucket[free_offset].
    auto pos = static_cast<std::size_t>(free_offset);

    std::vector<IsmBucketColIndex> indices(nr_columns);

    for (std::uint32_t c = 0; c < nr_columns; ++c) {
        auto& idx = indices[c];
        idx.nr_entries = read_le_u32_buf(bucket_data + pos);
        pos += 4;

        idx.row_numbers.resize(idx.nr_entries);
        for (std::uint32_t i = 0; i < idx.nr_entries; ++i) {
            idx.row_numbers[i] = read_le_u32_buf(bucket_data + pos);
            pos += 4;
        }

        idx.data_offsets.resize(idx.nr_entries);
        for (std::uint32_t i = 0; i < idx.nr_entries; ++i) {
            idx.data_offsets[i] = read_le_u32_buf(bucket_data + pos);
            pos += 4;
        }
    }

    return indices;
}

/// Read a value from the ISM data area at a given offset.
/// The data origin is bucket + 4 (after the free_offset u32).
/// Bool is stored as 1 byte (uChar), Int/uInt as 4, Double/Int64 as 8.
// NOLINTBEGIN(bugprone-branch-clone)
[[nodiscard]] CellValue read_ism_value(const std::uint8_t* data_origin, std::uint32_t offset,
                                       DataType dtype) {
    const std::uint8_t* p = data_origin + offset;
    switch (dtype) {
    case DataType::tp_bool:
        return p[0] != 0;
    case DataType::tp_int:
        return read_le_i32_buf(p);
    case DataType::tp_uint:
        return read_le_u32_buf(p);
    case DataType::tp_float: {
        float v = 0;
        std::memcpy(&v, p, 4);
        return v;
    }
    case DataType::tp_double:
        return read_le_f64_buf(p);
    case DataType::tp_int64:
        return static_cast<std::int64_t>(read_le_u64_buf(p));
    case DataType::tp_complex: {
        float re = 0;
        float im = 0;
        std::memcpy(&re, p, 4);
        std::memcpy(&im, p + 4, 4);
        return std::complex<float>(re, im);
    }
    case DataType::tp_dcomplex: {
        double re = 0;
        double im = 0;
        std::memcpy(&re, p, 8);
        std::memcpy(&im, p + 8, 8);
        return std::complex<double>(re, im);
    }
    case DataType::tp_string: {
        // ISM string format: [total_length_u32][string_bytes].
        // total_length includes the 4-byte length field itself.
        const auto total_len = read_le_u32_buf(p);
        if (total_len <= 4) {
            return std::string{};
        }
        const auto str_len = total_len - 4;
        return std::string(reinterpret_cast<const char*>(p + 4), str_len);
    }
    default:
        throw std::runtime_error("unsupported ISM value type");
    }
}
// NOLINTEND(bugprone-branch-clone)

} // namespace

// --- IsmReader ---

void IsmReader::open(const std::string_view table_dir, const std::size_t sm_index,
                     const TableDatFull& table_dat) {
    big_endian_ = table_dat.big_endian;
    row_count_ = table_dat.row_count;

    if (sm_index >= table_dat.storage_managers.size()) {
        throw std::runtime_error("ISM SM index out of range");
    }
    const auto& sm = table_dat.storage_managers[sm_index];
    if (sm.type_name != "IncrementalStMan") {
        throw std::runtime_error("not an IncrementalStMan: " + sm.type_name);
    }

    const auto f0_path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sm.sequence_number));
    auto file_data = read_file_ism(f0_path);
    if (file_data.size() < 512) {
        throw std::runtime_error("ISM file too small");
    }

    const auto header = parse_ism_file_header(std::span<const std::uint8_t>(file_data.data(), 512));

    // Build column info from table_dat.
    std::vector<DataType> col_types;
    for (const auto& cs : table_dat.column_setups) {
        if (cs.sequence_number != sm.sequence_number) {
            continue;
        }
        IsmColumnInfo info;
        info.name = cs.column_name;

        const auto& cols = table_dat.table_desc.columns;
        auto it = std::find_if(cols.begin(), cols.end(),
                               [&](const ColumnDesc& cd) { return cd.name == cs.column_name; });
        if (it == cols.end()) {
            throw std::runtime_error("column not in table desc: " + cs.column_name);
        }
        info.data_type = it->data_type;
        col_types.push_back(info.data_type);
        columns_.push_back(std::move(info));
    }

    const auto nr_columns = static_cast<std::uint32_t>(columns_.size());

    // Parse all buckets.
    constexpr std::size_t kHeaderSize = 512;
    for (std::uint32_t b = 0; b < header.nr_buckets; ++b) {
        const auto bucket_offset = kHeaderSize + static_cast<std::size_t>(b) *
                                                     static_cast<std::size_t>(header.bucket_size);
        if (file_data.size() < bucket_offset + header.bucket_size) {
            throw std::runtime_error("ISM file too small for bucket " + std::to_string(b));
        }

        const std::uint8_t* bucket = file_data.data() + bucket_offset;
        auto indices = parse_ism_bucket_indices(bucket, header.bucket_size, nr_columns);

        // Data origin is bucket + 4 (after the free_offset u32).
        const std::uint8_t* data_origin = bucket + 4;

        // Build (row, value) entries for each column from the bucket index.
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            const auto& idx = indices[c];
            for (std::uint32_t e = 0; e < idx.nr_entries; ++e) {
                IsmColumnInfo::Entry entry;
                entry.start_row = idx.row_numbers[e];
                entry.value =
                    read_ism_value(data_origin, idx.data_offsets[e], columns_[c].data_type);
                columns_[c].entries.push_back(std::move(entry));
            }
        }
    }

    // Sort entries by start_row after all buckets are processed.
    for (std::uint32_t c = 0; c < nr_columns; ++c) {
        std::sort(columns_[c].entries.begin(), columns_[c].entries.end(),
                  [](const IsmColumnInfo::Entry& a, const IsmColumnInfo::Entry& b) {
                      return a.start_row < b.start_row;
                  });
    }

    is_open_ = true;
}

CellValue IsmReader::read_cell(const std::string_view col_name, const std::uint64_t row) const {
    if (!is_open_) {
        throw std::runtime_error("IsmReader not open");
    }

    // Find column.
    const IsmColumnInfo* col = nullptr;
    for (const auto& c : columns_) {
        if (c.name == col_name) {
            col = &c;
            break;
        }
    }
    if (col == nullptr) {
        throw std::runtime_error("column not found: " + std::string(col_name));
    }

    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    // Binary search for the last entry with start_row <= row (ISM: value applies forward).
    // Entries are sorted by start_row after open().
    const auto& entries = col->entries;
    if (entries.empty()) {
        throw std::runtime_error("no ISM value for row " + std::to_string(row));
    }

    // upper_bound finds first entry with start_row > row; preceding entry is the match.
    auto it = std::upper_bound(
        entries.begin(), entries.end(), row,
        [](std::uint64_t r, const IsmColumnInfo::Entry& e) { return r < e.start_row; });
    if (it == entries.begin()) {
        throw std::runtime_error("no ISM value for row " + std::to_string(row));
    }
    --it;
    return it->value;
}

bool IsmReader::has_column(std::string_view col_name) const noexcept {
    for (const auto& col : columns_) {
        if (col.name == col_name)
            return true;
    }
    return false;
}

std::size_t IsmReader::column_count() const noexcept {
    return columns_.size();
}

bool IsmReader::is_open() const noexcept {
    return is_open_;
}

// --- IsmWriter ---

void IsmWriter::setup(const std::vector<ColumnDesc>& columns, const std::uint64_t row_count,
                      const bool big_endian, const std::string_view dm_name) {
    big_endian_ = big_endian;
    dm_name_ = dm_name;
    row_count_ = row_count;
    columns_.clear();

    for (const auto& cd : columns) {
        WriterColumnInfo info;
        info.name = cd.name;
        info.data_type = cd.data_type;
        info.row_values.resize(static_cast<std::size_t>(row_count));
        columns_.push_back(std::move(info));
    }

    is_setup_ = true;
}

void IsmWriter::write_cell(const std::size_t col_index, const CellValue& value,
                           const std::uint64_t row) {
    if (!is_setup_) {
        throw std::runtime_error("IsmWriter not setup");
    }
    if (col_index >= columns_.size()) {
        throw std::runtime_error("ISM column index out of range");
    }
    if (row >= row_count_) {
        throw std::runtime_error("ISM row out of range");
    }
    columns_[col_index].row_values[static_cast<std::size_t>(row)] = value;
}

namespace {

/// Canonical ISM value size for a given DataType (Bool=1, Int/uInt/Float=4,
/// Double/Int64=8). Returns 0 for unsupported types.
// NOLINTBEGIN(bugprone-branch-clone)
[[nodiscard]] std::uint32_t ism_canonical_size(DataType dt) noexcept {
    switch (dt) {
    case DataType::tp_bool:
        return 1;
    case DataType::tp_int:
        return 4;
    case DataType::tp_uint:
        return 4;
    case DataType::tp_float:
        return 4;
    case DataType::tp_double:
        return 8;
    case DataType::tp_int64:
        return 8;
    case DataType::tp_complex:
        return 8;
    case DataType::tp_dcomplex:
        return 16;
    default:
        return 0;
    }
}
// NOLINTEND(bugprone-branch-clone)

/// Write a single ISM value at the current end of `buf`.
void write_ism_value(std::vector<std::uint8_t>& buf, const CellValue& value, DataType dtype) {
    std::uint8_t b[8];
    switch (dtype) {
    case DataType::tp_bool: {
        const auto* bv = std::get_if<bool>(&value);
        buf.push_back(static_cast<std::uint8_t>(bv != nullptr && *bv ? 1 : 0));
        break;
    }
    case DataType::tp_int: {
        const auto* iv = std::get_if<std::int32_t>(&value);
        write_le_i32_buf(b, iv != nullptr ? *iv : 0);
        buf.insert(buf.end(), b, b + 4);
        break;
    }
    case DataType::tp_uint: {
        const auto* uv = std::get_if<std::uint32_t>(&value);
        write_le_u32_buf(b, uv != nullptr ? *uv : 0U);
        buf.insert(buf.end(), b, b + 4);
        break;
    }
    case DataType::tp_float: {
        const auto* fv = std::get_if<float>(&value);
        float val = fv != nullptr ? *fv : 0.0F;
        std::memcpy(b, &val, 4);
        buf.insert(buf.end(), b, b + 4);
        break;
    }
    case DataType::tp_double: {
        const auto* dv = std::get_if<double>(&value);
        double val = dv != nullptr ? *dv : 0.0;
        std::memcpy(b, &val, 8);
        buf.insert(buf.end(), b, b + 8);
        break;
    }
    case DataType::tp_int64: {
        const auto* lv = std::get_if<std::int64_t>(&value);
        std::int64_t val = lv != nullptr ? *lv : 0;
        std::uint8_t b8[8];
        std::memcpy(b8, &val, 8);
        buf.insert(buf.end(), b8, b8 + 8);
        break;
    }
    case DataType::tp_complex: {
        const auto* cv = std::get_if<std::complex<float>>(&value);
        float re = cv != nullptr ? cv->real() : 0.0F;
        float im = cv != nullptr ? cv->imag() : 0.0F;
        std::memcpy(b, &re, 4);
        std::memcpy(b + 4, &im, 4);
        buf.insert(buf.end(), b, b + 8);
        break;
    }
    case DataType::tp_dcomplex: {
        const auto* dcv = std::get_if<std::complex<double>>(&value);
        double re = dcv != nullptr ? dcv->real() : 0.0;
        double im = dcv != nullptr ? dcv->imag() : 0.0;
        std::uint8_t b16[16];
        std::memcpy(b16, &re, 8);
        std::memcpy(b16 + 8, &im, 8);
        buf.insert(buf.end(), b16, b16 + 16);
        break;
    }
    default:
        throw std::runtime_error("unsupported ISM write type");
    }
}

} // namespace

std::vector<std::uint8_t> IsmWriter::flush() const {
    if (!is_setup_) {
        throw std::runtime_error("IsmWriter not setup");
    }

    const auto nr_columns = static_cast<std::uint32_t>(columns_.size());

    // Compute per-column canonical sizes.
    std::vector<std::uint32_t> col_sizes(nr_columns);
    for (std::uint32_t c = 0; c < nr_columns; ++c) {
        col_sizes[c] = ism_canonical_size(columns_[c].data_type);
        if (col_sizes[c] == 0) {
            throw std::runtime_error("unsupported ISM column type");
        }
    }

    // Compute per-column offset within an entry record.
    std::vector<std::uint32_t> col_offsets(nr_columns);
    {
        std::uint32_t off = 0;
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            col_offsets[c] = off;
            off += col_sizes[c];
        }
    }

    // Build per-column incremental entries (collapse consecutive identical values).
    // Each entry stores the row number and the data offset (relative to data origin).
    struct ColEntry {
        std::uint32_t row = 0;
        std::uint32_t data_offset = 0; // relative to data origin (bucket + 4)
    };
    std::vector<std::vector<ColEntry>> col_entries(nr_columns);

    // Write packed entry records into a data buffer.
    // Each ISM entry contains ALL column values for that row concatenated.
    // We only store a new entry when at least one column's value changes.
    // For simplicity (and casacore compatibility), store per-column entries
    // independently -- each column gets its own entry when its value changes.
    std::vector<std::uint8_t> data_buf;

    // Since every column value is in the same packed record, we write full
    // entry records. The approach: walk rows and write a new entry record
    // whenever any column value changes. Then each column's index points
    // into the entry that contains its value.
    //
    // However, looking at the casacore fixture, every row is a separate entry
    // even when values don't change (because at least one column changes each row).
    // We store one packed entry per unique row that has at least one changed value.

    std::vector<bool> entry_written(static_cast<std::size_t>(row_count_), false);

    // First pass: determine which rows need entries (any column changes).
    for (std::uint64_t r = 0; r < row_count_; ++r) {
        if (r == 0) {
            entry_written[0] = true;
            continue;
        }
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            const auto& prev = columns_[c].row_values[static_cast<std::size_t>(r - 1)];
            const auto& cur = columns_[c].row_values[static_cast<std::size_t>(r)];
            if (cur != prev) {
                entry_written[static_cast<std::size_t>(r)] = true;
                break;
            }
        }
    }

    // Second pass: write entry records and build column indices.
    // Track the "current entry offset" for each column (last offset where that
    // column's value was written).
    std::vector<std::uint32_t> last_entry_offset(nr_columns, 0);
    bool first = true;

    for (std::uint64_t r = 0; r < row_count_; ++r) {
        if (!entry_written[static_cast<std::size_t>(r)]) {
            continue;
        }

        // Write a packed entry record.
        const auto entry_data_offset = static_cast<std::uint32_t>(data_buf.size());
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            write_ism_value(data_buf, columns_[c].row_values[static_cast<std::size_t>(r)],
                            columns_[c].data_type);
        }

        // For each column, record this entry if the value changed (or first row).
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            bool store = first;
            if (!store) {
                // Check if this column's value changed from previous entry.
                const auto& prev_entries = col_entries[c];
                if (!prev_entries.empty()) {
                    const auto prev_row = prev_entries.back().row;
                    const auto& prev_val = columns_[c].row_values[prev_row];
                    const auto& cur_val = columns_[c].row_values[static_cast<std::size_t>(r)];
                    if (cur_val != prev_val) {
                        store = true;
                    }
                }
            }
            if (store) {
                ColEntry ce;
                ce.row = static_cast<std::uint32_t>(r);
                ce.data_offset = entry_data_offset + col_offsets[c];
                col_entries[c].push_back(ce);
            }
            last_entry_offset[c] = entry_data_offset + col_offsets[c];
        }
        first = false;
    }

    // Use a standard bucket size (32768 like casacore default).
    constexpr std::uint32_t kBucketSize = 32768;

    // Distribute entries across multiple buckets when total exceeds capacity.
    // Each bucket needs: 4 (free_offset) + data bytes + index bytes.
    // We greedily pack entry records into buckets.

    // Compute per-entry total record size (sum of all column canonical sizes).
    std::uint32_t entry_record_size = 0;
    for (std::uint32_t c = 0; c < nr_columns; ++c) {
        entry_record_size += col_sizes[c];
    }

    // Per-entry index overhead: for each column that has an entry at this row,
    // we need 2 u32s (row_number + data_offset). In the worst case all columns
    // have entries at each packed row, so overhead per entry = nr_columns * 8.
    // Plus per-column nr_entries u32 (4 bytes each) once per bucket.
    const std::uint32_t per_col_index_fixed = 4;          // nr_entries u32
    const std::uint32_t per_entry_index = nr_columns * 8; // row + offset per col

    // Collect all entry rows in order.
    std::vector<std::uint64_t> entry_rows;
    for (std::uint64_t r = 0; r < row_count_; ++r) {
        if (entry_written[static_cast<std::size_t>(r)]) {
            entry_rows.push_back(r);
        }
    }

    // Distribute entries to buckets.
    struct BucketPlan {
        std::size_t first_entry_idx = 0;
        std::size_t entry_count = 0;
    };
    std::vector<BucketPlan> bucket_plans;

    {
        std::size_t cur_start = 0;
        while (cur_start < entry_rows.size()) {
            // Determine how many entries fit in this bucket.
            const std::uint32_t index_fixed = nr_columns * per_col_index_fixed;
            std::uint32_t used = 4 + index_fixed; // free_offset + index headers
            std::size_t count = 0;

            for (std::size_t i = cur_start; i < entry_rows.size(); ++i) {
                // Upper-bound: assume all columns get an entry per packed row.
                const auto entry_total = entry_record_size + per_entry_index;
                if (used + entry_total > kBucketSize && count > 0) {
                    break;
                }
                used += entry_total;
                ++count;
            }

            BucketPlan bp;
            bp.first_entry_idx = cur_start;
            bp.entry_count = count;
            bucket_plans.push_back(bp);
            cur_start += count;
        }
    }

    const auto nr_buckets = static_cast<std::uint32_t>(bucket_plans.size());

    // Build all buckets.
    std::vector<std::vector<std::uint8_t>> buckets;
    // Track per-bucket row boundaries for the ISM index.
    std::vector<std::uint32_t> bucket_start_rows;

    for (const auto& bp : bucket_plans) {
        std::vector<std::uint8_t> bkt_data_buf;
        std::vector<std::vector<ColEntry>> bkt_col_entries(nr_columns);

        bucket_start_rows.push_back(static_cast<std::uint32_t>(entry_rows[bp.first_entry_idx]));

        for (std::size_t ei = bp.first_entry_idx; ei < bp.first_entry_idx + bp.entry_count; ++ei) {
            const auto r = entry_rows[ei];

            // Write packed entry record.
            const auto entry_data_offset = static_cast<std::uint32_t>(bkt_data_buf.size());
            for (std::uint32_t c = 0; c < nr_columns; ++c) {
                write_ism_value(bkt_data_buf, columns_[c].row_values[static_cast<std::size_t>(r)],
                                columns_[c].data_type);
            }

            // Record per-column entries.
            for (std::uint32_t c = 0; c < nr_columns; ++c) {
                // Check if this column needs a new entry at this row.
                for (const auto& ce : col_entries[c]) {
                    if (ce.row == static_cast<std::uint32_t>(r)) {
                        ColEntry bce;
                        bce.row = ce.row;
                        bce.data_offset = entry_data_offset + col_offsets[c];
                        bkt_col_entries[c].push_back(bce);
                        break;
                    }
                }
            }
        }

        // Compute bucket data size.
        const auto bkt_data_size = static_cast<std::uint32_t>(bkt_data_buf.size());

        // Build bucket.
        std::vector<std::uint8_t> bucket(kBucketSize, 0);
        write_le_u32_buf(bucket.data(), 4 + bkt_data_size);
        std::memcpy(bucket.data() + 4, bkt_data_buf.data(), bkt_data_buf.size());

        std::size_t bkt_idx_pos = 4 + bkt_data_size;
        for (std::uint32_t c = 0; c < nr_columns; ++c) {
            const auto& entries = bkt_col_entries[c];
            const auto n = static_cast<std::uint32_t>(entries.size());
            write_le_u32_buf(bucket.data() + bkt_idx_pos, n);
            bkt_idx_pos += 4;
            for (std::uint32_t i = 0; i < n; ++i) {
                write_le_u32_buf(bucket.data() + bkt_idx_pos, entries[i].row);
                bkt_idx_pos += 4;
            }
            for (std::uint32_t i = 0; i < n; ++i) {
                write_le_u32_buf(bucket.data() + bkt_idx_pos, entries[i].data_offset);
                bkt_idx_pos += 4;
            }
        }

        buckets.push_back(std::move(bucket));
    }

    // Build file header (512 bytes).
    std::vector<std::uint8_t> header(512, 0);
    {
        // ISM header: AipsIO "IncrementalStMan" version 5.
        std::vector<std::uint8_t> hdr_buf;
        // Write AipsIO magic + placeholder objlen + type string + version.
        // Magic.
        write_le_u32_buf(header.data(), kAipsIoMagic);
        std::size_t off = 4;
        // Object length placeholder (will patch).
        const auto objlen_pos = off;
        write_le_u32_buf(header.data() + off, 0);
        off += 4;
        // Type string: "IncrementalStMan".
        const std::string_view type_str = "IncrementalStMan";
        write_le_u32_buf(header.data() + off, static_cast<std::uint32_t>(type_str.size()));
        off += 4;
        std::memcpy(header.data() + off, type_str.data(), type_str.size());
        off += type_str.size();
        // Version.
        write_le_u32_buf(header.data() + off, 5);
        off += 4;
        // Endian flag (version >= 5).
        header[off] = 0; // LE
        off += 1;
        // bucket_size.
        write_le_u32_buf(header.data() + off, kBucketSize);
        off += 4;
        // nr_buckets.
        write_le_u32_buf(header.data() + off, nr_buckets);
        off += 4;
        // pers_cache_size = 1.
        write_le_u32_buf(header.data() + off, 1);
        off += 4;
        // free_buckets_nr = 0.
        write_le_u32_buf(header.data() + off, 0);
        off += 4;
        // first_free_bucket = -1.
        write_le_i32_buf(header.data() + off, -1);
        off += 4;
        // unique_nr_row = 0.
        write_le_u32_buf(header.data() + off, 0);
        off += 4;

        // Patch object length.
        // casacore's object length includes the length field itself.
        write_le_u32_buf(header.data() + objlen_pos, static_cast<std::uint32_t>(off - objlen_pos));
    }

    // Build ISM index (ISMIndex::put format, LE).
    // casacore expects the index after the last bucket at offset 512 + nbuckets*bucketSize.
    std::vector<std::uint8_t> index_buf;
    {
        auto append_le_u32 = [&](std::uint32_t v) {
            std::uint8_t b[4];
            write_le_u32_buf(b, v);
            index_buf.insert(index_buf.end(), b, b + 4);
        };

        // AipsIO object header: ISMIndex version 1.
        append_le_u32(kAipsIoMagic);
        const auto objlen_pos = index_buf.size();
        append_le_u32(0); // placeholder for object length
        // Type string "ISMIndex".
        const std::string_view idx_type = "ISMIndex";
        append_le_u32(static_cast<std::uint32_t>(idx_type.size()));
        index_buf.insert(index_buf.end(), idx_type.begin(), idx_type.end());
        append_le_u32(1); // version

        // nused_p = nr_buckets.
        append_le_u32(nr_buckets);

        // Block<uInt> rows_p: (nr_buckets + 1) elements.
        // rows_p[i] = start row for bucket i, rows_p[nr_buckets] = row_count.
        {
            const auto block_pos = index_buf.size();
            append_le_u32(0); // placeholder for Block object length
            const std::string_view block_type = "Block";
            append_le_u32(static_cast<std::uint32_t>(block_type.size()));
            index_buf.insert(index_buf.end(), block_type.begin(), block_type.end());
            append_le_u32(1);              // Block version
            append_le_u32(nr_buckets + 1); // element count
            for (std::uint32_t i = 0; i < nr_buckets; ++i) {
                append_le_u32(bucket_start_rows[i]);
            }
            append_le_u32(static_cast<std::uint32_t>(row_count_));
            write_le_u32_buf(index_buf.data() + block_pos,
                             static_cast<std::uint32_t>(index_buf.size() - block_pos));
        }

        // Block<uInt> bucketNr_p: nr_buckets elements [0, 1, 2, ...].
        {
            const auto block_pos = index_buf.size();
            append_le_u32(0); // placeholder
            const std::string_view block_type = "Block";
            append_le_u32(static_cast<std::uint32_t>(block_type.size()));
            index_buf.insert(index_buf.end(), block_type.begin(), block_type.end());
            append_le_u32(1);          // Block version
            append_le_u32(nr_buckets); // element count
            for (std::uint32_t i = 0; i < nr_buckets; ++i) {
                append_le_u32(i);
            }
            write_le_u32_buf(index_buf.data() + block_pos,
                             static_cast<std::uint32_t>(index_buf.size() - block_pos));
        }

        // Patch ISMIndex object length (includes length field itself).
        write_le_u32_buf(index_buf.data() + objlen_pos,
                         static_cast<std::uint32_t>(index_buf.size() - objlen_pos));
    }

    // Assemble: header + all buckets + ISM index.
    std::vector<std::uint8_t> result;
    result.reserve(512 + static_cast<std::size_t>(nr_buckets) * kBucketSize + index_buf.size());
    result.insert(result.end(), header.begin(), header.end());
    for (const auto& bkt : buckets) {
        result.insert(result.end(), bkt.begin(), bkt.end());
    }
    result.insert(result.end(), index_buf.begin(), index_buf.end());

    return result;
}

std::vector<std::uint8_t> IsmWriter::make_blob() const {
    // ISM blob is a minimal placeholder. The actual ISM data is in the .f0 file.
    // The blob contains the ISM data manager info.
    AipsIoWriter writer;

    const auto header_start = writer.size();
    writer.write_object_header(0, "ISM", 1);
    writer.write_string(dm_name_);
    // Patch object length.
    writer.patch_u32(header_start + 4,
                     static_cast<std::uint32_t>(writer.size() - header_start - 4));

    return writer.take_bytes();
}

void IsmWriter::write_file(const std::string_view table_dir,
                           const std::uint32_t sequence_number) const {
    const auto data = flush();
    const auto path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sequence_number));
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot create ISM file: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

} // namespace casacore_mini
