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

/// Typed value used by `Record`.
///
/// This is a small, deterministic tagged union for persistence-facing metadata.
/// It intentionally supports both `std::complex<float>` and
/// `std::complex<double>` to reflect casacore numeric value domains.
class RecordValue {
  public:
    /// Shared pointer type for list values.
    using list_ptr = std::shared_ptr<RecordList>;
    /// Shared pointer type for nested record values.
    using record_ptr = std::shared_ptr<Record>;
    /// Variant storage backing the value.
    using storage_type = std::variant<bool, std::int64_t, double, std::string, std::complex<float>,
                                      std::complex<double>, list_ptr, record_ptr>;

    /// Construct a default value (`false`).
    RecordValue();
    explicit RecordValue(bool value);
    explicit RecordValue(std::int64_t value);
    explicit RecordValue(double value);
    explicit RecordValue(std::string value);
    explicit RecordValue(const char* value);
    explicit RecordValue(std::complex<float> value);
    explicit RecordValue(std::complex<double> value);

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
