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

class Record;
struct RecordList;

/// Shape-aware typed array value used inside `RecordValue`.
///
/// Values are stored in flattened Fortran-order (first axis varying fastest).
template <typename element_t> struct RecordArray {
    std::vector<std::uint64_t> shape;
    std::vector<element_t> elements;

    [[nodiscard]] bool operator==(const RecordArray& other) const;
};

/// Typed value used by `Record`.
///
/// This is a small, deterministic tagged union for persistence-facing metadata.
/// It intentionally supports both `std::complex<float>` and
/// `std::complex<double>` to reflect casacore numeric value domains.
class RecordValue {
  public:
    using int16_array = RecordArray<std::int16_t>;
    using uint16_array = RecordArray<std::uint16_t>;
    using int32_array = RecordArray<std::int32_t>;
    using uint32_array = RecordArray<std::uint32_t>;
    using int64_array = RecordArray<std::int64_t>;
    using uint64_array = RecordArray<std::uint64_t>;
    using float_array = RecordArray<float>;
    using double_array = RecordArray<double>;
    using string_array = RecordArray<std::string>;
    using complex64_array = RecordArray<std::complex<float>>;
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

    /// Construct a default value (`false`).
    RecordValue();
    explicit RecordValue(bool value);
    explicit RecordValue(std::int16_t value);
    explicit RecordValue(std::uint16_t value);
    explicit RecordValue(std::int32_t value);
    explicit RecordValue(std::uint32_t value);
    explicit RecordValue(std::int64_t value);
    explicit RecordValue(std::uint64_t value);
    explicit RecordValue(float value);
    explicit RecordValue(double value);
    explicit RecordValue(std::string value);
    explicit RecordValue(const char* value);
    explicit RecordValue(std::complex<float> value);
    explicit RecordValue(std::complex<double> value);
    explicit RecordValue(int16_array value);
    explicit RecordValue(uint16_array value);
    explicit RecordValue(int32_array value);
    explicit RecordValue(uint32_array value);
    explicit RecordValue(int64_array value);
    explicit RecordValue(uint64_array value);
    explicit RecordValue(float_array value);
    explicit RecordValue(double_array value);
    explicit RecordValue(string_array value);
    explicit RecordValue(complex64_array value);
    explicit RecordValue(complex128_array value);

    /// Construct from an owned list value.
    [[nodiscard]] static RecordValue from_list(RecordList value);
    /// Construct from an owned nested record value.
    [[nodiscard]] static RecordValue from_record(Record value);

    /// Access underlying variant storage.
    [[nodiscard]] const storage_type& storage() const noexcept;
    /// Type- and value-aware equality across all supported alternatives.
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
    void set(std::string_view key, RecordValue value);

    /// Find value by key; returns `nullptr` when absent.
    [[nodiscard]] const RecordValue* find(std::string_view key) const;
    /// Number of entries in insertion order.
    [[nodiscard]] std::size_t size() const noexcept;
    /// Ordered entry view.
    [[nodiscard]] const std::vector<entry>& entries() const noexcept;

    [[nodiscard]] bool operator==(const Record& other) const;

  private:
    std::vector<entry> entries_;
};

} // namespace casacore_mini
