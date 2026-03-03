// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_desc_writer.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {
namespace {

[[nodiscard]] std::uint32_t checked_u32(std::size_t value, std::string_view field_name) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t checked_u32_from_u64(std::uint64_t value, std::string_view field_name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::int32_t checked_i32(std::int64_t value, std::string_view field_name) {
    if (value < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds int32 range");
    }
    return static_cast<std::int32_t>(value);
}

/// Write object header with placeholder length, return length offset.
[[nodiscard]] std::size_t begin_object(AipsIoWriter& writer, std::string_view type_name,
                                       std::uint32_t version) {
    const auto length_offset = writer.size();
    writer.write_u32(0); // placeholder
    writer.write_string(type_name);
    writer.write_u32(version);
    return length_offset;
}

/// Patch object length field.
void end_object(AipsIoWriter& writer, std::size_t length_offset) {
    const auto length = writer.size() - length_offset;
    writer.patch_u32(length_offset, checked_u32(length, "AipsIO object length"));
}

/// Write an IPosition (version 1, i32 dimensions).
void write_iposition(AipsIoWriter& writer, const std::vector<std::int64_t>& shape) {
    const auto length_offset = begin_object(writer, "IPosition", 1U);
    writer.write_u32(checked_u32(shape.size(), "IPosition rank"));
    for (const auto dim : shape) {
        writer.write_i32(checked_i32(dim, "IPosition dimension"));
    }
    end_object(writer, length_offset);
}

/// Write a TableRecord (thin wrapper: TableRecord header, then Record body).
/// Sub-records are also written as "TableRecord" to match casacore's expectation.
void write_table_record(AipsIoWriter& writer, const Record& record) {
    const auto length_offset = begin_object(writer, "TableRecord", 1U);
    write_aipsio_table_record_body(writer, record);
    end_object(writer, length_offset);
}

/// Write scalar default value (zeros/empty).
void write_scalar_default(AipsIoWriter& writer, DataType dtype) {
    // Grouped by wire size to avoid bugprone-branch-clone warnings.
    const auto code = static_cast<std::int32_t>(dtype);
    if (code == static_cast<std::int32_t>(DataType::tp_bool) ||
        code == static_cast<std::int32_t>(DataType::tp_char) ||
        code == static_cast<std::int32_t>(DataType::tp_uchar)) {
        writer.write_u8(0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_short) ||
               code == static_cast<std::int32_t>(DataType::tp_ushort)) {
        writer.write_i16(0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_int) ||
               code == static_cast<std::int32_t>(DataType::tp_uint)) {
        writer.write_i32(0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_int64)) {
        writer.write_i64(0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_float)) {
        writer.write_f32(0.0F);
    } else if (code == static_cast<std::int32_t>(DataType::tp_double)) {
        writer.write_f64(0.0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_complex)) {
        writer.write_f32(0.0F);
        writer.write_f32(0.0F);
    } else if (code == static_cast<std::int32_t>(DataType::tp_dcomplex)) {
        writer.write_f64(0.0);
        writer.write_f64(0.0);
    } else if (code == static_cast<std::int32_t>(DataType::tp_string) ||
               code == static_cast<std::int32_t>(DataType::tp_table)) {
        writer.write_string("");
    }
}

/// Write a single column descriptor.
void write_column_desc(AipsIoWriter& writer, const ColumnDesc& col) {
    const auto length_offset = begin_object(writer, col.type_string, col.version);

    // BaseColumnDesc fields.
    writer.write_string(col.name);
    writer.write_string(col.comment);
    writer.write_string(col.dm_type);
    writer.write_string(col.dm_group);
    writer.write_i32(static_cast<std::int32_t>(col.data_type));
    writer.write_i32(col.options);
    writer.write_i32(col.ndim);

    if (col.ndim != 0) {
        write_iposition(writer, col.shape);
    }

    writer.write_i32(col.max_length);
    write_table_record(writer, col.keywords);

    // Type-specific suffix.
    if (col.kind == ColumnKind::scalar) {
        writer.write_u32(1U); // ScalarColumnDesc version
        write_scalar_default(writer, col.data_type);
    } else {
        writer.write_u32(1U); // ArrayColumnDesc version
        // isFixedShape: true if shape is non-empty and options & 4 set
        const auto is_fixed = static_cast<std::uint8_t>((col.options & 4) != 0 ? 1U : 0U);
        writer.write_u8(is_fixed);
    }

    end_object(writer, length_offset);
}

/// Write the TableDesc section.
void write_table_desc(AipsIoWriter& writer, const TableDesc& desc) {
    const auto length_offset = begin_object(writer, "TableDesc", desc.version);

    if (desc.version >= 2U) {
        writer.write_string(""); // userType string (empty)
    }

    writer.write_string(desc.name);
    writer.write_string(desc.comment);
    write_table_record(writer, desc.keywords);
    write_table_record(writer, desc.private_keywords);

    writer.write_u32(checked_u32(desc.columns.size(), "TableDesc column count"));
    for (const auto& col : desc.columns) {
        write_column_desc(writer, col);
    }

    end_object(writer, length_offset);
}

/// Write per-column SM assignment (version 2 format).
///
/// Wire format: PlainColumn version(2) + name + derived class version(1) + seqnr
/// Array columns also write: shapeColDef_p (Bool) + optional shapeCol_p (IPosition).
/// casacore's ArrayColumnData::putFileDerived writes shapeColDef_p=true when the
/// column has a fixed shape defined, followed by the shape IPosition. When casacore
/// reads this back, if shapeColDef_p is true + FixedShape option is set, it calls
/// setFixedShapeColumn(shapeCol_p).
void write_column_setup_v2(AipsIoWriter& writer, const ColumnManagerSetup& setup, bool is_array) {
    writer.write_u32(2U); // PlainColumn version
    writer.write_string(setup.column_name);
    writer.write_u32(1U); // derived class version (ScalarColumnData / ArrayColumnData)
    writer.write_u32(setup.sequence_number);

    if (is_array) {
        const bool shape_defined = setup.has_shape && !setup.shape.empty();
        writer.write_u8(shape_defined ? 1U : 0U); // shapeColDef_p
        if (shape_defined) {
            write_iposition(writer, setup.shape);
        }
    }
}

} // namespace

void write_table_dat_full(AipsIoWriter& writer, const TableDatFull& table) {
    constexpr std::uint32_t kTableVersion = 2U;

    // Outer Table object header (with magic).
    writer.write_u32(kAipsIoMagic);
    const auto length_offset = writer.size();
    writer.write_u32(0); // placeholder for object_length
    writer.write_string("Table");
    writer.write_u32(kTableVersion);

    // Header fields.
    writer.write_u32(checked_u32_from_u64(table.row_count, "table.dat row_count"));
    writer.write_u32(table.big_endian ? 0U : 1U);
    writer.write_string(table.table_type);

    // TableDesc section.
    write_table_desc(writer, table.table_desc);

    // Post-TD section (always v2 format: no Table keywords TR).
    writer.write_i32(-2); // sentinel

    writer.write_u32(checked_u32_from_u64(table.row_count, "post-TD row_count"));

    // SM descriptors.
    writer.write_u32(checked_u32(table.storage_managers.size(), "SM seq count"));
    writer.write_u32(checked_u32(table.storage_managers.size(), "SM entry count"));
    for (const auto& sm : table.storage_managers) {
        writer.write_string(sm.type_name);
        writer.write_u32(sm.sequence_number);
    }

    // Per-column SM assignments — written in column insertion order to match
    // casacore's ColumnSet::putFile which iterates via getColumn(i) using
    // the TableDesc column sequence (colSeq_p).
    for (std::size_t i = 0; i < table.column_setups.size(); ++i) {
        const bool is_array = i < table.table_desc.columns.size() &&
                              table.table_desc.columns[i].kind == ColumnKind::array;
        write_column_setup_v2(writer, table.column_setups[i], is_array);
    }

    // Per-SM data blobs (u32 length + raw bytes, matching reader's format).
    for (const auto& sm : table.storage_managers) {
        writer.write_u32(checked_u32(sm.data_blob.size(), "SM data blob length"));
        for (const auto b : sm.data_blob) {
            writer.write_u8(b);
        }
    }

    // Patch outer object length.
    const auto length = writer.size() - length_offset;
    writer.patch_u32(length_offset, checked_u32(length, "table.dat object length"));
}

std::vector<std::uint8_t> serialize_table_dat_full(const TableDatFull& table) {
    AipsIoWriter writer;
    write_table_dat_full(writer, table);
    return writer.take_bytes();
}

} // namespace casacore_mini
