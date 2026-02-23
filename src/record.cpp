#include "casacore_mini/record.hpp"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace casacore_mini {

RecordValue::RecordValue() : storage_(false) {}

RecordValue::RecordValue(const bool value) : storage_(value) {}

RecordValue::RecordValue(const std::int64_t value) : storage_(value) {}

RecordValue::RecordValue(const double value) : storage_(value) {}

RecordValue::RecordValue(std::string value) : storage_(std::move(value)) {}

RecordValue::RecordValue(const char* value) : storage_(std::string(value)) {}

RecordValue::RecordValue(const std::complex<double> value) : storage_(value) {}

RecordValue RecordValue::from_list(RecordList value) {
    return RecordValue(std::make_shared<RecordList>(std::move(value)));
}

RecordValue RecordValue::from_record(Record value) {
    return RecordValue(std::make_shared<Record>(std::move(value)));
}

const RecordValue::storage_type& RecordValue::storage() const noexcept {
    return storage_;
}

RecordValue::RecordValue(list_ptr value) : storage_(std::move(value)) {
    if (!std::get<list_ptr>(storage_)) {
        throw std::invalid_argument("Record list pointer must not be null");
    }
}

RecordValue::RecordValue(record_ptr value) : storage_(std::move(value)) {
    if (!std::get<record_ptr>(storage_)) {
        throw std::invalid_argument("Record pointer must not be null");
    }
}

bool RecordValue::operator==(const RecordValue& other) const {
    if (storage_.index() != other.storage_.index()) {
        return false;
    }

    return std::visit(
        [&](const auto& left) {
            using value_type = std::decay_t<decltype(left)>;
            const auto* right = std::get_if<value_type>(&other.storage_);
            if (right == nullptr) {
                return false;
            }

            if constexpr (std::is_same_v<value_type, list_ptr> ||
                          std::is_same_v<value_type, record_ptr>) {
                if (!left || !*right) {
                    return left == *right;
                }
                return *left == **right;
            } else {
                return left == *right;
            }
        },
        storage_);
}

bool RecordList::operator==(const RecordList& other) const {
    return elements == other.elements;
}

void Record::set(const std::string_view key, RecordValue value) {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const entry& item) { return item.first == key; });

    if (it != entries_.end()) {
        it->second = std::move(value);
        return;
    }

    entries_.emplace_back(std::string(key), std::move(value));
}

const RecordValue* Record::find(const std::string_view key) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const entry& item) { return item.first == key; });

    if (it == entries_.end()) {
        return nullptr;
    }

    return &it->second;
}

std::size_t Record::size() const noexcept {
    return entries_.size();
}

const std::vector<Record::entry>& Record::entries() const noexcept {
    return entries_;
}

bool Record::operator==(const Record& other) const {
    return entries_ == other.entries_;
}

} // namespace casacore_mini
