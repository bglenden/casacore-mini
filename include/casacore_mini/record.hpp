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

/// Shape-aware typed array value used inside `RecordValue`.
///
/// Values are stored in flattened Fortran-order (first axis varying fastest).
///
/// Invariant:
/// - `elements.size()` equals product of all dimensions in `shape`.
/// - If `shape` is empty, expected element count is `0`.
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

/// Typed value used by `Record`.
///
/// This is a small, deterministic tagged union for persistence-facing metadata.
/// It intentionally supports both `std::complex<float>` and
/// `std::complex<double>` to reflect casacore numeric value domains.
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

/// Ordered list container for `RecordValue`.
struct RecordList {
    /// Elements in insertion order.
    std::vector<RecordValue> elements;

    [[nodiscard]] bool operator==(const RecordList& other) const;
};

/// Ordered key/value record.
///
/// Insertion order is preserved. Setting an existing key replaces its value in
/// place without changing order.
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
    /// Remove an entry by key.
    ///
    /// @return True if the key was found and removed, false if absent.
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
