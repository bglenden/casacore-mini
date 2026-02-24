#include "casacore_mini/table_desc.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"

#include <cstdint>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <string>
#include <vector>

namespace casacore_mini {
namespace {

/// Read an embedded AipsIO object header (no magic prefix).
/// Accepts both magic-prefixed and bare forms for compatibility.
[[nodiscard]] AipsIoObjectHeader read_embedded_header(AipsIoReader& reader) {
    AipsIoObjectHeader header;
    const auto first = reader.read_u32();
    if (first == kAipsIoMagic) {
        header.object_length = reader.read_u32();
    } else {
        header.object_length = first;
    }
    header.object_type = reader.read_string();
    header.object_version = reader.read_u32();
    return header;
}

/// Read an IPosition from the stream.
[[nodiscard]] std::vector<std::int64_t> read_iposition(AipsIoReader& reader) {
    const auto header = read_embedded_header(reader);
    if (header.object_type != "IPosition") {
        throw std::runtime_error("expected IPosition, got: " + header.object_type);
    }

    const auto nelements = static_cast<std::size_t>(reader.read_u32());
    constexpr std::size_t kMinBytesPerDim = 4;
    if (nelements > reader.remaining() / kMinBytesPerDim) {
        throw std::runtime_error("IPosition element count exceeds remaining bytes");
    }

    std::vector<std::int64_t> values;
    values.reserve(nelements);
    for (std::size_t i = 0; i < nelements; ++i) {
        if (header.object_version >= 2U) {
            values.push_back(reader.read_i64());
        } else {
            values.push_back(reader.read_i32());
        }
    }
    return values;
}

/// Read a TableRecord.
///
/// Wire format: TableRecord header (OL + "TableRecord" + ver) followed by
/// the Record body (RecordDesc + recordType + field values). The Record body
/// uses the embedded format that `read_aipsio_embedded_record` handles.
[[nodiscard]] Record read_table_record(AipsIoReader& reader) {
    const auto header = read_embedded_header(reader);
    if (header.object_type != "TableRecord") {
        throw std::runtime_error("expected TableRecord, got: " + header.object_type);
    }
    return read_aipsio_record_body(reader);
}

/// Skip type-specific default value. Grouped by byte width to avoid
/// bugprone-branch-clone warnings from consecutive identical switch arms.
void skip_scalar_default(AipsIoReader& reader, DataType dtype) {
    const auto code = static_cast<std::int32_t>(dtype);
    if (code == static_cast<std::int32_t>(DataType::tp_bool) ||
        code == static_cast<std::int32_t>(DataType::tp_char) ||
        code == static_cast<std::int32_t>(DataType::tp_uchar)) {
        reader.skip(1);
    } else if (code == static_cast<std::int32_t>(DataType::tp_short) ||
               code == static_cast<std::int32_t>(DataType::tp_ushort)) {
        reader.skip(2);
    } else if (code == static_cast<std::int32_t>(DataType::tp_int) ||
               code == static_cast<std::int32_t>(DataType::tp_uint) ||
               code == static_cast<std::int32_t>(DataType::tp_float)) {
        reader.skip(4);
    } else if (code == static_cast<std::int32_t>(DataType::tp_int64) ||
               code == static_cast<std::int32_t>(DataType::tp_double) ||
               code == static_cast<std::int32_t>(DataType::tp_complex)) {
        reader.skip(8);
    } else if (code == static_cast<std::int32_t>(DataType::tp_dcomplex)) {
        reader.skip(16);
    } else if (code == static_cast<std::int32_t>(DataType::tp_string) ||
               code == static_cast<std::int32_t>(DataType::tp_table)) {
        static_cast<void>(reader.read_string());
    }
}

/// Read a single column descriptor.
[[nodiscard]] ColumnDesc read_column_desc(AipsIoReader& reader) {
    const auto header = read_embedded_header(reader);
    const auto& type_str = header.object_type;

    ColumnDesc col;
    col.type_string = type_str;
    col.version = header.object_version;

    if (type_str.find("ScalarColumnDesc") != std::string::npos) {
        col.kind = ColumnKind::scalar;
    } else if (type_str.find("ArrayColumnDesc") != std::string::npos) {
        col.kind = ColumnKind::array;
    } else {
        throw std::runtime_error("unknown column descriptor type: " + type_str);
    }

    // BaseColumnDesc fields
    col.name = reader.read_string();
    col.comment = reader.read_string();
    col.dm_type = reader.read_string();
    col.dm_group = reader.read_string();
    col.data_type = static_cast<DataType>(reader.read_i32());
    col.options = reader.read_i32();
    col.ndim = reader.read_i32();

    if (col.ndim != 0) {
        col.shape = read_iposition(reader);
    }

    col.max_length = reader.read_i32();
    col.keywords = read_table_record(reader);

    // Type-specific suffix
    if (col.kind == ColumnKind::scalar) {
        // ScalarColumnDesc: version(u32) + default_value
        const auto scalar_ver = reader.read_u32();
        static_cast<void>(scalar_ver);
        skip_scalar_default(reader, col.data_type);
    } else {
        // ArrayColumnDesc: version(u32) + isFixedShape(u8)
        const auto array_ver = reader.read_u32();
        static_cast<void>(array_ver);
        static_cast<void>(reader.read_u8()); // isFixedShape flag
    }

    return col;
}

/// Read the TableDesc section.
[[nodiscard]] TableDesc read_table_desc(AipsIoReader& reader) {
    const auto header = read_embedded_header(reader);
    if (header.object_type != "TableDesc") {
        throw std::runtime_error("expected TableDesc, got: " + header.object_type);
    }

    TableDesc desc;
    desc.version = header.object_version;

    if (desc.version >= 2U) {
        static_cast<void>(reader.read_string()); // userType string
    }

    desc.name = reader.read_string();
    desc.comment = reader.read_string();
    desc.keywords = read_table_record(reader);
    desc.private_keywords = read_table_record(reader);

    const auto ncol = reader.read_u32();
    desc.columns.reserve(static_cast<std::size_t>(ncol));
    for (std::uint32_t i = 0; i < ncol; ++i) {
        desc.columns.push_back(read_column_desc(reader));
    }

    return desc;
}

/// Read a per-column SM assignment (tableVer=1 format).
[[nodiscard]] ColumnManagerSetup read_column_setup_v1(AipsIoReader& reader, bool is_array) {
    ColumnManagerSetup setup;
    static_cast<void>(reader.read_u32());         // preamble (always 1)
    static_cast<void>(read_table_record(reader)); // per-column keywords (discard)
    setup.column_name = reader.read_string();
    setup.sequence_number = reader.read_u32();
    setup.flags = reader.read_u32();

    if (is_array) {
        const auto has_shape_byte = reader.read_u8();
        setup.has_shape = has_shape_byte != 0U;
        if (setup.has_shape) {
            setup.shape = read_iposition(reader);
        }
    }

    return setup;
}

/// Read a per-column SM assignment (tableVer=2 format).
[[nodiscard]] ColumnManagerSetup read_column_setup_v2(AipsIoReader& reader, bool is_array) {
    ColumnManagerSetup setup;
    static_cast<void>(reader.read_u32()); // version (always 2)
    setup.column_name = reader.read_string();
    setup.sequence_number = reader.read_u32();
    setup.flags = reader.read_u32();

    if (is_array) {
        const auto has_shape_byte = reader.read_u8();
        setup.has_shape = has_shape_byte != 0U;
        if (setup.has_shape) {
            setup.shape = read_iposition(reader);
        }
    }

    return setup;
}

/// Determine if a column descriptor is array-type.
[[nodiscard]] bool is_array_column(const ColumnDesc& col) {
    return col.kind == ColumnKind::array;
}

} // namespace

TableDatFull parse_table_dat_full(const std::span<const std::uint8_t> bytes) {
    AipsIoReader reader(bytes);

    // Outer Table object header (with magic).
    const auto table_header = reader.read_object_header();
    if (table_header.object_type != "Table") {
        throw std::runtime_error("table.dat root object is not Table");
    }

    TableDatFull result;
    result.table_version = table_header.object_version;

    if (result.table_version <= 2U) {
        result.row_count = reader.read_u32();
    } else {
        result.row_count = reader.read_u64();
    }

    const auto endian_flag = reader.read_u32();
    if (endian_flag > 1U) {
        throw std::runtime_error("table.dat endian flag is not 0/1");
    }
    result.big_endian = endian_flag == 0U;
    result.table_type = reader.read_string();

    // TableDesc section.
    result.table_desc = read_table_desc(reader);

    // Post-TD section (format depends on table_version).
    if (result.table_version == 1U) {
        // v1: Table keywords TR, sentinel, nrow, SM entries
        result.table_keywords_v1 = read_table_record(reader);
    }

    // Sentinel (-2 as i32).
    const auto sentinel = reader.read_i32();
    if (sentinel != -2) {
        throw std::runtime_error("expected sentinel -2 in post-TD section, got: " +
                                 std::to_string(sentinel));
    }

    result.post_td_row_count = reader.read_u32();

    // Storage manager descriptors.
    static_cast<void>(reader.read_u32()); // sm_seq_count (informational)
    const auto n_sm_entries = reader.read_u32();
    result.storage_managers.reserve(static_cast<std::size_t>(n_sm_entries));
    for (std::uint32_t i = 0; i < n_sm_entries; ++i) {
        StorageManagerSetup sm;
        sm.type_name = reader.read_string();
        sm.sequence_number = reader.read_u32();
        result.storage_managers.push_back(std::move(sm));
    }

    // Per-column SM assignments (one per column in TableDesc order).
    const auto ncol = result.table_desc.columns.size();
    result.column_setups.reserve(ncol);
    for (std::size_t i = 0; i < ncol; ++i) {
        const bool is_array = is_array_column(result.table_desc.columns[i]);
        if (result.table_version == 1U) {
            result.column_setups.push_back(read_column_setup_v1(reader, is_array));
        } else {
            result.column_setups.push_back(read_column_setup_v2(reader, is_array));
        }
    }

    return result;
}

TableDatFull read_table_dat_full(const std::string_view table_dat_path) {
    std::ifstream input(std::string(table_dat_path), std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open table.dat file: " + std::string(table_dat_path));
    }

    std::vector<std::uint8_t> file_bytes((std::istreambuf_iterator<char>(input)),
                                         std::istreambuf_iterator<char>());
    return parse_table_dat_full(file_bytes);
}

} // namespace casacore_mini
