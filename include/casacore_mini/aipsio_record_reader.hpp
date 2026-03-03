// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Decode casacore `Record` objects from canonical `AipsIO` byte streams.

/// Read a casacore-encoded `Record` from an `AipsIO` byte stream.
///
/// 
/// Decodes the binary format produced by casacore `RecordRep::putRecord`,
/// including the RecordDesc, field values, nested sub-records, and arrays.
/// The reader is positioned just after the terminating end-marker when the
/// function returns.
///
/// Supported object types encountered during decode: `Record`, `RecordDesc`,
/// `Array`, `IPosition`.
///
/// Type promotion notes:
/// - `Char` and `uChar` scalars and arrays are losslessly promoted to
///   `int16_t` and `uint16_t` respectively because
///   `RecordValue` has no 8-bit type slot.
///
/// Entry points:
/// - `read_aipsio_record` — root record with `0xBEBEBEBE` magic prefix.
/// - `read_aipsio_embedded_record` — nested record without magic prefix.
/// - `read_aipsio_record_body` — raw body (no wrapping Record header at all),
///   used when a `TableRecord` header already wraps the contents.
/// 
///
/// @par Example
/// @code{.cpp}
///   auto raw = read_file("table_keywords.bin");
///   AipsIoReader reader(raw);
///   Record rec = read_aipsio_record(reader);
/// @endcode
/// 
///
/// @note `Char` and `uChar` scalars and arrays are losslessly promoted to
/// `int16_t` and `uint16_t` respectively since `RecordValue` has no 8-bit type.
///
/// @throws std::runtime_error on malformed input, unsupported object type,
/// truncated data, negative size fields, or nesting depth limit exceeded.
[[nodiscard]] Record read_aipsio_record(AipsIoReader& reader);

/// Read an embedded `Record` object from an `AipsIO` byte stream.
///
/// Same as `read_aipsio_record` but expects no leading `0xBEBEBEBE` magic
/// prefix. Use this for Records nested inside other AipsIO objects.
///
/// @throws std::runtime_error on malformed input.
[[nodiscard]] Record read_aipsio_embedded_record(AipsIoReader& reader);

/// Read a `Record` body (RecordDesc + recordType + field values) without
/// any wrapping `Record` object header.
///
/// Use this for the inner body of casacore `TableRecord` objects where the
/// TableRecord header wraps the Record contents directly.
///
/// @throws std::runtime_error on malformed input.
[[nodiscard]] Record read_aipsio_record_body(AipsIoReader& reader);

} // namespace casacore_mini
