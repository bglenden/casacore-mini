// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Typed nested keyword-record representation and `showtableinfo` parser.
/// @ingroup records
/// @addtogroup records
/// @{

class KeywordRecord;
struct KeywordArray;

///
/// Typed keyword value used by nested keyword records.
///
///
///
///
///
/// `KeywordValue` is a discriminated union over the types that can appear
/// in a casacore keyword record as parsed from `showtableinfo` text:
/// `bool`, `int64_t`, `double`, `string`, an array of values
/// (`KeywordArray`), or a nested record (`KeywordRecord`).
///
/// Arrays and nested records are stored via `shared_ptr` to keep the value
/// copyable without deep-copying the potentially large sub-structures.
/// The factory methods `from_array` and `from_record` move-construct the
/// shared pointer from an owned value.
///
/// Equality comparison (`operator==`) recurses into array elements and nested
/// record entries, comparing pointed-to values rather than pointer identity.
///
class KeywordValue {
  public:
    /// Shared pointer type for array values.
    using array_ptr = std::shared_ptr<KeywordArray>;
    /// Shared pointer type for nested record values.
    using record_ptr = std::shared_ptr<KeywordRecord>;
    /// Variant storage backing the value.
    using storage_type =
        std::variant<bool, std::int64_t, double, std::string, array_ptr, record_ptr>;

    /// Construct default value (`bool(false)`).
    KeywordValue();
    /// Construct `bool` value.
    explicit KeywordValue(bool value);
    /// Construct signed integer value.
    explicit KeywordValue(std::int64_t value);
    /// Construct floating-point value.
    explicit KeywordValue(double value);
    /// Construct string value.
    explicit KeywordValue(std::string value);
    /// Construct string value from C string.
    explicit KeywordValue(const char* value);

    /// Construct from owned array moved into shared ownership.
    [[nodiscard]] static KeywordValue from_array(KeywordArray value);
    /// Construct from owned nested record moved into shared ownership.
    [[nodiscard]] static KeywordValue from_record(KeywordRecord value);

    /// Access underlying variant storage.
    [[nodiscard]] const storage_type& storage() const noexcept;
    /// Equality by type and value, with pointer alternatives compared by pointed value.
    [[nodiscard]] bool operator==(const KeywordValue& other) const;

  private:
    explicit KeywordValue(array_ptr value);
    explicit KeywordValue(record_ptr value);

    storage_type storage_;
};

///
/// Ordered list of keyword values.
///
///
///
///
///
/// `KeywordArray` is a simple ordered sequence of `KeywordValue` elements.
/// It appears in the text-model representation when a casacore keyword stores
/// a one-dimensional array, such as `QuantumUnits` or `projection_parameters`.
///
struct KeywordArray {
    /// Values in stored order.
    std::vector<KeywordValue> elements;

    [[nodiscard]] bool operator==(const KeywordArray& other) const;
};

///
/// Ordered keyword record with deterministic insertion ordering.
///
///
///
///
///
/// `KeywordRecord` maintains a list of `(key, value)` entries in insertion
/// order.  It does not use a hash map, so the entry ordering matches the
/// original `showtableinfo` output and round-trips predictably through the
/// text-to-binary conversion layer.
///
/// `set()` inserts a new key or replaces an existing one while preserving its
/// position.  `find()` returns a const pointer to the value or `nullptr` if
/// the key is absent.
///
///
/// @par Example
/// @code{.cpp}
///   KeywordRecord rec;
///   rec.set("type", KeywordValue(std::string("epoch")));
///   rec.set("refer", KeywordValue(std::string("UTC")));
///   const auto* v = rec.find("type");  // non-null
/// @endcode
///
class KeywordRecord {
  public:
    /// Stored key/value entry type.
    using entry = std::pair<std::string, KeywordValue>;

    /// Insert or replace a key/value pair.
    ///
    /// Existing keys retain position when replaced.
    void set(std::string_view key, KeywordValue value);
    /// Find value by key; returns `nullptr` if absent.
    [[nodiscard]] const KeywordValue* find(std::string_view key) const;
    /// Number of entries in insertion order.
    [[nodiscard]] std::size_t size() const noexcept;
    /// Ordered entry view.
    [[nodiscard]] const std::vector<entry>& entries() const noexcept;

    [[nodiscard]] bool operator==(const KeywordRecord& other) const;

  private:
    std::vector<entry> entries_;
};

///
/// Parsed keyword structures from a `showtableinfo` document.
///
///
///
///
///
/// `ShowtableinfoKeywords` aggregates the table-level keyword record and the
/// per-column keyword records extracted by `parse_showtableinfo_keywords`.
///
/// - `table_keywords` holds the `Table Keywords` section.
/// - `column_keywords` is a vector of `(column_name, keywords_record)` pairs
///   in first-seen order from the document.
///
/// Use `find_column()` to locate the keyword record for a specific column name.
///
struct ShowtableinfoKeywords {
    /// Top-level `Table Keywords` record.
    KeywordRecord table_keywords;
    /// Column keyword records in first-seen order: `(column_name, keywords_record)`.
    std::vector<std::pair<std::string, KeywordRecord>> column_keywords;

    /// Find a parsed column keyword record by column name.
    [[nodiscard]] const KeywordRecord* find_column(std::string_view column_name) const;
    [[nodiscard]] bool operator==(const ShowtableinfoKeywords& other) const;
};

/// Parse table/column keyword records from `showtableinfo` textual output.
///
///
/// Parsed section coverage:
/// - `Table Keywords` nested records
/// - `Column <name>` keyword records
/// - scalar values: `Bool`, `Int`, `Float`, `Double`, `String`, `Table`
/// - one-line bracket arrays declared as `<Type> array with shape [...]`
///
/// Missing `Keywords of main table` yields an empty result.
///
///
/// @throws std::runtime_error on malformed keyword record structure.
[[nodiscard]] ShowtableinfoKeywords
parse_showtableinfo_keywords(std::string_view showtableinfo_text);

/// @}
} // namespace casacore_mini
