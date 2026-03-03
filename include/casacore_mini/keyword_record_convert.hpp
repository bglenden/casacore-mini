// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/keyword_record.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Bidirectional conversion between text `KeywordRecord` and binary
/// `Record` models.

/// <synopsis>
/// This header provides four free functions that convert between the two
/// keyword-record representations used in casacore-mini:
///
/// - The **text model** (`KeywordRecord` / `KeywordValue` / `KeywordArray`)
///   is built by parsing `showtableinfo` output and holds only the types
///   distinguishable in that text format.
/// - The **binary model** (`Record` / `RecordValue`) is produced by decoding
///   AipsIO byte streams and preserves the full type vocabulary of the
///   casacore `DataType` enum.
///
/// ## Type promotion rules (KeywordRecord -> Record)
///
/// | Text type          | Binary type        |
/// |--------------------|--------------------|
/// | `bool`             | `bool`             |
/// | `int64_t`          | `int64_t`          |
/// | `double`           | `double`           |
/// | `string`           | `string`           |
/// | `KeywordArray`     | 1-D typed array    |
/// | `KeywordRecord`    | nested `Record`    |
///
/// ## Type demotion rules (Record -> KeywordRecord)
///
/// | Binary type         | Text type          | Notes                    |
/// |---------------------|--------------------|--------------------------|
/// | `bool`              | `bool`             |                          |
/// | All integer types   | `int64_t`          | Widened/cast to int64_t  |
/// | All float types     | `double`           | Widened to double        |
/// | `string`            | `string`           |                          |
/// | `complex<T>`        | `string`           | "(real,imag)" format     |
/// | Typed N-D array     | flat `KeywordArray`| Shape information lost   |
/// | `RecordList`        | flat `KeywordArray`| Heterogeneous flattened  |
/// | Nested `Record`     | `KeywordRecord`    |                          |
/// </synopsis>

/// Convert a text-model `KeywordValue` to a binary-model `RecordValue`.
///
/// Arrays use the element types from the first element to determine the
/// target array type. Empty arrays produce empty `string_array` by default.
[[nodiscard]] RecordValue keyword_value_to_record_value(const KeywordValue& kv);

/// Convert a text-model `KeywordRecord` to a binary-model `Record`.
[[nodiscard]] Record keyword_record_to_record(const KeywordRecord& kr);

/// Convert a binary-model `RecordValue` to a text-model `KeywordValue`.
///
/// Complex values are serialized as `"(real,imag)"` strings.
/// Multi-dimensional arrays lose shape information and are flattened.
[[nodiscard]] KeywordValue record_value_to_keyword_value(const RecordValue& rv);

/// Convert a binary-model `Record` to a text-model `KeywordRecord`.
[[nodiscard]] KeywordRecord record_to_keyword_record(const Record& rec);

} // namespace casacore_mini
