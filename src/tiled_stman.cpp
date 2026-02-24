#include "casacore_mini/tiled_stman.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_writer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {
namespace {

/// Default bucket size used by casacore when no specific size can be determined.
constexpr std::uint32_t kDefaultBucketSize = 32768;

// Read a file into a byte vector.
[[nodiscard]] std::vector<std::uint8_t> read_file_tsm(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Element size in bytes for a given data type.
// NOLINTBEGIN(bugprone-branch-clone)
[[nodiscard]] std::uint32_t tsm_element_size(DataType dt) {
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
    case DataType::tp_complex:
        return 8;
    case DataType::tp_int64:
        return 8;
    default:
        throw std::runtime_error("unsupported TSM data type");
    }
}
// NOLINTEND(bugprone-branch-clone)

// Product of shape elements.
[[nodiscard]] std::uint64_t shape_product(const std::vector<std::int64_t>& shape) {
    if (shape.empty()) {
        return 0;
    }
    std::uint64_t p = 1;
    for (const auto s : shape) {
        p *= static_cast<std::uint64_t>(s);
    }
    return p;
}

// ============================================================================
// .f0 header parser for casacore-produced TSM files
// ============================================================================

/// Parsed data from a TSM .f0 header file.
struct TsmHeaderInfo {
    std::string sm_type; // e.g. "TiledColumnStMan"
    std::uint32_t nrow = 0;
    std::uint32_t ncol = 0;
    std::vector<std::int32_t> col_dtypes; // casacore DataType per column
    std::uint32_t ndim = 0;
    std::uint32_t bucket_size = kDefaultBucketSize;
    std::uint64_t file_length = 0;    // total length of the TSM data file
    std::int32_t data_file_seqnr = 0; // sequence number of first present TSMFile

    /// Per-cube info.
    struct CubeInfo {
        std::vector<std::int64_t> cube_shape;
        std::vector<std::int64_t> tile_shape;
        std::int32_t file_nr = 0;
        std::uint64_t file_offset = 0;
    };
    std::vector<CubeInfo> cubes;
};

/// Read an IPosition from AipsIO stream (handles both root and nested format).
[[nodiscard]] std::vector<std::int64_t> read_iposition(AipsIoReader& reader) {
    const auto hdr = reader.read_object_header_auto();
    if (hdr.object_type != "IPosition") {
        throw std::runtime_error("expected IPosition, got: " + hdr.object_type);
    }
    const auto ndim = reader.read_u32();
    std::vector<std::int64_t> result;
    result.reserve(ndim);
    for (std::uint32_t i = 0; i < ndim; ++i) {
        if (hdr.object_version >= 2) {
            result.push_back(reader.read_i64());
        } else {
            result.push_back(static_cast<std::int64_t>(reader.read_i32()));
        }
    }
    return result;
}

/// Read an empty/simple Record from AipsIO (skip it).
void skip_aipsio_record(AipsIoReader& reader) {
    const auto hdr = reader.read_object_header_auto();
    if (hdr.object_type != "Record" && hdr.object_type != "TableRecord") {
        throw std::runtime_error("expected Record, got: " + hdr.object_type);
    }
    // RecordDesc sub-object.
    const auto rdhdr = reader.read_object_header_auto();
    if (rdhdr.object_type != "RecordDesc") {
        throw std::runtime_error("expected RecordDesc, got: " + rdhdr.object_type);
    }
    const auto nfields = reader.read_u32();
    // For each field: i32 type + string name + optional sub-record-desc.
    for (std::uint32_t i = 0; i < nfields; ++i) {
        const auto ftype = reader.read_i32();
        static_cast<void>(reader.read_string()); // field name
        if (ftype == 25) {                       // TpRecord -- nested RecordDesc
            // Recursively skip a RecordDesc.
            const auto sub_rd = reader.read_object_header_auto();
            static_cast<void>(sub_rd);
            const auto sub_n = reader.read_u32();
            static_cast<void>(sub_n);
            // This is a simplification: skip nested field types.
            // For our use case, TSM cube records are typically empty.
        }
        // Array fields have an IPosition shape too.
        const auto ftype_i = static_cast<std::int32_t>(ftype);
        if (ftype_i >= 13 && ftype_i <= 24) {
            // Array type — read the IPosition shape.
            static_cast<void>(read_iposition(reader));
        }
    }
    // recordType (Int).
    static_cast<void>(reader.read_i32());

    // Skip actual field values — for empty records there are none.
    // For non-empty records we'd need to know the types; but TSM cube
    // records in practice are empty. If nfields > 0, we just warn and
    // skip nothing (the caller will encounter parse errors if the record
    // is non-trivial). For robustness we handle the common case.
}

/// Parse the .f0 header file to extract TSM layout info.
/// Returns nullopt-like default if the file cannot be parsed (mini-produced).
[[nodiscard]] TsmHeaderInfo parse_tsm_header(const std::filesystem::path& header_path) {
    const auto bytes = read_file_tsm(header_path);
    AipsIoReader reader(bytes);

    TsmHeaderInfo info;

    // Root object: SM_type version=1
    const auto root_hdr = reader.read_object_header();
    info.sm_type = root_hdr.object_type;

    // TiledShapeStMan and TiledDataStMan write TiledStMan FIRST (no preceding
    // IPosition). TiledColumnStMan and TiledCellStMan write an IPosition before.
    const bool is_shape = (info.sm_type == "TiledShapeStMan");
    const bool is_data = (info.sm_type == "TiledDataStMan");
    if (!is_shape && !is_data) {
        // IPosition: default tile shape (without row dimension).
        static_cast<void>(read_iposition(reader));
    }

    // Sub-object: "TiledStMan" version=2 (nested, auto-detect magic)
    const auto tsm_hdr = reader.read_object_header_auto();
    if (tsm_hdr.object_type != "TiledStMan") {
        throw std::runtime_error("expected TiledStMan, got: " + tsm_hdr.object_type);
    }

    // Version >= 2 has bigEndian flag.
    if (tsm_hdr.object_version >= 2) {
        static_cast<void>(reader.read_u8()); // bigEndian (Bool = 1 byte)
    }

    // sequenceNr.
    static_cast<void>(reader.read_u32());

    // nrow.
    info.nrow = reader.read_u32();

    // ncol.
    info.ncol = reader.read_u32();

    // Column data types: Int[ncol].
    info.col_dtypes.reserve(info.ncol);
    for (std::uint32_t i = 0; i < info.ncol; ++i) {
        info.col_dtypes.push_back(reader.read_i32());
    }

    // Hypercolumn name (String).
    static_cast<void>(reader.read_string());

    // maxCacheSize (uInt).
    static_cast<void>(reader.read_u32());

    // ndim.
    info.ndim = reader.read_u32();

    // nfiles.
    const auto nfiles = reader.read_u32();
    bool found_first_file = false;
    for (std::uint32_t f = 0; f < nfiles; ++f) {
        // Bool: has_file (1 byte).
        const auto has_file = reader.read_u8();
        if (has_file != 0U) {
            // TSMFile: uInt version.
            const auto fver = reader.read_u32();
            // uInt seqnr.
            const auto seqnr = reader.read_u32();
            // File length: u64 for version >= 2, u32 otherwise.
            if (fver >= 2) {
                info.file_length = reader.read_u64();
            } else {
                info.file_length = reader.read_u32();
            }
            // Capture the sequence number from the first present file.
            if (!found_first_file) {
                info.data_file_seqnr = static_cast<std::int32_t>(seqnr);
                found_first_file = true;
            }
        }
    }

    // Compute bucket_size from file_length and total bucket data.
    // The file length should be an exact multiple of bucket_size.
    // Default to 32768 if we cannot determine it.
    info.bucket_size = kDefaultBucketSize;

    // ncubes.
    const auto ncubes = reader.read_u32();
    info.cubes.reserve(ncubes);
    for (std::uint32_t c = 0; c < ncubes; ++c) {
        // NOTE: For TSM cubes there is NO Bool flag before each cube.
        // Each cube is serialized directly.
        TsmHeaderInfo::CubeInfo cube;

        // TSMCube: uInt version.
        const auto cube_ver = reader.read_u32();

        // Record (cube values) — skip it.
        skip_aipsio_record(reader);

        // Bool: extensible (1 byte).
        static_cast<void>(reader.read_u8());

        // uInt: nrdim (number of dims excluding the row dimension...
        // actually it's total dims of the cube).
        static_cast<void>(reader.read_u32());

        // IPosition: cubeShape.
        cube.cube_shape = read_iposition(reader);

        // IPosition: tileShape.
        cube.tile_shape = read_iposition(reader);

        // Int: fileNr.
        cube.file_nr = reader.read_i32();

        // fileOffset: i64 for cube version >= 2, i32 otherwise.
        if (cube_ver >= 2) {
            cube.file_offset = static_cast<std::uint64_t>(reader.read_i64());
        } else {
            cube.file_offset = static_cast<std::uint64_t>(reader.read_i32());
        }

        info.cubes.push_back(std::move(cube));
    }

    return info;
}

// ============================================================================
// Tile layout computation (matching casacore's TiledStMan logic)
// ============================================================================

/// Compute the row tiling dimension.  Copied from casacore's
/// TSMCube::adjustTileShape (TSMCube.cc).
///
/// tile_rows = max(1, round(32768.0 / product(cell_tile_shape)))
///
/// This is completely deterministic from the cell shape and the constant
/// 32768 (the casacore default target tile size in elements).
// TODO(casacore-compat): formula copied from casacore TSMCube::adjustTileShape.
// The constant 32768 is baked in; casacore allows overriding via
// maximumCacheSize but always uses 32768 for TiledColumnStMan. If casacore
// ever changes this default we may need to revisit.
[[nodiscard]] std::int64_t compute_tile_rows(const std::vector<std::int64_t>& cell_shape) {
    constexpr double kTargetTilePixels = 32768.0;
    double cell_pixels = 1.0;
    for (const auto s : cell_shape) {
        cell_pixels *= static_cast<double>(s);
    }
    if (cell_pixels <= 0.0) {
        return 1;
    }
    auto tile_rows =
        static_cast<std::int64_t>(std::max(1.0, std::round(kTargetTilePixels / cell_pixels)));
    return tile_rows;
}

/// Layout info for a TSM hypercube with shared-bucket model.
///
/// In casacore's TiledStMan, each tile is ONE bucket containing ALL columns'
/// data laid out sequentially (columns sorted by descending pixel size,
/// stable sort).  Within a tile, column c starts at its per-column offset.
struct TsmTileLayout {
    std::vector<std::int64_t> tile_shape; // [cell_shape..., tile_rows]
    std::uint64_t tile_pixels = 0;        // product(tile_shape)
    std::int64_t tile_rows = 0;           // last element of tile_shape
    std::uint64_t bucket_size = 0;        // sum of all column data per tile
    std::uint64_t tiles_along_row = 0;    // ceil(nrow / tile_rows)

    /// Sorted column indices (descending pixel size, stable).
    std::vector<std::size_t> sorted_col_indices;

    /// Per-column byte offset within a tile (indexed by SORTED position).
    std::vector<std::uint64_t> col_tile_offsets;
};

/// Find the first non-dummy cube in a TSM header.
/// TiledShapeStMan has cube[0] as a dummy (nrdim=0, fileNr=-1, empty shapes).
/// Returns nullptr if no valid cube exists.
[[nodiscard]] const TsmHeaderInfo::CubeInfo* find_first_valid_cube(const TsmHeaderInfo& header) {
    for (const auto& c : header.cubes) {
        if (c.file_nr >= 0 && !c.tile_shape.empty()) {
            return &c;
        }
    }
    return nullptr;
}

/// Compute tile layout from a parsed .f0 header (casacore-produced).
[[nodiscard]] TsmTileLayout
compute_tile_layout_from_header(const TsmHeaderInfo& header,
                                const std::vector<std::uint32_t>& col_pixel_sizes) {

    if (header.cubes.empty()) {
        throw std::runtime_error("TSM header has no cubes");
    }
    const auto* cube_ptr = find_first_valid_cube(header);
    if (cube_ptr == nullptr) {
        throw std::runtime_error("TSM header has no valid cubes");
    }
    const auto& cube = *cube_ptr;
    if (cube.tile_shape.empty()) {
        throw std::runtime_error("TSM tile shape is empty");
    }

    TsmTileLayout layout;
    layout.tile_shape = cube.tile_shape;
    layout.tile_pixels = shape_product(layout.tile_shape);
    layout.tile_rows = layout.tile_shape.back();

    const auto nrow_u64 = static_cast<std::uint64_t>(header.nrow);
    const auto tr_u64 = static_cast<std::uint64_t>(layout.tile_rows);
    layout.tiles_along_row = (nrow_u64 + tr_u64 - 1U) / tr_u64;

    // Sort columns by descending pixel size, matching casacore's GenSort behavior:
    // stable ascending sort then reverse.
    const auto ncol = col_pixel_sizes.size();
    layout.sorted_col_indices.resize(ncol);
    std::iota(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end(), 0U);
    std::stable_sort(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end(),
                     [&](std::size_t a, std::size_t b) {
                         return col_pixel_sizes[a] < col_pixel_sizes[b]; // ascending
                     });
    std::reverse(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end());

    // Compute per-column offset within a tile.
    layout.col_tile_offsets.resize(ncol);
    std::uint64_t offset = 0;
    for (std::size_t i = 0; i < ncol; ++i) {
        layout.col_tile_offsets[i] = offset;
        const auto sorted_idx = layout.sorted_col_indices[i];
        offset += layout.tile_pixels * static_cast<std::uint64_t>(col_pixel_sizes[sorted_idx]);
    }
    layout.bucket_size = offset;
    return layout;
}

/// Compute tile layout from column metadata (no .f0 header available).
/// Uses casacore's deterministic tile_rows formula.
[[nodiscard]] TsmTileLayout
compute_tile_layout_from_columns(const std::vector<std::int64_t>& cell_shape,
                                 const std::vector<std::uint32_t>& col_pixel_sizes,
                                 std::uint64_t row_count) {

    TsmTileLayout layout;
    layout.tile_rows = compute_tile_rows(cell_shape);

    // tile_shape = [cell_shape..., tile_rows].
    layout.tile_shape.reserve(cell_shape.size() + 1);
    for (const auto s : cell_shape) {
        layout.tile_shape.push_back(s);
    }
    layout.tile_shape.push_back(layout.tile_rows);
    layout.tile_pixels = shape_product(layout.tile_shape);

    const auto tr_u64 = static_cast<std::uint64_t>(layout.tile_rows);
    layout.tiles_along_row = (row_count + tr_u64 - 1U) / tr_u64;

    // Sort columns by descending pixel size, matching casacore's GenSort behavior:
    // stable ascending sort then reverse. For equal-sized columns this reverses
    // the original insertion order, which must be matched exactly for TSM0 layout.
    const auto ncol = col_pixel_sizes.size();
    layout.sorted_col_indices.resize(ncol);
    std::iota(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end(), 0U);
    std::stable_sort(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end(),
                     [&](std::size_t a, std::size_t b) {
                         return col_pixel_sizes[a] < col_pixel_sizes[b]; // ascending
                     });
    std::reverse(layout.sorted_col_indices.begin(), layout.sorted_col_indices.end());

    // Compute per-column offset within a tile.
    layout.col_tile_offsets.resize(ncol);
    std::uint64_t offset = 0;
    for (std::size_t i = 0; i < ncol; ++i) {
        layout.col_tile_offsets[i] = offset;
        const auto sorted_idx = layout.sorted_col_indices[i];
        offset += layout.tile_pixels * static_cast<std::uint64_t>(col_pixel_sizes[sorted_idx]);
    }
    layout.bucket_size = offset;
    return layout;
}

// ============================================================================
// Writer helpers: casacore-compatible .f0 header and TSM0 file
// ============================================================================

/// Write an IPosition as a nested AipsIO sub-object (no magic prefix).
void write_iposition(AipsIoWriter& writer, const std::vector<std::int64_t>& shape) {
    const auto ip_start = writer.size();
    writer.write_nested_object_header(0, "IPosition", 1);
    writer.write_u32(static_cast<std::uint32_t>(shape.size()));
    for (const auto s : shape) {
        writer.write_u32(static_cast<std::uint32_t>(s));
    }
    // casacore's object length includes the length field itself.
    writer.patch_u32(ip_start, static_cast<std::uint32_t>(writer.size() - ip_start));
}

/// Write an empty Record as a nested AipsIO sub-object (no magic prefix).
void write_empty_record(AipsIoWriter& writer) {
    const auto rec_start = writer.size();
    writer.write_nested_object_header(0, "Record", 1);
    // RecordDesc sub-object.
    const auto rd_start = writer.size();
    writer.write_nested_object_header(0, "RecordDesc", 2);
    writer.write_u32(0); // nfields
    writer.patch_u32(rd_start, static_cast<std::uint32_t>(writer.size() - rd_start));
    // recordType = Variable (1).
    writer.write_i32(1);
    // No field values.
    writer.patch_u32(rec_start, static_cast<std::uint32_t>(writer.size() - rec_start));
}

} // namespace

// --- TiledStManReader ---

void TiledStManReader::open(const std::string_view table_dir, const std::size_t sm_index,
                            const TableDatFull& table_dat) {
    if (sm_index >= table_dat.storage_managers.size()) {
        throw std::runtime_error("TSM SM index out of range");
    }
    const auto& sm = table_dat.storage_managers[sm_index];
    row_count_ = table_dat.row_count;

    // Build column info from table_dat.
    for (const auto& cs : table_dat.column_setups) {
        if (cs.sequence_number != sm.sequence_number) {
            continue;
        }
        const auto& cols = table_dat.table_desc.columns;
        auto it = std::find_if(cols.begin(), cols.end(),
                               [&](const ColumnDesc& cd) { return cd.name == cs.column_name; });
        if (it == cols.end()) {
            throw std::runtime_error("column not in table desc: " + cs.column_name);
        }

        TsmColumnInfo info;
        info.name = it->name;
        info.data_type = it->data_type;
        info.element_size = tsm_element_size(it->data_type);
        // Use shape from column_setup if available, else from column desc.
        info.shape = cs.has_shape ? cs.shape : it->shape;
        info.cell_elements = shape_product(info.shape);
        columns_.push_back(std::move(info));
    }

    // Collect per-column pixel sizes for layout computation.
    std::vector<std::uint32_t> col_pixel_sizes;
    col_pixel_sizes.reserve(columns_.size());
    for (const auto& ci : columns_) {
        col_pixel_sizes.push_back(ci.element_size);
    }

    // Try to parse the .f0 header for accurate tile-based layout.
    const auto header_path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sm.sequence_number));

    bool have_header = false;
    TsmHeaderInfo tsm_header;
    if (std::filesystem::exists(header_path)) {
        try {
            tsm_header = parse_tsm_header(header_path);
            have_header = true;
        } catch (const std::exception&) {
            // Fall back to computed layout if header parsing fails.
            have_header = false;
        }
    }

    // Variable-shape columns (e.g. TiledShapeStMan) have no shape in
    // ColumnDesc or ColumnSetup.  Derive cell shape from the cube shape
    // in the .f0 header: cube_shape = [cell_dim0, ..., cell_dimN, nrows],
    // so cell_shape = cube_shape with the last dimension stripped.
    if (have_header) {
        const auto* cube = find_first_valid_cube(tsm_header);
        if (cube != nullptr && cube->cube_shape.size() >= 2) {
            for (auto& col : columns_) {
                if (col.cell_elements == 0) {
                    col.shape.assign(cube->cube_shape.begin(), cube->cube_shape.end() - 1);
                    col.cell_elements = shape_product(col.shape);
                }
            }
        }
    }

    TsmTileLayout tsm_layout;
    if (have_header && !tsm_header.cubes.empty()) {
        tsm_layout = compute_tile_layout_from_header(tsm_header, col_pixel_sizes);
    } else {
        // Fallback: compute from column metadata using casacore's formula.
        if (columns_.empty()) {
            throw std::runtime_error("TSM has no columns");
        }
        tsm_layout =
            compute_tile_layout_from_columns(columns_[0].shape, col_pixel_sizes, row_count_);
    }
    // Copy to member.
    tile_layout_.tile_shape = std::move(tsm_layout.tile_shape);
    tile_layout_.tile_pixels = tsm_layout.tile_pixels;
    tile_layout_.tile_rows = tsm_layout.tile_rows;
    tile_layout_.bucket_size = tsm_layout.bucket_size;
    tile_layout_.tiles_along_row = tsm_layout.tiles_along_row;
    tile_layout_.sorted_col_indices = std::move(tsm_layout.sorted_col_indices);
    tile_layout_.col_tile_offsets = std::move(tsm_layout.col_tile_offsets);

    // Read the TSM data file (no BucketFile header — raw bucket data).
    // Use the header-derived sequence number for the _TSMn suffix.
    // Fall back to other suffix if header-derived path doesn't exist.
    const auto base_path =
        std::filesystem::path(table_dir) / ("table.f" + std::to_string(sm.sequence_number));
    const auto primary_suffix = "_TSM" + std::to_string(tsm_header.data_file_seqnr);
    auto tsm_path = base_path.string() + primary_suffix;
    if (!std::filesystem::exists(tsm_path)) {
        // Fallback: try the other common suffix.
        const auto alt_suffix = (tsm_header.data_file_seqnr == 0) ? "_TSM1" : "_TSM0";
        auto alt_path = base_path.string() + alt_suffix;
        if (std::filesystem::exists(alt_path)) {
            tsm_path = std::move(alt_path);
        }
        // If neither exists, we'll get an error from read_file_tsm below.
    }
    tsm_data_ = read_file_tsm(tsm_path);

    is_open_ = true;
}

/// Helper: find a column by name and compute byte offset for a given row.
const TiledStManReader::TsmColumnInfo*
TiledStManReader::find_column(const std::string_view col_name, std::size_t& original_index) const {
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == col_name) {
            original_index = i;
            return &columns_[i];
        }
    }
    return nullptr;
}

std::uint64_t TiledStManReader::cell_byte_offset(std::size_t original_col_index,
                                                 std::uint32_t pixel_size,
                                                 const std::vector<std::int64_t>& cell_shape,
                                                 std::uint64_t row) const {

    // Find the sorted position of this column.
    std::size_t sorted_pos = 0;
    for (std::size_t i = 0; i < tile_layout_.sorted_col_indices.size(); ++i) {
        if (tile_layout_.sorted_col_indices[i] == original_col_index) {
            sorted_pos = i;
            break;
        }
    }

    const auto tile_rows_u64 = static_cast<std::uint64_t>(tile_layout_.tile_rows);
    const auto tile_index = row / tile_rows_u64;
    const auto row_in_tile = row % tile_rows_u64;

    const auto cell_elements = shape_product(cell_shape);
    const auto cell_bytes = cell_elements * static_cast<std::uint64_t>(pixel_size);

    return tile_index * tile_layout_.bucket_size + tile_layout_.col_tile_offsets[sorted_pos] +
           row_in_tile * cell_bytes;
}

std::vector<float> TiledStManReader::read_float_cell(const std::string_view col_name,
                                                     const std::uint64_t row) const {
    if (!is_open_) {
        throw std::runtime_error("TiledStManReader not open");
    }

    std::size_t col_idx = 0;
    const auto* col = find_column(col_name, col_idx);
    if (col == nullptr) {
        throw std::runtime_error("column not found: " + std::string(col_name));
    }
    if (col->data_type != DataType::tp_float) {
        throw std::runtime_error("column is not Float: " + std::string(col_name));
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto cell_bytes = col->cell_elements * col->element_size;
    const auto byte_offset = cell_byte_offset(col_idx, col->element_size, col->shape, row);

    if (byte_offset + cell_bytes > tsm_data_.size()) {
        throw std::runtime_error("TSM0 read beyond file end");
    }

    std::vector<float> result(static_cast<std::size_t>(col->cell_elements));
    std::memcpy(result.data(), tsm_data_.data() + byte_offset, cell_bytes);
    return result;
}

std::vector<std::int32_t> TiledStManReader::read_int_cell(const std::string_view col_name,
                                                          const std::uint64_t row) const {
    if (!is_open_) {
        throw std::runtime_error("TiledStManReader not open");
    }

    std::size_t col_idx = 0;
    const auto* col = find_column(col_name, col_idx);
    if (col == nullptr) {
        throw std::runtime_error("column not found: " + std::string(col_name));
    }
    if (col->data_type != DataType::tp_int) {
        throw std::runtime_error("column is not Int: " + std::string(col_name));
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto cell_bytes = col->cell_elements * col->element_size;
    const auto byte_offset = cell_byte_offset(col_idx, col->element_size, col->shape, row);

    if (byte_offset + cell_bytes > tsm_data_.size()) {
        throw std::runtime_error("TSM0 read beyond file end");
    }

    std::vector<std::int32_t> result(static_cast<std::size_t>(col->cell_elements));
    std::memcpy(result.data(), tsm_data_.data() + byte_offset, cell_bytes);
    return result;
}

std::vector<std::uint8_t> TiledStManReader::read_raw_cell(const std::string_view col_name,
                                                          const std::uint64_t row) const {
    if (!is_open_) {
        throw std::runtime_error("TiledStManReader not open");
    }

    std::size_t col_idx = 0;
    const auto* col = find_column(col_name, col_idx);
    if (col == nullptr) {
        throw std::runtime_error("column not found: " + std::string(col_name));
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto cell_bytes = col->cell_elements * col->element_size;
    const auto byte_offset = cell_byte_offset(col_idx, col->element_size, col->shape, row);

    if (byte_offset + cell_bytes > tsm_data_.size()) {
        throw std::runtime_error("TSM0 read beyond file end");
    }

    std::vector<std::uint8_t> result(static_cast<std::size_t>(cell_bytes));
    std::memcpy(result.data(), tsm_data_.data() + byte_offset, cell_bytes);
    return result;
}

std::size_t TiledStManReader::column_count() const noexcept {
    return columns_.size();
}

bool TiledStManReader::is_open() const noexcept {
    return is_open_;
}

// --- TiledStManWriter ---

void TiledStManWriter::setup(const std::string_view sm_type, const std::string_view dm_name,
                             const std::vector<ColumnDesc>& columns,
                             const std::uint64_t row_count) {
    sm_type_ = sm_type;
    dm_name_ = dm_name;
    row_count_ = row_count;
    columns_.clear();

    for (const auto& cd : columns) {
        WriterColumnInfo info;
        info.name = cd.name;
        info.data_type = cd.data_type;
        info.element_size = tsm_element_size(cd.data_type);
        info.shape = cd.shape;
        info.cell_elements = shape_product(cd.shape);
        const auto total_bytes = info.cell_elements * info.element_size * row_count;
        info.data.resize(static_cast<std::size_t>(total_bytes), 0);
        columns_.push_back(std::move(info));
    }

    is_setup_ = true;
}

void TiledStManWriter::write_float_cell(const std::size_t col_index, const std::uint64_t row,
                                        const std::vector<float>& values) {
    if (!is_setup_) {
        throw std::runtime_error("TiledStManWriter not setup");
    }
    if (col_index >= columns_.size()) {
        throw std::runtime_error("TSM column index out of range");
    }
    auto& col = columns_[col_index];
    if (col.data_type != DataType::tp_float) {
        throw std::runtime_error("column is not Float");
    }
    if (values.size() != static_cast<std::size_t>(col.cell_elements)) {
        throw std::runtime_error("wrong number of values for cell");
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto cell_bytes = col.cell_elements * col.element_size;
    const auto offset = row * cell_bytes;
    std::memcpy(col.data.data() + offset, values.data(), cell_bytes);
}

void TiledStManWriter::write_int_cell(const std::size_t col_index, const std::uint64_t row,
                                      const std::vector<std::int32_t>& values) {
    if (!is_setup_) {
        throw std::runtime_error("TiledStManWriter not setup");
    }
    if (col_index >= columns_.size()) {
        throw std::runtime_error("TSM column index out of range");
    }
    auto& col = columns_[col_index];
    if (col.data_type != DataType::tp_int) {
        throw std::runtime_error("column is not Int");
    }
    if (values.size() != static_cast<std::size_t>(col.cell_elements)) {
        throw std::runtime_error("wrong number of values for cell");
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto cell_bytes = col.cell_elements * col.element_size;
    const auto offset = row * cell_bytes;
    std::memcpy(col.data.data() + offset, values.data(), cell_bytes);
}

void TiledStManWriter::write_raw_cell(const std::size_t col_index, const std::uint64_t row,
                                      const std::vector<std::uint8_t>& data) {
    if (!is_setup_) {
        throw std::runtime_error("TiledStManWriter not setup");
    }
    if (col_index >= columns_.size()) {
        throw std::runtime_error("TSM column index out of range");
    }
    auto& col = columns_[col_index];
    const auto cell_bytes = col.cell_elements * col.element_size;
    if (data.size() != static_cast<std::size_t>(cell_bytes)) {
        throw std::runtime_error("wrong number of bytes for raw cell");
    }
    if (row >= row_count_) {
        throw std::runtime_error("row out of range");
    }

    const auto offset = row * cell_bytes;
    std::memcpy(col.data.data() + offset, data.data(), cell_bytes);
}

std::vector<std::uint8_t> TiledStManWriter::make_blob() const {
    // TSM blob: minimal AipsIO object for the data manager info.
    AipsIoWriter writer;
    const auto start = writer.size();
    writer.write_object_header(0, "TSM", 1);
    writer.write_string(dm_name_);
    writer.patch_u32(start + 4, static_cast<std::uint32_t>(writer.size() - start - 4));
    return writer.take_bytes();
}

/// Write a Block<uInt> AipsIO object (nested, no magic).
void write_block_u32(AipsIoWriter& writer, const std::vector<std::uint32_t>& values) {
    const auto start = writer.size();
    writer.write_nested_object_header(0, "Block", 1);
    writer.write_u32(static_cast<std::uint32_t>(values.size()));
    for (const auto v : values) {
        writer.write_u32(v);
    }
    writer.patch_u32(start, static_cast<std::uint32_t>(writer.size() - start));
}

/// Write a Block<uInt64> AipsIO object (nested, no magic).
void write_block_u64(AipsIoWriter& writer, const std::vector<std::uint64_t>& values) {
    const auto start = writer.size();
    writer.write_nested_object_header(0, "Block", 1);
    writer.write_u32(static_cast<std::uint32_t>(values.size()));
    for (const auto v : values) {
        writer.write_u64(v);
    }
    writer.patch_u32(start, static_cast<std::uint32_t>(writer.size() - start));
}

/// Write a TSMCube record into an AipsIoWriter.
/// Writes: cube_version(1), empty Record, extensible, nrdim,
///         cubeShape (IPosition), tileShape (IPosition), fileNr, fileOffset.
void write_tsm_cube(AipsIoWriter& hdr, bool extensible, std::uint32_t nrdim,
                    const std::vector<std::int64_t>& cube_shape,
                    const std::vector<std::int64_t>& tile_shape, std::int32_t file_nr,
                    std::int32_t file_offset) {
    hdr.write_u32(1); // cube version
    write_empty_record(hdr);
    hdr.write_u8(extensible ? 1 : 0);
    hdr.write_u32(nrdim);
    write_iposition(hdr, cube_shape);
    write_iposition(hdr, tile_shape);
    hdr.write_i32(file_nr);
    hdr.write_i32(file_offset);
}

/// Write the common TiledStMan nested sub-object (no magic).
/// Returns the start position for patching the length field.
std::size_t write_tsm_nested_begin(AipsIoWriter& hdr, std::uint32_t sequence_number,
                                   std::uint64_t row_count,
                                   const std::vector<std::int32_t>& col_dtypes,
                                   const std::string& dm_name, std::uint32_t ndim) {
    const auto tsm_start = hdr.size();
    hdr.write_nested_object_header(0, "TiledStMan", 2);
    hdr.write_u8(0); // bigEndian = false (LE)
    hdr.write_u32(sequence_number);
    hdr.write_u32(static_cast<std::uint32_t>(row_count));
    hdr.write_u32(static_cast<std::uint32_t>(col_dtypes.size()));
    for (const auto dt : col_dtypes) {
        hdr.write_i32(dt);
    }
    hdr.write_string(dm_name);
    hdr.write_u32(0); // maxCacheSize
    hdr.write_u32(ndim);
    return tsm_start;
}

/// Compute column sort order: ascending pixel size then reverse
/// (matching casacore's GenSortIndirect Descending).
[[nodiscard]] std::vector<std::size_t>
compute_sorted_col_indices(const std::vector<std::uint32_t>& col_pixel_sizes) {
    const auto ncol = col_pixel_sizes.size();
    std::vector<std::size_t> indices(ncol);
    std::iota(indices.begin(), indices.end(), 0U);
    std::stable_sort(indices.begin(), indices.end(), [&](std::size_t a, std::size_t b) {
        return col_pixel_sizes[a] < col_pixel_sizes[b];
    });
    std::reverse(indices.begin(), indices.end());
    return indices;
}

/// Compute per-column byte offsets within a tile.
[[nodiscard]] std::vector<std::uint64_t>
compute_col_offsets(const std::vector<std::size_t>& sorted_indices,
                    const std::vector<std::uint32_t>& col_pixel_sizes, std::uint64_t tile_pixels) {
    std::vector<std::uint64_t> offsets(sorted_indices.size());
    std::uint64_t off = 0;
    for (std::size_t i = 0; i < sorted_indices.size(); ++i) {
        offsets[i] = off;
        off += tile_pixels * col_pixel_sizes[sorted_indices[i]];
    }
    return offsets;
}

/// Per-column info needed for writing TSM data files.
struct TsmColData {
    const std::uint8_t* data_ptr;
    std::uint64_t cell_bytes; // element_size * cell_elements
};

/// Parameters for write_tsm_data_file (avoids easily-swappable uint64 args).
struct TsmWriteParams {
    std::uint64_t bucket_size = 0;
    std::uint64_t tile_rows = 0;
    std::uint64_t row_count = 0;
    std::uint64_t tiles_along_row = 0;
};

/// Write TSM data file with row-based tile layout.
void write_tsm_data_file(const std::filesystem::path& path, const std::vector<TsmColData>& col_data,
                         const std::vector<std::size_t>& sorted_indices,
                         const std::vector<std::uint64_t>& col_offsets,
                         const TsmWriteParams& params) {
    const auto total_size = params.tiles_along_row * params.bucket_size;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(total_size), 0);

    for (std::uint64_t row = 0; row < params.row_count; ++row) {
        const auto tile_index = row / params.tile_rows;
        const auto row_in_tile = row % params.tile_rows;

        for (std::size_t sp = 0; sp < sorted_indices.size(); ++sp) {
            const auto orig_idx = sorted_indices[sp];
            const auto& cd = col_data[orig_idx];
            const auto src = row * cd.cell_bytes;
            const auto dst =
                tile_index * params.bucket_size + col_offsets[sp] + row_in_tile * cd.cell_bytes;
            std::memcpy(buf.data() + dst, cd.data_ptr + src,
                        static_cast<std::size_t>(cd.cell_bytes));
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot create TSM file: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

void TiledStManWriter::write_files(const std::string_view table_dir,
                                   const std::uint32_t sequence_number) const {
    if (!is_setup_) {
        throw std::runtime_error("TiledStManWriter not setup");
    }

    const auto nr_columns = columns_.size();
    const auto& cell_shape = columns_[0].shape;
    const auto cell_pixels = shape_product(cell_shape);

    // Build per-column metadata vectors.
    std::vector<std::uint32_t> col_pixel_sizes;
    std::vector<std::int32_t> col_dtypes;
    std::vector<TsmColData> col_data;
    col_pixel_sizes.reserve(nr_columns);
    col_dtypes.reserve(nr_columns);
    col_data.reserve(nr_columns);
    for (const auto& col : columns_) {
        col_pixel_sizes.push_back(col.element_size);
        col_dtypes.push_back(static_cast<std::int32_t>(col.data_type));
        col_data.push_back({col.data.data(), col.cell_elements * col.element_size});
    }
    const auto sorted_indices = compute_sorted_col_indices(col_pixel_sizes);

    // SM type flags.
    const bool is_cell = (sm_type_ == "TiledCellStMan");
    const bool is_shape = (sm_type_ == "TiledShapeStMan");
    const bool is_data = (sm_type_ == "TiledDataStMan");

    // ndim: TiledCellStMan uses extraNdim=0, all others use +1.
    const std::uint32_t ndim = static_cast<std::uint32_t>(cell_shape.size() + (is_cell ? 0U : 1U));

    // Root AipsIO version: TiledDataStMan=2, all others=1.
    const std::uint32_t root_version = is_data ? 2U : 1U;

    // ============================================================
    // Compute tile layout and write data file(s)
    // ============================================================
    std::uint64_t file_length = 0;
    std::vector<std::int64_t> tile_shape;
    std::vector<std::int64_t> cube_shape;

    if (is_cell) {
        // TiledCellStMan: one cube per row, cubeShape = tileShape = cell_shape.
        // No row dimension.
        tile_shape = cell_shape;
        cube_shape = cell_shape;

        const auto col_offsets = compute_col_offsets(sorted_indices, col_pixel_sizes, cell_pixels);
        std::uint64_t tile_size = 0;
        for (std::size_t i = 0; i < nr_columns; ++i) {
            tile_size += cell_pixels * col_pixel_sizes[sorted_indices[i]];
        }
        file_length = row_count_ * tile_size;

        // Write data: each row is one tile (one cube).
        const auto data_path = std::filesystem::path(table_dir) /
                               ("table.f" + std::to_string(sequence_number) + "_TSM0");
        // tile_rows=1 with cell_pixels per tile, tiles_along_row = nrow.
        write_tsm_data_file(data_path, col_data, sorted_indices, col_offsets,
                            {.bucket_size = tile_size,
                             .tile_rows = 1,
                             .row_count = row_count_,
                             .tiles_along_row = row_count_});
    } else if (is_shape) {
        // TiledShapeStMan: tileShape = [cell_shape..., 1], 1 row per tile.
        // Uses TSM1 (file seqnr 1).
        tile_shape.reserve(cell_shape.size() + 1);
        for (const auto s : cell_shape)
            tile_shape.push_back(s);
        tile_shape.push_back(1); // 1 row per tile

        cube_shape.reserve(cell_shape.size() + 1);
        for (const auto s : cell_shape)
            cube_shape.push_back(s);
        cube_shape.push_back(static_cast<std::int64_t>(row_count_));

        const auto tile_pixels_full = cell_pixels * 1; // 1 row
        const auto col_offsets =
            compute_col_offsets(sorted_indices, col_pixel_sizes, tile_pixels_full);
        std::uint64_t tile_size = 0;
        for (std::size_t i = 0; i < nr_columns; ++i) {
            tile_size += tile_pixels_full * col_pixel_sizes[sorted_indices[i]];
        }
        file_length = row_count_ * tile_size;

        const auto data_path = std::filesystem::path(table_dir) /
                               ("table.f" + std::to_string(sequence_number) + "_TSM1");
        write_tsm_data_file(data_path, col_data, sorted_indices, col_offsets,
                            {.bucket_size = tile_size,
                             .tile_rows = 1,
                             .row_count = row_count_,
                             .tiles_along_row = row_count_});
    } else if (is_data) {
        // TiledDataStMan: tileShape = cubeShape = [cell_shape..., nrow].
        // All data in one tile.
        tile_shape.reserve(cell_shape.size() + 1);
        for (const auto s : cell_shape)
            tile_shape.push_back(s);
        tile_shape.push_back(static_cast<std::int64_t>(row_count_));
        cube_shape = tile_shape;

        const auto tile_pixels_full = cell_pixels * row_count_;
        const auto col_offsets =
            compute_col_offsets(sorted_indices, col_pixel_sizes, tile_pixels_full);
        std::uint64_t tile_size = 0;
        for (std::size_t i = 0; i < nr_columns; ++i) {
            tile_size += tile_pixels_full * col_pixel_sizes[sorted_indices[i]];
        }
        file_length = tile_size;

        const auto data_path = std::filesystem::path(table_dir) /
                               ("table.f" + std::to_string(sequence_number) + "_TSM0");
        write_tsm_data_file(data_path, col_data, sorted_indices, col_offsets,
                            {.bucket_size = tile_size,
                             .tile_rows = row_count_,
                             .row_count = row_count_,
                             .tiles_along_row = 1});
    } else {
        // TiledColumnStMan: standard tile layout with deterministic tile_rows.
        const auto layout =
            compute_tile_layout_from_columns(cell_shape, col_pixel_sizes, row_count_);
        tile_shape = layout.tile_shape;
        file_length = layout.tiles_along_row * layout.bucket_size;

        cube_shape.reserve(cell_shape.size() + 1);
        for (const auto s : cell_shape)
            cube_shape.push_back(s);
        cube_shape.push_back(static_cast<std::int64_t>(row_count_));

        const auto data_path = std::filesystem::path(table_dir) /
                               ("table.f" + std::to_string(sequence_number) + "_TSM0");
        write_tsm_data_file(data_path, col_data, sorted_indices, layout.col_tile_offsets,
                            {.bucket_size = layout.bucket_size,
                             .tile_rows = static_cast<std::uint64_t>(layout.tile_rows),
                             .row_count = row_count_,
                             .tiles_along_row = layout.tiles_along_row});
    }

    // ============================================================
    // Build .f0 header file
    // ============================================================
    AipsIoWriter hdr;
    const auto root_start = hdr.size();
    hdr.write_object_header(0, sm_type_, root_version);

    // --- Pre-TiledStMan fields (type-specific) ---
    // TiledColumnStMan and TiledCellStMan write an IPosition (tile/default
    // tile shape) before the TiledStMan nested object.
    // TiledShapeStMan and TiledDataStMan write TiledStMan first.
    if (!is_shape && !is_data) {
        write_iposition(hdr, cell_shape);
    }

    // --- TiledStMan nested sub-object ---
    const auto tsm_start =
        write_tsm_nested_begin(hdr, sequence_number, row_count_, col_dtypes, dm_name_, ndim);

    // Files section.
    if (is_shape) {
        // TiledShapeStMan: nfiles=2, file[0] absent, file[1] present.
        hdr.write_u32(2);
        hdr.write_u8(0);  // has_file[0] = false
        hdr.write_u8(1);  // has_file[1] = true
        hdr.write_u32(1); // TSMFile version
        hdr.write_u32(1); // TSMFile seqnr = 1
        hdr.write_u32(static_cast<std::uint32_t>(file_length));
    } else {
        // All others: nfiles=1, file[0] present.
        hdr.write_u32(1);
        hdr.write_u8(1);  // has_file[0] = true
        hdr.write_u32(1); // TSMFile version
        hdr.write_u32(0); // TSMFile seqnr = 0
        hdr.write_u32(static_cast<std::uint32_t>(file_length));
    }

    // Cubes section.
    if (is_cell) {
        // TiledCellStMan: nrow cubes, each = one row's cell data.
        hdr.write_u32(static_cast<std::uint32_t>(row_count_));
        // Compute per-cube tile size for fileOffset calculation.
        std::uint64_t per_cube_size = 0;
        for (std::size_t i = 0; i < nr_columns; ++i) {
            per_cube_size += cell_pixels * col_pixel_sizes[sorted_indices[i]];
        }
        for (std::uint64_t r = 0; r < row_count_; ++r) {
            write_tsm_cube(hdr, false, ndim, cube_shape, tile_shape, 0,
                           static_cast<std::int32_t>(r * per_cube_size));
        }
    } else if (is_shape) {
        // TiledShapeStMan: 2 cubes — cube 0 = dummy, cube 1 = real.
        hdr.write_u32(2);
        // Cube 0: dummy with nrdim=0, empty shapes, fileNr=-1.
        std::vector<std::int64_t> empty_shape;
        write_tsm_cube(hdr, false, 0, empty_shape, empty_shape, -1, 0);
        // Cube 1: real data cube.
        write_tsm_cube(hdr, true, ndim, cube_shape, tile_shape, 1, 0);
    } else if (is_data) {
        // TiledDataStMan: 1 cube, NOT extensible.
        hdr.write_u32(1);
        write_tsm_cube(hdr, false, ndim, cube_shape, tile_shape, 0, 0);
    } else {
        // TiledColumnStMan: 1 cube, extensible.
        hdr.write_u32(1);
        write_tsm_cube(hdr, true, ndim, cube_shape, tile_shape, 0, 0);
    }

    // End TiledStMan sub-object.
    hdr.patch_u32(tsm_start, static_cast<std::uint32_t>(hdr.size() - tsm_start));

    // --- Post-TiledStMan fields (type-specific) ---
    if (is_shape) {
        // TiledShapeStMan: defaultTileShape + nrUsedRowMap + 3 Block<uInt>.
        // defaultTileShape = [cell_shape..., 1].
        std::vector<std::int64_t> default_ts;
        default_ts.reserve(cell_shape.size() + 1);
        for (const auto s : cell_shape)
            default_ts.push_back(s);
        default_ts.push_back(1);
        write_iposition(hdr, default_ts);

        // nrUsedRowMap_p: for a single contiguous cube, 1 entry suffices.
        const std::uint32_t nr_used = 1;
        hdr.write_u32(nr_used);

        // rowMap_p: last_row of range = nrow - 1.
        write_block_u32(hdr, {static_cast<std::uint32_t>(row_count_ - 1)});
        // cubeMap_p: maps to cube 1.
        write_block_u32(hdr, {1});
        // posMap_p: starting position in cube = nrow - 1.
        write_block_u32(hdr, {static_cast<std::uint32_t>(row_count_ - 1)});
    } else if (is_data) {
        // TiledDataStMan v2: nrrowLast_p (uInt64) + 3 Blocks.
        hdr.write_u64(row_count_); // nrrowLast_p

        // rowMap_p (Block<rownr_t = uInt64>): [0].
        write_block_u64(hdr, {0});
        // cubeMap_p (Block<uInt>): [0].
        write_block_u32(hdr, {0});
        // posMap_p (Block<uInt>): [0].
        write_block_u32(hdr, {0});
    }

    // End root object.
    hdr.patch_u32(root_start + 4, static_cast<std::uint32_t>(hdr.size() - root_start - 4));

    // Write .f0 header file.
    {
        const auto path =
            std::filesystem::path(table_dir) / ("table.f" + std::to_string(sequence_number));
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            throw std::runtime_error("cannot create TSM header file: " + path.string());
        }
        const auto bytes = hdr.take_bytes();
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
}

} // namespace casacore_mini
