#include "casacore_mini/standard_stman.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_writer.hpp"

#include <algorithm>
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

// --- Endian-aware primitive readers ---

[[nodiscard]] std::uint32_t read_le_u32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8U) |
           (static_cast<std::uint32_t>(p[2]) << 16U) | (static_cast<std::uint32_t>(p[3]) << 24U);
}

[[nodiscard]] std::int32_t read_le_i32(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_le_u32(p));
}

[[nodiscard]] std::uint64_t read_le_u64(const std::uint8_t* p) noexcept {
    return static_cast<std::uint64_t>(p[0]) | (static_cast<std::uint64_t>(p[1]) << 8U) |
           (static_cast<std::uint64_t>(p[2]) << 16U) | (static_cast<std::uint64_t>(p[3]) << 24U) |
           (static_cast<std::uint64_t>(p[4]) << 32U) | (static_cast<std::uint64_t>(p[5]) << 40U) |
           (static_cast<std::uint64_t>(p[6]) << 48U) | (static_cast<std::uint64_t>(p[7]) << 56U);
}

[[nodiscard]] float read_le_f32(const std::uint8_t* p) noexcept {
    float v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

[[nodiscard]] double read_le_f64(const std::uint8_t* p) noexcept {
    double v = 0;
    std::memcpy(&v, p, 8);
    return v;
}

[[nodiscard]] std::uint32_t read_be_u32(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint32_t>(p[0]) << 24U) | (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) | static_cast<std::uint32_t>(p[3]);
}

[[nodiscard]] std::int32_t read_be_i32(const std::uint8_t* p) noexcept {
    return static_cast<std::int32_t>(read_be_u32(p));
}

[[nodiscard]] float read_be_f32(const std::uint8_t* p) noexcept {
    const auto bits = read_be_u32(p);
    float v = 0;
    std::memcpy(&v, &bits, 4);
    return v;
}

[[nodiscard]] std::uint64_t read_be_u64(const std::uint8_t* p) noexcept {
    return (static_cast<std::uint64_t>(p[0]) << 56U) | (static_cast<std::uint64_t>(p[1]) << 48U) |
           (static_cast<std::uint64_t>(p[2]) << 40U) | (static_cast<std::uint64_t>(p[3]) << 32U) |
           (static_cast<std::uint64_t>(p[4]) << 24U) | (static_cast<std::uint64_t>(p[5]) << 16U) |
           (static_cast<std::uint64_t>(p[6]) << 8U) | static_cast<std::uint64_t>(p[7]);
}

[[nodiscard]] double read_be_f64(const std::uint8_t* p) noexcept {
    const auto bits = read_be_u64(p);
    double v = 0;
    std::memcpy(&v, &bits, 8);
    return v;
}

// --- Endian-dispatch helpers ---

[[nodiscard]] std::uint32_t read_endian_u32(const std::uint8_t* p, bool be) noexcept {
    return be ? read_be_u32(p) : read_le_u32(p);
}

[[nodiscard]] std::int32_t read_endian_i32(const std::uint8_t* p, bool be) noexcept {
    return be ? read_be_i32(p) : read_le_i32(p);
}

[[nodiscard]] std::uint64_t read_endian_u64(const std::uint8_t* p, bool be) noexcept {
    return be ? read_be_u64(p) : read_le_u64(p);
}

// --- Canonical type sizes (same for LE and BE on all supported platforms) ---

// NOLINTBEGIN(bugprone-branch-clone)
[[nodiscard]] std::uint32_t canonical_type_size(DataType dt) {
    switch (dt) {
    case DataType::tp_bool:
        return 0; // bits, not bytes
    case DataType::tp_char:
        return 1;
    case DataType::tp_uchar:
        return 1;
    case DataType::tp_short:
        return 2;
    case DataType::tp_ushort:
        return 2;
    case DataType::tp_int:
        return 4;
    case DataType::tp_uint:
        return 4;
    case DataType::tp_float:
        return 4;
    case DataType::tp_double:
        return 8;
    case DataType::tp_complex:
        return 8;
    case DataType::tp_dcomplex:
        return 16;
    case DataType::tp_string:
        return 12; // 3 * 4 bytes (indirect storage)
    case DataType::tp_int64:
        return 8;
    default:
        throw std::runtime_error("unsupported data type for SSM");
    }
}
// NOLINTEND(bugprone-branch-clone)

// --- File I/O helper ---

[[nodiscard]] std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// --- EndianAipsIoReader: reads AipsIO streams in the table's byte order ---

class EndianAipsIoReader {
  public:
    EndianAipsIoReader(std::span<const std::uint8_t> bytes, bool big_endian)
        : bytes_(bytes), be_(big_endian) {}

    [[nodiscard]] std::size_t remaining() const noexcept {
        return offset_ < bytes_.size() ? bytes_.size() - offset_ : 0;
    }

    void skip(std::size_t n) {
        if (n > remaining()) {
            throw std::runtime_error("EndianAipsIoReader: skip past end");
        }
        offset_ += n;
    }

    [[nodiscard]] std::uint8_t read_u8() {
        ensure(1);
        return bytes_[offset_++];
    }

    [[nodiscard]] std::uint32_t read_u32() {
        ensure(4);
        const auto v = read_endian_u32(bytes_.data() + offset_, be_);
        offset_ += 4;
        return v;
    }

    [[nodiscard]] std::int32_t read_i32() {
        ensure(4);
        const auto v = read_endian_i32(bytes_.data() + offset_, be_);
        offset_ += 4;
        return v;
    }

    [[nodiscard]] std::uint64_t read_u64() {
        ensure(8);
        const auto v = read_endian_u64(bytes_.data() + offset_, be_);
        offset_ += 8;
        return v;
    }

    [[nodiscard]] std::string read_string() {
        const auto len = read_u32();
        ensure(len);
        std::string s(reinterpret_cast<const char*>(bytes_.data() + offset_), len);
        offset_ += len;
        return s;
    }

    struct ObjHeader {
        std::string type;
        std::uint32_t version = 0;
    };

    [[nodiscard]] ObjHeader read_object_header() {
        const auto magic = read_u32();
        if (magic != kAipsIoMagic) {
            throw std::runtime_error("bad AipsIO magic in SSM stream");
        }
        static_cast<void>(read_u32()); // object length
        ObjHeader h;
        h.type = read_string();
        h.version = read_u32();
        return h;
    }

    /// AipsIO getend() reads no bytes — it only validates byte counts.
    static void read_object_end() {}

    /// Read a Block<uInt>: objlen + "Block" + version + count + data.
    /// AipsIO getend() validates byte counts but reads no bytes.
    [[nodiscard]] std::vector<std::uint32_t> read_block_u32() {
        static_cast<void>(read_u32());    // object length
        static_cast<void>(read_string()); // "Block"
        static_cast<void>(read_u32());    // version
        const auto count = read_u32();
        std::vector<std::uint32_t> result(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            result[i] = read_u32();
        }
        return result;
    }

    /// Read a Block<Int64>: objlen + "Block" + version + count + data.
    [[nodiscard]] std::vector<std::uint64_t> read_block_u64() {
        static_cast<void>(read_u32());    // object length
        static_cast<void>(read_string()); // "Block"
        static_cast<void>(read_u32());    // version
        const auto count = read_u32();
        std::vector<std::uint64_t> result(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            result[i] = read_u64();
        }
        return result;
    }

  private:
    void ensure(std::size_t n) const {
        if (n > remaining()) {
            throw std::runtime_error("EndianAipsIoReader: truncated input");
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
    bool be_ = false;
};

// --- SSM string cell reader ---

[[nodiscard]] std::string read_ssm_string_cell(const std::uint8_t* cell_ptr,
                                               const std::vector<std::uint8_t>& bucket_data,
                                               std::uint32_t bucket_size, bool be) {
    // The 3 canonical Ints: bucket_nr, offset, length.
    const auto str_length = read_endian_i32(cell_ptr + 8, be);

    if (str_length <= 0) {
        return {};
    }

    if (str_length <= 8) {
        // Short string: data is stored inline starting at cell_ptr.
        return std::string(reinterpret_cast<const char*>(cell_ptr),
                           static_cast<std::size_t>(str_length));
    }

    // Long string: stored in a string bucket.
    const auto str_bucket_nr = read_endian_i32(cell_ptr, be);
    const auto str_offset = read_endian_i32(cell_ptr + 4, be);

    constexpr std::uint32_t kStrBucketHeader = 16; // 4 canonical ints
    auto bucket_byte_offset = static_cast<std::size_t>(str_bucket_nr) * bucket_size;
    if (bucket_byte_offset + bucket_size > bucket_data.size()) {
        throw std::runtime_error("SSM string bucket out of range");
    }

    const std::uint8_t* sbkt = bucket_data.data() + bucket_byte_offset;
    const auto data_area = bucket_size - kStrBucketHeader;
    if (static_cast<std::uint32_t>(str_offset + str_length) > data_area) {
        throw std::runtime_error("SSM string data out of range");
    }

    return std::string(reinterpret_cast<const char*>(sbkt + kStrBucketHeader + str_offset),
                       static_cast<std::size_t>(str_length));
}

} // namespace

// --- Public API implementations ---

SsmTableDatBlob parse_ssm_blob(const std::span<const std::uint8_t> blob) {
    // The SSM blob is big-endian AipsIO (from the table.dat stream).
    AipsIoReader reader(blob);

    // Embedded object header: may have magic prefix or just length.
    const auto first = reader.read_u32();
    std::string obj_type;
    if (first == kAipsIoMagic) {
        static_cast<void>(reader.read_u32()); // object length
        obj_type = reader.read_string();
        static_cast<void>(reader.read_u32()); // version
    } else {
        // first was object length
        obj_type = reader.read_string();
        static_cast<void>(reader.read_u32()); // version
    }

    if (obj_type != "SSM") {
        throw std::runtime_error("expected SSM blob, got: " + obj_type);
    }

    SsmTableDatBlob result;
    result.data_man_name = reader.read_string();

    // Block<uInt> itsColumnOffset.
    // AipsIO format: objlen + "Block" + version + count + data.
    // getend() reads no bytes (just validates byte counts).
    {
        static_cast<void>(reader.read_u32()); // object length
        const auto blk_type = reader.read_string();
        if (blk_type != "Block") {
            throw std::runtime_error("expected Block, got: " + blk_type);
        }
        static_cast<void>(reader.read_u32()); // version
        const auto co_count = reader.read_u32();
        result.column_offset.resize(co_count);
        for (std::uint32_t i = 0; i < co_count; ++i) {
            result.column_offset[i] = reader.read_u32();
        }
    }

    // Block<uInt> itsColIndexMap.
    {
        static_cast<void>(reader.read_u32()); // object length
        const auto blk_type = reader.read_string();
        if (blk_type != "Block") {
            throw std::runtime_error("expected Block, got: " + blk_type);
        }
        static_cast<void>(reader.read_u32()); // version
        const auto ci_count = reader.read_u32();
        result.col_index_map.resize(ci_count);
        for (std::uint32_t i = 0; i < ci_count; ++i) {
            result.col_index_map[i] = reader.read_u32();
        }
    }

    return result;
}

SsmFileHeader parse_ssm_file_header(const std::span<const std::uint8_t> header_bytes,
                                    const bool big_endian) {
    if (header_bytes.size() < 512) {
        throw std::runtime_error("SSM file header too short (< 512 bytes)");
    }

    EndianAipsIoReader reader(header_bytes, big_endian);
    const auto hdr = reader.read_object_header();
    if (hdr.type != "StandardStMan") {
        throw std::runtime_error("expected StandardStMan header, got: " + hdr.type);
    }

    SsmFileHeader result;

    if (hdr.version >= 3) {
        const auto be_flag = reader.read_u8();
        if ((be_flag != 0) != big_endian) {
            throw std::runtime_error("SSM endian flag mismatch");
        }
    }

    result.bucket_size = reader.read_u32();
    result.nr_buckets = reader.read_u32();
    result.pers_cache_size = reader.read_u32();
    result.free_buckets_nr = reader.read_u32();
    result.first_free_bucket = reader.read_i32();
    result.nr_idx_buckets = reader.read_u32();
    result.first_idx_bucket = reader.read_i32();

    if (hdr.version >= 2) {
        result.idx_bucket_offset = reader.read_u32();
    }

    result.last_string_bucket = reader.read_i32();
    result.index_length = reader.read_u32();
    result.nr_indices = reader.read_u32();

    return result;
}

std::vector<SsmIndex> parse_ssm_indices(const std::span<const std::uint8_t> index_data,
                                        const std::uint32_t nr_indices, const bool big_endian) {
    EndianAipsIoReader reader(index_data, big_endian);
    std::vector<SsmIndex> indices;
    indices.reserve(nr_indices);

    for (std::uint32_t idx = 0; idx < nr_indices; ++idx) {
        const auto hdr = reader.read_object_header();
        if (hdr.type != "SSMIndex") {
            throw std::runtime_error("expected SSMIndex, got: " + hdr.type);
        }

        SsmIndex si;
        const auto n_used = reader.read_u32();
        si.rows_per_bucket = reader.read_u32();
        si.nr_columns = reader.read_i32();

        // Free space map: serialized as "SimpleOrderedMap" AipsIO object.
        // Format: objlen + "SimpleOrderedMap" + version + default_value(i32)
        //         + count(u32) + incr(u32) + count * (key(i32), value(i32)).
        // getend() reads no bytes.
        {
            static_cast<void>(reader.read_u32());    // object length
            static_cast<void>(reader.read_string()); // "SimpleOrderedMap"
            static_cast<void>(reader.read_u32());    // version
            static_cast<void>(reader.read_i32());    // default value
            const auto fs_count = reader.read_u32();
            static_cast<void>(reader.read_u32()); // incr
            for (std::uint32_t fi = 0; fi < fs_count; ++fi) {
                static_cast<void>(reader.read_i32()); // key
                static_cast<void>(reader.read_i32()); // value
            }
        }

        if (hdr.version == 1) {
            auto tmp = reader.read_block_u32();
            si.last_row.resize(tmp.size());
            for (std::size_t i = 0; i < tmp.size(); ++i) {
                si.last_row[i] = tmp[i];
            }
        } else {
            si.last_row = reader.read_block_u64();
        }

        si.bucket_number = reader.read_block_u32();

        if (si.last_row.size() > n_used) {
            si.last_row.resize(n_used);
        }
        if (si.bucket_number.size() > n_used) {
            si.bucket_number.resize(n_used);
        }

        reader.read_object_end();
        indices.push_back(std::move(si));
    }

    return indices;
}

// --- SsmReader ---

void SsmReader::open(const std::string_view table_dir, const std::size_t sm_index,
                     const TableDatFull& table_dat) {
    table_dat_ = &table_dat;
    big_endian_ = table_dat.big_endian;
    row_count_ = table_dat.row_count;

    if (sm_index >= table_dat.storage_managers.size()) {
        throw std::runtime_error("SM index out of range");
    }
    const auto& sm = table_dat.storage_managers[sm_index];
    if (sm.type_name != "StandardStMan") {
        throw std::runtime_error("not a StandardStMan: " + sm.type_name);
    }

    blob_ = parse_ssm_blob(sm.data_blob);

    const auto f0_path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sm.sequence_number));
    auto file_data = read_file(f0_path);
    if (file_data.size() < 512) {
        throw std::runtime_error("SSM file too small: " + f0_path.string());
    }

    file_header_ =
        parse_ssm_file_header(std::span<const std::uint8_t>(file_data.data(), 512), big_endian_);

    constexpr std::size_t kHeaderSize = 512;
    if (file_data.size() > kHeaderSize) {
        bucket_data_.assign(file_data.begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
                            file_data.end());
    }

    // Assemble index data from index buckets.
    constexpr std::uint32_t kCanonIntSize = 4;
    constexpr std::uint32_t kIdxLinkSize = 2 * kCanonIntSize;
    const std::uint32_t idx_data_per_bucket = file_header_.bucket_size - kIdxLinkSize;

    std::vector<std::uint8_t> assembled_index;
    assembled_index.reserve(file_header_.index_length);

    std::int32_t bkt_nr = file_header_.first_idx_bucket;
    std::uint32_t bytes_left = file_header_.index_length;

    for (std::uint32_t j = 0; j < file_header_.nr_idx_buckets && bytes_left > 0; ++j) {
        if (bkt_nr < 0) {
            throw std::runtime_error("invalid index bucket number");
        }
        const auto bkt_off = static_cast<std::size_t>(bkt_nr) * file_header_.bucket_size;
        if (bkt_off + file_header_.bucket_size > bucket_data_.size()) {
            throw std::runtime_error("index bucket out of range");
        }
        const std::uint8_t* bp = bucket_data_.data() + bkt_off;

        if (file_header_.idx_bucket_offset > 0) {
            const auto n =
                std::min(bytes_left, file_header_.bucket_size - file_header_.idx_bucket_offset);
            assembled_index.insert(assembled_index.end(), bp + file_header_.idx_bucket_offset,
                                   bp + file_header_.idx_bucket_offset + n);
            bytes_left -= n;
        } else {
            const auto n = std::min(bytes_left, idx_data_per_bucket);
            assembled_index.insert(assembled_index.end(), bp + kIdxLinkSize, bp + kIdxLinkSize + n);
            bytes_left -= n;
            // Next bucket number is at offset kCanonIntSize in the link header.
            bkt_nr = read_endian_i32(bp + kCanonIntSize, big_endian_);
        }
    }

    indices_ = parse_ssm_indices(assembled_index, file_header_.nr_indices, big_endian_);

    // Build column metadata for columns belonging to this SM.
    columns_.clear();
    for (const auto& cs : table_dat.column_setups) {
        if (cs.sequence_number != sm.sequence_number) {
            continue;
        }

        ColumnInfo info;
        info.name = cs.column_name;

        const auto& cols = table_dat.table_desc.columns;
        auto it = std::find_if(cols.begin(), cols.end(),
                               [&](const ColumnDesc& cd) { return cd.name == cs.column_name; });
        if (it == cols.end()) {
            throw std::runtime_error("column not in table desc: " + cs.column_name);
        }
        info.data_type = it->data_type;

        const auto sm_col = columns_.size();
        if (sm_col >= blob_.column_offset.size() || sm_col >= blob_.col_index_map.size()) {
            throw std::runtime_error("SM column metadata out of range: " + cs.column_name);
        }
        info.col_offset = blob_.column_offset[sm_col];
        info.index_nr = blob_.col_index_map[sm_col];
        info.ext_size = canonical_type_size(info.data_type);

        columns_.push_back(std::move(info));
    }

    is_open_ = true;
}

CellValue SsmReader::read_cell(const std::string_view col_name, const std::uint64_t row) const {
    if (!is_open_) {
        throw std::runtime_error("SsmReader not open");
    }

    const auto ci = find_column(col_name);
    const auto& col = columns_[ci];

    if (col.index_nr >= indices_.size()) {
        throw std::runtime_error("index out of range for column: " + col.name);
    }
    const auto lookup = find_bucket(indices_[col.index_nr], row);

    const auto bkt_off = static_cast<std::size_t>(lookup.bucket_nr) * file_header_.bucket_size;
    if (bkt_off + file_header_.bucket_size > bucket_data_.size()) {
        throw std::runtime_error("data bucket out of range");
    }

    const std::uint8_t* bp = bucket_data_.data() + bkt_off;
    const auto row_in_bkt = row - lookup.start_row;

    if (col.data_type == DataType::tp_bool) {
        const std::uint8_t* base = bp + col.col_offset;
        return static_cast<bool>((base[row_in_bkt / 8] >> (row_in_bkt % 8)) & 1U);
    }

    if (col.data_type == DataType::tp_string) {
        const std::uint8_t* cp = bp + col.col_offset + row_in_bkt * col.ext_size;
        return read_ssm_string_cell(cp, bucket_data_, file_header_.bucket_size, big_endian_);
    }

    const std::uint8_t* cp = bp + col.col_offset + row_in_bkt * col.ext_size;

    if (big_endian_) {
        switch (col.data_type) {
        case DataType::tp_int:
            return read_be_i32(cp);
        case DataType::tp_uint:
            return static_cast<std::uint32_t>(read_be_u32(cp));
        case DataType::tp_int64:
            return static_cast<std::int64_t>(read_be_u64(cp));
        case DataType::tp_float:
            return read_be_f32(cp);
        case DataType::tp_double:
            return read_be_f64(cp);
        default:
            throw std::runtime_error("unsupported data type for SSM read");
        }
    }

    switch (col.data_type) {
    case DataType::tp_int:
        return read_le_i32(cp);
    case DataType::tp_uint:
        return static_cast<std::uint32_t>(read_le_u32(cp));
    case DataType::tp_int64:
        return static_cast<std::int64_t>(read_le_u64(cp));
    case DataType::tp_float:
        return read_le_f32(cp);
    case DataType::tp_double:
        return read_le_f64(cp);
    default:
        throw std::runtime_error("unsupported data type for SSM read");
    }
}

std::size_t SsmReader::column_count() const noexcept {
    return columns_.size();
}

bool SsmReader::is_open() const noexcept {
    return is_open_;
}

std::size_t SsmReader::find_column(const std::string_view col_name) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == col_name) {
            return i;
        }
    }
    throw std::runtime_error("column not found in SSM: " + std::string(col_name));
}

SsmReader::BucketLookup SsmReader::find_bucket(const SsmIndex& index,
                                               const std::uint64_t row) const {
    auto it = std::lower_bound(index.last_row.begin(), index.last_row.end(), row);
    if (it == index.last_row.end()) {
        throw std::runtime_error("row " + std::to_string(row) + " out of range for SSM index");
    }
    const auto i = static_cast<std::size_t>(it - index.last_row.begin());
    BucketLookup result;
    result.bucket_nr = index.bucket_number[i];
    result.end_row = index.last_row[i];
    result.start_row = (i > 0) ? (index.last_row[i - 1] + 1) : 0;
    return result;
}

std::uint32_t SsmReader::external_size_bytes(const DataType dt, bool /*big_endian*/) {
    return canonical_type_size(dt);
}

// --- SsmWriter ---

namespace {

void write_le_u32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8U);
    p[2] = static_cast<std::uint8_t>(v >> 16U);
    p[3] = static_cast<std::uint8_t>(v >> 24U);
}

void write_le_i32(std::uint8_t* p, std::int32_t v) noexcept {
    write_le_u32(p, static_cast<std::uint32_t>(v));
}

void write_le_u64(std::uint8_t* p, std::uint64_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8U);
    p[2] = static_cast<std::uint8_t>(v >> 16U);
    p[3] = static_cast<std::uint8_t>(v >> 24U);
    p[4] = static_cast<std::uint8_t>(v >> 32U);
    p[5] = static_cast<std::uint8_t>(v >> 40U);
    p[6] = static_cast<std::uint8_t>(v >> 48U);
    p[7] = static_cast<std::uint8_t>(v >> 56U);
}

void write_le_f32(std::uint8_t* p, float v) noexcept {
    std::memcpy(p, &v, 4);
}

void write_le_f64(std::uint8_t* p, double v) noexcept {
    std::memcpy(p, &v, 8);
}

void write_be_u32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 24U);
    p[1] = static_cast<std::uint8_t>(v >> 16U);
    p[2] = static_cast<std::uint8_t>(v >> 8U);
    p[3] = static_cast<std::uint8_t>(v);
}

void write_be_i32(std::uint8_t* p, std::int32_t v) noexcept {
    write_be_u32(p, static_cast<std::uint32_t>(v));
}

void write_be_u64(std::uint8_t* p, std::uint64_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v >> 56U);
    p[1] = static_cast<std::uint8_t>(v >> 48U);
    p[2] = static_cast<std::uint8_t>(v >> 40U);
    p[3] = static_cast<std::uint8_t>(v >> 32U);
    p[4] = static_cast<std::uint8_t>(v >> 24U);
    p[5] = static_cast<std::uint8_t>(v >> 16U);
    p[6] = static_cast<std::uint8_t>(v >> 8U);
    p[7] = static_cast<std::uint8_t>(v);
}

void write_be_f32(std::uint8_t* p, float v) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, 4);
    write_be_u32(p, bits);
}

void write_be_f64(std::uint8_t* p, double v) noexcept {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, 8);
    write_be_u64(p, bits);
}

void write_endian_u32(std::uint8_t* p, std::uint32_t v, bool be) noexcept {
    be ? write_be_u32(p, v) : write_le_u32(p, v);
}

void write_endian_i32(std::uint8_t* p, std::int32_t v, bool be) noexcept {
    be ? write_be_i32(p, v) : write_le_i32(p, v);
}

void write_endian_u64(std::uint8_t* p, std::uint64_t v, bool be) noexcept {
    be ? write_be_u64(p, v) : write_le_u64(p, v);
}

/// EndianAipsIoWriter: writes AipsIO-style streams in the table's byte order.
class EndianAipsIoWriter {
  public:
    EndianAipsIoWriter(std::vector<std::uint8_t>& buffer, bool big_endian)
        : buffer_(buffer), be_(big_endian) {}

    [[nodiscard]] std::size_t position() const noexcept {
        return buffer_.size();
    }

    void write_u8(std::uint8_t v) {
        buffer_.push_back(v);
    }

    void write_u32(std::uint32_t v) {
        std::uint8_t b[4];
        write_endian_u32(b, v, be_);
        buffer_.insert(buffer_.end(), b, b + 4);
    }

    void write_i32(std::int32_t v) {
        std::uint8_t b[4];
        write_endian_i32(b, v, be_);
        buffer_.insert(buffer_.end(), b, b + 4);
    }

    void write_u64(std::uint64_t v) {
        std::uint8_t b[8];
        write_endian_u64(b, v, be_);
        buffer_.insert(buffer_.end(), b, b + 8);
    }

    void write_string(std::string_view s) {
        write_u32(static_cast<std::uint32_t>(s.size()));
        buffer_.insert(buffer_.end(), s.begin(), s.end());
    }

    void write_object_header(std::string_view type, std::uint32_t version) {
        write_u32(kAipsIoMagic);
        obj_len_pos_ = position();
        write_u32(0); // placeholder for object length
        write_string(type);
        write_u32(version);
    }

    void write_object_end() {
        // Patch the object length field.
        // casacore's putend stores objlen = position() - objlen_pos (includes the
        // 4-byte length field itself in the count).
        const auto obj_len = static_cast<std::uint32_t>(position() - obj_len_pos_);
        std::uint8_t b[4];
        write_endian_u32(b, obj_len, be_);
        std::memcpy(buffer_.data() + obj_len_pos_, b, 4);
    }

    void write_block_u32(const std::vector<std::uint32_t>& data) {
        // Block<uInt>: objlen + "Block" + version + count + data.
        const auto block_start = position();
        write_u32(0); // placeholder for object length
        write_string("Block");
        write_u32(1); // version
        write_u32(static_cast<std::uint32_t>(data.size()));
        for (const auto v : data) {
            write_u32(v);
        }
        // Patch block object length (includes length field itself, matching casacore).
        const auto block_len = static_cast<std::uint32_t>(position() - block_start);
        std::uint8_t b[4];
        write_endian_u32(b, block_len, be_);
        std::memcpy(buffer_.data() + block_start, b, 4);
    }

    void write_block_u64(const std::vector<std::uint64_t>& data) {
        const auto block_start = position();
        write_u32(0); // placeholder
        write_string("Block");
        write_u32(1);
        write_u32(static_cast<std::uint32_t>(data.size()));
        for (const auto v : data) {
            write_u64(v);
        }
        const auto block_len = static_cast<std::uint32_t>(position() - block_start);
        std::uint8_t b[4];
        write_endian_u32(b, block_len, be_);
        std::memcpy(buffer_.data() + block_start, b, 4);
    }

  private:
    std::vector<std::uint8_t>& buffer_;
    bool be_ = false;
    std::size_t obj_len_pos_ = 0;
};

} // namespace

void SsmWriter::setup(const std::vector<ColumnDesc>& columns, const std::uint64_t row_count,
                      const bool big_endian, const std::string_view dm_name) {
    big_endian_ = big_endian;
    dm_name_ = dm_name;
    row_count_ = row_count;

    // Compute column offsets and bucket size.
    // All columns go into a single index (one column group).
    std::uint32_t offset = 0;
    columns_.clear();
    for (const auto& cd : columns) {
        WriterColumnInfo info;
        info.name = cd.name;
        info.data_type = cd.data_type;
        info.ext_size = canonical_type_size(cd.data_type);
        info.col_offset = offset;

        // Allocate space for rows_per_bucket rows.
        // We compute rows_per_bucket below, so first accumulate per-row size.
        columns_.push_back(std::move(info));

        // Minimum 1 byte per row even for booleans.
        if (cd.data_type == DataType::tp_bool) {
            offset += 1; // will be adjusted below
        } else {
            offset += columns_.back().ext_size;
        }
    }

    // casacore uses a default bucket size that can hold about 32 rows.
    // Let's compute: bucket_size = per_row_bytes * 32, rounded up to be practical.
    constexpr std::uint32_t kTargetRowsPerBucket = 32;
    rows_per_bucket_ = kTargetRowsPerBucket;
    bucket_size_ = 0;
    for (const auto& ci : columns_) {
        if (ci.data_type == DataType::tp_bool) {
            // rows_per_bucket bits, rounded up to bytes.
            bucket_size_ += (rows_per_bucket_ + 7U) / 8U;
        } else {
            bucket_size_ += ci.ext_size * rows_per_bucket_;
        }
    }

    // Recompute column offsets for rows_per_bucket.
    offset = 0;
    for (auto& ci : columns_) {
        ci.col_offset = offset;
        if (ci.data_type == DataType::tp_bool) {
            offset += (rows_per_bucket_ + 7U) / 8U;
        } else {
            offset += ci.ext_size * rows_per_bucket_;
        }
    }

    // Compute number of data buckets needed.
    nr_data_buckets_ =
        (row_count_ > 0) ? static_cast<std::uint32_t>((row_count_ - 1) / rows_per_bucket_ + 1) : 0;

    // Allocate bucket data, zero-initialized.
    bucket_data_.assign(static_cast<std::size_t>(nr_data_buckets_) * bucket_size_, 0);

    last_string_bucket_ = -1;
    string_bucket_data_.clear();
    string_bucket_offset_ = 0;
    nr_string_buckets_ = 0;

    is_setup_ = true;
}

void SsmWriter::write_cell(const std::size_t col_index, const CellValue& value,
                           const std::uint64_t row) {
    if (!is_setup_) {
        throw std::runtime_error("SsmWriter not setup");
    }
    if (col_index >= columns_.size()) {
        throw std::runtime_error("column index out of range");
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto& col = columns_[col_index];
    const auto bucket_nr = static_cast<std::uint32_t>(row / rows_per_bucket_);
    const auto row_in_bucket = row % rows_per_bucket_;
    const auto bucket_offset = static_cast<std::size_t>(bucket_nr) * bucket_size_;

    std::uint8_t* bp = bucket_data_.data() + bucket_offset;

    if (col.data_type == DataType::tp_bool) {
        const auto* bv = std::get_if<bool>(&value);
        if (bv == nullptr) {
            throw std::runtime_error("type mismatch: expected bool");
        }
        std::uint8_t* base = bp + col.col_offset;
        if (*bv) {
            base[row_in_bucket / 8] |= static_cast<std::uint8_t>(1U << (row_in_bucket % 8));
        } else {
            base[row_in_bucket / 8] &= static_cast<std::uint8_t>(~(1U << (row_in_bucket % 8)));
        }
        return;
    }

    if (col.data_type == DataType::tp_string) {
        const auto* sv = std::get_if<std::string>(&value);
        if (sv == nullptr) {
            throw std::runtime_error("type mismatch: expected string");
        }
        std::uint8_t* cp = bp + col.col_offset + row_in_bucket * col.ext_size;
        const auto len = static_cast<std::int32_t>(sv->size());

        if (len <= 8) {
            // Short string: stored inline in the first 8 bytes.
            std::memset(cp, 0, 12);
            if (len > 0) {
                std::memcpy(cp, sv->data(), static_cast<std::size_t>(len));
            }
            write_endian_i32(cp + 8, len, big_endian_);
        } else {
            // Long string: allocate in string bucket.
            // For simplicity we allocate one string bucket.
            constexpr std::uint32_t kStrBucketHeader = 16;
            if (string_bucket_data_.empty()) {
                string_bucket_data_.assign(bucket_size_, 0);
                // Header: 4 canonical ints (next_bucket, data_used, deleted, next_deleted)
                write_endian_i32(string_bucket_data_.data(), -1, big_endian_);
                write_endian_i32(string_bucket_data_.data() + 4, 0, big_endian_);
                write_endian_i32(string_bucket_data_.data() + 8, 0, big_endian_);
                write_endian_i32(string_bucket_data_.data() + 12, -1, big_endian_);
                nr_string_buckets_ = 1;
                string_bucket_offset_ = 0;
            }
            const auto str_bucket_nr =
                static_cast<std::int32_t>(nr_data_buckets_); // string buckets follow data buckets
            last_string_bucket_ = str_bucket_nr;

            auto str_offset = static_cast<std::int32_t>(string_bucket_offset_);
            std::memcpy(string_bucket_data_.data() + kStrBucketHeader + string_bucket_offset_,
                        sv->data(), static_cast<std::size_t>(len));
            string_bucket_offset_ += static_cast<std::uint32_t>(len);

            write_endian_i32(cp, str_bucket_nr, big_endian_);
            write_endian_i32(cp + 4, str_offset, big_endian_);
            write_endian_i32(cp + 8, len, big_endian_);
        }
        return;
    }

    std::uint8_t* cp = bp + col.col_offset + row_in_bucket * col.ext_size;

    // NOLINTBEGIN(bugprone-branch-clone)
    if (big_endian_) {
        switch (col.data_type) {
        case DataType::tp_int: {
            const auto* iv = std::get_if<std::int32_t>(&value);
            if (iv == nullptr) {
                throw std::runtime_error("type mismatch: expected int");
            }
            write_be_i32(cp, *iv);
            break;
        }
        case DataType::tp_uint: {
            const auto* uv = std::get_if<std::uint32_t>(&value);
            if (uv == nullptr) {
                throw std::runtime_error("type mismatch: expected uint");
            }
            write_be_u32(cp, *uv);
            break;
        }
        case DataType::tp_int64: {
            const auto* lv = std::get_if<std::int64_t>(&value);
            if (lv == nullptr) {
                throw std::runtime_error("type mismatch: expected int64");
            }
            write_be_u64(cp, static_cast<std::uint64_t>(*lv));
            break;
        }
        case DataType::tp_float: {
            const auto* fv = std::get_if<float>(&value);
            if (fv == nullptr) {
                throw std::runtime_error("type mismatch: expected float");
            }
            write_be_f32(cp, *fv);
            break;
        }
        case DataType::tp_double: {
            const auto* dv = std::get_if<double>(&value);
            if (dv == nullptr) {
                throw std::runtime_error("type mismatch: expected double");
            }
            write_be_f64(cp, *dv);
            break;
        }
        default:
            throw std::runtime_error("unsupported data type for SSM write");
        }
    } else {
        switch (col.data_type) {
        case DataType::tp_int: {
            const auto* iv = std::get_if<std::int32_t>(&value);
            if (iv == nullptr) {
                throw std::runtime_error("type mismatch: expected int");
            }
            write_le_i32(cp, *iv);
            break;
        }
        case DataType::tp_uint: {
            const auto* uv = std::get_if<std::uint32_t>(&value);
            if (uv == nullptr) {
                throw std::runtime_error("type mismatch: expected uint");
            }
            write_le_u32(cp, *uv);
            break;
        }
        case DataType::tp_int64: {
            const auto* lv = std::get_if<std::int64_t>(&value);
            if (lv == nullptr) {
                throw std::runtime_error("type mismatch: expected int64");
            }
            write_le_u64(cp, static_cast<std::uint64_t>(*lv));
            break;
        }
        case DataType::tp_float: {
            const auto* fv = std::get_if<float>(&value);
            if (fv == nullptr) {
                throw std::runtime_error("type mismatch: expected float");
            }
            write_le_f32(cp, *fv);
            break;
        }
        case DataType::tp_double: {
            const auto* dv = std::get_if<double>(&value);
            if (dv == nullptr) {
                throw std::runtime_error("type mismatch: expected double");
            }
            write_le_f64(cp, *dv);
            break;
        }
        default:
            throw std::runtime_error("unsupported data type for SSM write");
        }
    }
    // NOLINTEND(bugprone-branch-clone)
}

std::vector<std::uint8_t> SsmWriter::flush() const {
    if (!is_setup_) {
        throw std::runtime_error("SsmWriter not setup");
    }

    // Build the index data (one SSMIndex for all columns).
    std::vector<std::uint8_t> index_buf;
    {
        EndianAipsIoWriter iw(index_buf, big_endian_);
        iw.write_object_header("SSMIndex", 1);

        // n_used: number of data buckets.
        iw.write_u32(nr_data_buckets_);
        // rows_per_bucket.
        iw.write_u32(rows_per_bucket_);
        // nr_columns.
        iw.write_i32(static_cast<std::int32_t>(columns_.size()));

        // Free space map: SimpleOrderedMap<Int, Int>.
        const auto map_start = iw.position();
        iw.write_u32(0); // placeholder
        iw.write_string("SimpleOrderedMap");
        iw.write_u32(1); // version
        iw.write_i32(0); // default value
        iw.write_u32(0); // count (empty map)
        iw.write_u32(1); // incr
        // Patch map object length.
        const auto map_len = static_cast<std::uint32_t>(iw.position() - map_start);
        std::uint8_t mb[4];
        write_endian_u32(mb, map_len, big_endian_);
        std::memcpy(index_buf.data() + map_start, mb, 4);

        // Block<uInt64> last_row (version 2+ uses Int64 blocks).
        // For version 1, use Block<uInt>.
        // We write version 1, so use Block<uInt>.
        std::vector<std::uint32_t> last_row_v;
        std::vector<std::uint32_t> bucket_number_v;
        for (std::uint32_t b = 0; b < nr_data_buckets_; ++b) {
            auto last = static_cast<std::uint64_t>(b + 1) * rows_per_bucket_ - 1;
            if (last >= row_count_) {
                last = row_count_ - 1;
            }
            last_row_v.push_back(static_cast<std::uint32_t>(last));
            bucket_number_v.push_back(b);
        }
        iw.write_block_u32(last_row_v);
        iw.write_block_u32(bucket_number_v);

        iw.write_object_end();
    }

    // Total number of buckets: data + string + 1 index.
    const std::uint32_t nr_index_buckets = 1;
    const auto total_buckets = nr_data_buckets_ + nr_string_buckets_ + nr_index_buckets;

    // Index bucket number: after data + string buckets.
    const auto idx_bucket_nr = static_cast<std::int32_t>(nr_data_buckets_ + nr_string_buckets_);

    // Build the file header (512 bytes).
    std::vector<std::uint8_t> header(512, 0);
    {
        EndianAipsIoWriter hw(header, big_endian_);
        // Rewrite: we can't use hw because header is pre-allocated.
        // Instead, build header in a temp buffer and copy.
        static_cast<void>(hw); // unused
    }

    // Build header directly.
    header.assign(512, 0);
    {
        std::vector<std::uint8_t> hdr_buf;
        EndianAipsIoWriter hw(hdr_buf, big_endian_);

        hw.write_object_header("StandardStMan", 3);
        // Version >= 3: endian flag.
        hw.write_u8(big_endian_ ? 1 : 0);
        hw.write_u32(bucket_size_);
        hw.write_u32(total_buckets);
        hw.write_u32(2);  // pers_cache_size
        hw.write_u32(0);  // free_buckets_nr
        hw.write_i32(-1); // first_free_bucket
        hw.write_u32(nr_index_buckets);
        hw.write_i32(idx_bucket_nr);
        hw.write_u32(8); // idx_bucket_offset (version >= 2)
        hw.write_i32(last_string_bucket_);
        hw.write_u32(static_cast<std::uint32_t>(index_buf.size()));
        hw.write_u32(1); // nr_indices

        hw.write_object_end();

        if (hdr_buf.size() > 512) {
            throw std::runtime_error("SSM file header exceeds 512 bytes");
        }
        std::memcpy(header.data(), hdr_buf.data(), hdr_buf.size());
    }

    // Assemble the complete file.
    std::vector<std::uint8_t> result;
    result.reserve(512 + static_cast<std::size_t>(total_buckets) * bucket_size_);

    // Header.
    result.insert(result.end(), header.begin(), header.end());

    // Data buckets.
    result.insert(result.end(), bucket_data_.begin(), bucket_data_.end());

    // String buckets (if any).
    if (!string_bucket_data_.empty()) {
        result.insert(result.end(), string_bucket_data_.begin(), string_bucket_data_.end());
    }

    // Index bucket: pad to bucket_size_.
    {
        std::vector<std::uint8_t> idx_bucket(bucket_size_, 0);
        // Index bucket layout when idx_bucket_offset > 0:
        // First 8 bytes: link header (ff ff ff ff ff ff ff ff for no chaining).
        // Then index data starting at idx_bucket_offset.
        write_endian_i32(idx_bucket.data(), -1, big_endian_);
        write_endian_i32(idx_bucket.data() + 4, -1, big_endian_);
        if (index_buf.size() + 8 > bucket_size_) {
            throw std::runtime_error("index data exceeds bucket size");
        }
        std::memcpy(idx_bucket.data() + 8, index_buf.data(), index_buf.size());
        result.insert(result.end(), idx_bucket.begin(), idx_bucket.end());
    }

    return result;
}

std::vector<std::uint8_t> SsmWriter::make_blob() const {
    if (!is_setup_) {
        throw std::runtime_error("SsmWriter not setup");
    }

    // The SSM blob is written in big-endian AipsIO format.
    AipsIoWriter writer;

    // Object header: "SSM" version 2 (casacore uses v2 for big-endian).
    const auto header_start = writer.size();
    writer.write_object_header(0, "SSM", 2);

    // data_man_name.
    writer.write_string(dm_name_);

    // Block<uInt> column_offset.
    // AipsIO object lengths include the length field itself (casacore compat).
    {
        const auto block_start = writer.size();
        writer.write_u32(0); // placeholder for object length
        writer.write_string("Block");
        writer.write_u32(1);
        writer.write_u32(static_cast<std::uint32_t>(columns_.size()));
        for (const auto& ci : columns_) {
            writer.write_u32(ci.col_offset);
        }
        writer.patch_u32(block_start, static_cast<std::uint32_t>(writer.size() - block_start));
    }

    // Block<uInt> col_index_map (all columns in index 0).
    {
        const auto block_start = writer.size();
        writer.write_u32(0); // placeholder for object length
        writer.write_string("Block");
        writer.write_u32(1);
        writer.write_u32(static_cast<std::uint32_t>(columns_.size()));
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            writer.write_u32(0); // all in index 0
        }
        writer.patch_u32(block_start, static_cast<std::uint32_t>(writer.size() - block_start));
    }

    // Patch the SSM object length (position after magic, includes itself).
    writer.patch_u32(header_start + 4,
                     static_cast<std::uint32_t>(writer.size() - header_start - 4));

    return writer.take_bytes();
}

void SsmWriter::write_file(const std::string_view table_dir,
                           const std::uint32_t sequence_number) const {
    const auto data = flush();
    const auto path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sequence_number));
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot create SSM file: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

} // namespace casacore_mini
