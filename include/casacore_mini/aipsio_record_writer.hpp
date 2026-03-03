// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Encode casacore `Record` objects into canonical `AipsIO` byte streams.

/// Write a casacore-compatible `Record` into an `AipsIO` byte stream.
///
/// <synopsis>
/// Produces the binary format consumed by `read_aipsio_record`: a Record
/// object header, RecordDesc, recordType flag, and encoded field values.
/// The output is byte-for-byte compatible with the format written by
/// casacore's `RecordRep::putRecord`.
///
/// Three entry points are provided:
/// - `write_aipsio_record` — writes a root record with `0xBEBEBEBE` magic
///   prefix (mirrors `read_aipsio_record`).
/// - `write_aipsio_embedded_record` — writes a nested record without magic
///   (mirrors `read_aipsio_embedded_record`).
/// - `write_aipsio_record_body` — writes only the body (RecordDesc +
///   recordType + fields), used for `TableRecord` inner payloads.
/// - `write_aipsio_table_record_body` — like `_body` but encodes nested
///   records as `"TableRecord"` AipsIO type, required for table descriptor
///   keywords.
///
/// Wire limitations:
/// - `uint64_t` scalar values have no `DataType` representation in casacore
///   and will throw.
/// - `list_ptr` alternatives also throw (no casacore wire encoding).
/// - Bool arrays decoded by `read_aipsio_record` become `int32_array` and
///   the writer emits them as `Array<Int>`, not `Array<Bool>`.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   Record rec;
///   rec.fields["NROW"] = RecordValue(std::int32_t{100});
///   AipsIoWriter writer;
///   write_aipsio_record(writer, rec);
///   auto bytes = writer.take_bytes();
/// </srcblock>
/// </example>
///
/// @note `uint64_t` scalar values are not representable in casacore's DataType
/// enum and will throw. `list_ptr` alternatives also throw as they have no
/// casacore wire encoding.
///
/// @note Bool arrays decoded by `read_aipsio_record` become `int32_array` and
/// are indistinguishable from genuine int32 arrays. The writer emits them as
/// `Array<Int>`, not `Array<Bool>`.
///
/// @throws std::runtime_error on unsupported value types, nesting depth
/// exceeded, or when counts/dimensions cannot be represented in the casacore
/// wire ranges (`uInt`/`Int`).
void write_aipsio_record(AipsIoWriter& writer, const Record& record);

/// Write an embedded `Record` object (no leading magic prefix).
///
/// Use this for Records nested inside other AipsIO objects (e.g. TableRecord).
///
/// @throws std::runtime_error on unsupported value types or nesting depth exceeded.
void write_aipsio_embedded_record(AipsIoWriter& writer, const Record& record);

/// Write a `Record` body (RecordDesc + recordType + field values) without
/// any wrapping `Record` object header.
///
/// Use this for the inner body of casacore `TableRecord` objects.
///
/// @throws std::runtime_error on unsupported value types or nesting depth exceeded.
void write_aipsio_record_body(AipsIoWriter& writer, const Record& record);

/// Write a `Record` body where sub-records are encoded as `TableRecord`.
///
/// Casacore's `TableRecord` hierarchy expects nested records to use the
/// `"TableRecord"` AipsIO type name, not `"Record"`.  Use this variant for
/// table descriptor keywords and private keywords.
///
/// @throws std::runtime_error on unsupported value types or nesting depth exceeded.
void write_aipsio_table_record_body(AipsIoWriter& writer, const Record& record);

} // namespace casacore_mini
