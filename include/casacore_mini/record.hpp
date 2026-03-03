// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <complex>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Public in-memory record model used by persistence-facing APIs.

class Record;
struct RecordList;

/// <summary>
/// Shape-aware typed array value used in Record fields.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// RecordArray stores a multidimensional array of a single element type
/// in flattened Fortran-order (first axis varies fastest), together with
/// a shape vector describing the array extents.
///
/// The shape invariant: <src>elements.size()</src> must equal the product
/// of all values in <src>shape</src>.  An empty <src>shape</src> implies
/// zero elements.
///
/// RecordArray is used as one of the array alternatives inside
/// <linkto>RecordValue</linkto>.  Public APIs work with
/// <src>uint64_t</src> extents; signed IPosition semantics are handled
/// internally during I/O conversion.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   // Build a 3x4 double array
///   RecordArray<double> arr;
///   arr.shape = {3, 4};
///   arr.elements.assign(12, 0.0);   // 12 = 3*4
///
///   // Wrap in a RecordValue and store in a Record
///   Record rec;
///   rec.set("data", RecordValue(std::move(arr)));
/// </srcblock>
/// </example>
template <typename element_t> struct RecordArray {
    /// Array extents. Rank is `shape.size()`.
    ///
    /// Public APIs use unsigned extents (`uint64_t`); wire formats with signed
    /// `IPosition` semantics are handled internally during I/O conversion.
    std::vector<std::uint64_t> shape;
    /// Flattened values in Fortran-order.
    std::vector<element_t> elements;

    /// Compare both shape and flattened element payload.
    [[nodiscard]] bool operator==(const RecordArray& other) const;
};

/// <summary>
/// Tagged union of all persistence-facing value types used by Record fields.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// RecordValue is a small, deterministic tagged union that covers every
/// numeric and string type that casacore records can contain.  It supports
/// both scalar and multidimensional array variants, as well as nested
/// <linkto>Record</linkto> and <linkto>RecordList</linkto> containers.
///
/// Both <src>std::complex<float></src> and <src>std::complex<double></src>
/// are included to reflect casacore's numeric value domains precisely.
///
/// The underlying storage is a <src>std::variant</src> accessible through
/// <src>storage()</src>.  Callers should use
/// <src>std::holds_alternative<T></src> and <src>std::get_if<T></src> to
/// inspect the active alternative.
///
/// Nested records and lists are stored behind <src>shared_ptr</src> so that
/// <src>RecordValue</src> remains cheaply copyable; equality comparisons
/// dereference the pointer and compare by value.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   // Scalar construction
///   RecordValue v_int(std::int32_t{42});
///   RecordValue v_str(std::string{"hello"});
///
///   // Array construction
///   RecordArray<float> arr;
///   arr.shape = {2, 3};
///   arr.elements = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
///   RecordValue v_arr(std::move(arr));
///
///   // Introspection
///   if (auto* p = std::get_if<std::int32_t>(&v_int.storage()))
///       std::cout << *p << "\n";
///
///   // Nested record
///   Record inner;
///   inner.set("key", RecordValue(1.0));
///   RecordValue v_rec = RecordValue::from_record(std::move(inner));
/// </srcblock>
/// </example>
///
/// <note role="caution">
/// Array constructors validate the shape-product invariant and throw
/// <src>std::invalid_argument</src> if
/// <src>elements.size() != product(shape)</src>.
/// </note>
class RecordValue {
  public:
    /// Signed 16-bit multidimensional array.
    using int16_array = RecordArray<std::int16_t>;
    /// Unsigned 16-bit multidimensional array.
    using uint16_array = RecordArray<std::uint16_t>;
    /// Signed 32-bit multidimensional array.
    using int32_array = RecordArray<std::int32_t>;
    /// Unsigned 32-bit multidimensional array.
    using uint32_array = RecordArray<std::uint32_t>;
    /// Signed 64-bit multidimensional array.
    using int64_array = RecordArray<std::int64_t>;
    /// Unsigned 64-bit multidimensional array.
    using uint64_array = RecordArray<std::uint64_t>;
    /// 32-bit float multidimensional array.
    using float_array = RecordArray<float>;
    /// 64-bit float multidimensional array.
    using double_array = RecordArray<double>;
    /// UTF-8 string multidimensional array.
    using string_array = RecordArray<std::string>;
    /// Complex<float> multidimensional array.
    using complex64_array = RecordArray<std::complex<float>>;
    /// Complex<double> multidimensional array.
    using complex128_array = RecordArray<std::complex<double>>;

    /// Shared pointer type for list values.
    using list_ptr = std::shared_ptr<RecordList>;
    /// Shared pointer type for nested record values.
    using record_ptr = std::shared_ptr<Record>;
    /// Variant storage backing the value.
    using storage_type =
        std::variant<bool, std::int16_t, std::uint16_t, std::int32_t, std::uint32_t, std::int64_t,
                     std::uint64_t, float, double, std::string, std::complex<float>,
                     std::complex<double>, int16_array, uint16_array, int32_array, uint32_array,
                     int64_array, uint64_array, float_array, double_array, string_array,
                     complex64_array, complex128_array, list_ptr, record_ptr>;

    /// Construct default value (`bool(false)`).
    RecordValue();
    /// Construct scalar `bool`.
    explicit RecordValue(bool value);
    /// Construct scalar signed 16-bit integer.
    explicit RecordValue(std::int16_t value);
    /// Construct scalar unsigned 16-bit integer.
    explicit RecordValue(std::uint16_t value);
    /// Construct scalar signed 32-bit integer.
    explicit RecordValue(std::int32_t value);
    /// Construct scalar unsigned 32-bit integer.
    explicit RecordValue(std::uint32_t value);
    /// Construct scalar signed 64-bit integer.
    explicit RecordValue(std::int64_t value);
    /// Construct scalar unsigned 64-bit integer.
    explicit RecordValue(std::uint64_t value);
    /// Construct scalar 32-bit float.
    explicit RecordValue(float value);
    /// Construct scalar 64-bit float.
    explicit RecordValue(double value);
    /// Construct scalar string.
    explicit RecordValue(std::string value);
    /// Construct scalar string from C string.
    explicit RecordValue(const char* value);
    /// Construct scalar complex<float>.
    explicit RecordValue(std::complex<float> value);
    /// Construct scalar complex<double>.
    explicit RecordValue(std::complex<double> value);
    /// Construct signed 16-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(int16_array value);
    /// Construct unsigned 16-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(uint16_array value);
    /// Construct signed 32-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(int32_array value);
    /// Construct unsigned 32-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(uint32_array value);
    /// Construct signed 64-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(int64_array value);
    /// Construct unsigned 64-bit array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(uint64_array value);
    /// Construct float array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(float_array value);
    /// Construct double array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(double_array value);
    /// Construct string array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(string_array value);
    /// Construct complex<float> array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(complex64_array value);
    /// Construct complex<double> array value.
    /// @throws std::invalid_argument if shape-product invariant is violated.
    explicit RecordValue(complex128_array value);

    /// Construct from an owned list value moved into shared ownership.
    ///
    /// The internal pointer alternative is guaranteed non-null.
    [[nodiscard]] static RecordValue from_list(RecordList value);
    /// Construct from an owned nested record value moved into shared ownership.
    ///
    /// The internal pointer alternative is guaranteed non-null.
    [[nodiscard]] static RecordValue from_record(Record value);

    /// Access underlying variant storage.
    ///
    /// This is the canonical introspection interface for this type. Callers can
    /// use `std::holds_alternative<T>` / `std::get_if<T>` against the returned
    /// variant.
    [[nodiscard]] const storage_type& storage() const noexcept;
    /// Type- and value-aware equality across all supported alternatives.
    ///
    /// Pointer-based alternatives (`list_ptr`, `record_ptr`) are compared by
    /// pointed-to value, not pointer identity.
    [[nodiscard]] bool operator==(const RecordValue& other) const;

  private:
    explicit RecordValue(list_ptr value);
    explicit RecordValue(record_ptr value);

    storage_type storage_;
};

template <typename element_t>
bool RecordArray<element_t>::operator==(const RecordArray<element_t>& other) const {
    return shape == other.shape && elements == other.elements;
}

/// <summary>
/// Ordered list container for heterogeneous RecordValue elements.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// RecordList holds an ordered sequence of <linkto>RecordValue</linkto>
/// objects.  Unlike <linkto>Record</linkto>, elements are not keyed; they
/// are accessed by position.  RecordList is the in-memory representation
/// of casacore TableRecord arrays-of-records and AIPS++ list fields.
///
/// A RecordList is always heap-allocated and referenced through
/// <src>RecordValue::list_ptr</src> (a <src>shared_ptr<RecordList></src>)
/// when stored inside a RecordValue.  Use
/// <src>RecordValue::from_list()</src> to transfer ownership.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   RecordList lst;
///   lst.elements.push_back(RecordValue(std::int32_t{1}));
///   lst.elements.push_back(RecordValue(std::string{"two"}));
///
///   Record rec;
///   rec.set("items", RecordValue::from_list(std::move(lst)));
/// </srcblock>
/// </example>
struct RecordList {
    /// Elements in insertion order.
    std::vector<RecordValue> elements;

    [[nodiscard]] bool operator==(const RecordList& other) const;
};

/// <summary>
/// Ordered key/value record mapping string keys to RecordValue entries.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> <src>RecordValue</src> — the value type stored per key
///   <li> <src>RecordList</src>  — list alternative storable in a RecordValue
/// </prerequisite>
///
/// <synopsis>
/// Record is the in-memory representation of a casacore TableRecord.  It
/// maps string keys to <linkto>RecordValue</linkto> entries while
/// preserving insertion order exactly.
///
/// Key characteristics:
/// <ul>
///   <li> Insertion order is preserved across all mutations.
///   <li> Setting an existing key replaces its value in place without
///        changing position.
///   <li> Lookup and removal are linear in the number of entries; the
///        expected entry count in practice is small (tens of fields).
///   <li> Nested records are stored inside RecordValue via
///        <src>RecordValue::from_record()</src>, giving arbitrary depth.
/// </ul>
///
/// Record is used at several levels of the casacore-mini API:
/// table-level keywords (<src>Table::keywords()</src>), column-level
/// keywords (<src>ColumnDesc::keywords</src>), and individual cells
/// returned by <src>TableRow::get()</src>.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   Record rec;
///   rec.set("OBSERVER", RecordValue(std::string{"Alice"}));
///   rec.set("NANT",     RecordValue(std::int32_t{27}));
///
///   // Lookup
///   if (const RecordValue* v = rec.find("OBSERVER"))
///       std::cout << std::get<std::string>(v->storage()) << "\n";
///
///   // Iterate in insertion order
///   for (const auto& [key, val] : rec.entries())
///       std::cout << key << "\n";
///
///   // Remove
///   rec.remove("NANT");
/// </srcblock>
/// </example>
///
/// <motivation>
/// casacore TableRecord uses a complex class hierarchy with runtime type
/// registration.  Record provides a simpler, header-only alternative that
/// is sufficient for the keyword payloads encountered in real Measurement
/// Sets while remaining straightforward to serialize and deserialize.
/// </motivation>
class Record {
  public:
    /// Stored key/value entry type.
    using entry = std::pair<std::string, RecordValue>;

    /// Insert or replace a key/value pair.
    ///
    /// If `key` exists, value is replaced in place and ordering is unchanged.
    /// If `key` is new, it is appended at the end.
    ///
    /// Complexity: linear in entry count.
    void set(std::string_view key, RecordValue value);

    /// Find value by key.
    ///
    /// @return Pointer to stored value, or `nullptr` if key is absent.
    ///
    /// Complexity: linear in entry count.
    [[nodiscard]] const RecordValue* find(std::string_view key) const;
    /// Remove entry by key.
    ///
    /// @return `true` if an entry was removed, `false` if key was absent.
    ///
    /// Complexity: linear in entry count.
    bool remove(std::string_view key);

    /// Number of entries in insertion order.
    [[nodiscard]] std::size_t size() const noexcept;
    /// Ordered entry view.
    ///
    /// Returned reference remains valid until the record is mutated.
    [[nodiscard]] const std::vector<entry>& entries() const noexcept;

    /// Equality by exact ordered key/value sequence.
    [[nodiscard]] bool operator==(const Record& other) const;

  private:
    std::vector<entry> entries_;
};

} // namespace casacore_mini
