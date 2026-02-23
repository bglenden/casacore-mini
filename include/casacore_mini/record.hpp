#pragma once

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

class RecordValue {
  public:
    using list_ptr = std::shared_ptr<RecordList>;
    using record_ptr = std::shared_ptr<Record>;
    using storage_type = std::variant<bool, std::int64_t, double, std::string, std::complex<double>,
                                      list_ptr, record_ptr>;

    RecordValue();
    explicit RecordValue(bool value);
    explicit RecordValue(std::int64_t value);
    explicit RecordValue(double value);
    explicit RecordValue(std::string value);
    explicit RecordValue(const char* value);
    explicit RecordValue(std::complex<double> value);

    [[nodiscard]] static RecordValue from_list(RecordList value);
    [[nodiscard]] static RecordValue from_record(Record value);

    [[nodiscard]] const storage_type& storage() const noexcept;
    [[nodiscard]] bool operator==(const RecordValue& other) const;

  private:
    explicit RecordValue(list_ptr value);
    explicit RecordValue(record_ptr value);

    storage_type storage_;
};

struct RecordList {
    std::vector<RecordValue> elements;

    [[nodiscard]] bool operator==(const RecordList& other) const;
};

class Record {
  public:
    using entry = std::pair<std::string, RecordValue>;

    void set(std::string_view key, RecordValue value);

    [[nodiscard]] const RecordValue* find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<entry>& entries() const noexcept;

    [[nodiscard]] bool operator==(const Record& other) const;

  private:
    std::vector<entry> entries_;
};

} // namespace casacore_mini
