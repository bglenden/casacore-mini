#pragma once

#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Encode casacore `Record` objects into canonical `AipsIO` byte streams.

/// Write a casacore-compatible `Record` into an `AipsIO` byte stream.
///
/// This produces the binary format consumed by `read_aipsio_record`: a Record
/// object header, RecordDesc, recordType flag, and encoded field values.
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
