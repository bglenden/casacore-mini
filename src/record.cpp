// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/record.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace casacore_mini {
namespace {

template <typename element_t> void validate_array_shape(const RecordArray<element_t>& value) {
    std::size_t expected = 0;
    if (!value.shape.empty()) {
        expected = 1;
        for (const auto dim : value.shape) {
            if (dim == 0) {
                expected = 0;
                break;
            }
            if (expected >
                std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(dim)) {
                throw std::invalid_argument("Record array shape product overflow");
            }
            expected *= static_cast<std::size_t>(dim);
        }
    }

    if (expected != value.elements.size()) {
        throw std::invalid_argument("Record array shape does not match element count");
    }
}

} // namespace

RecordValue::RecordValue() : storage_(false) {}

RecordValue::RecordValue(const bool value) : storage_(value) {}

RecordValue::RecordValue(const std::int16_t value) : storage_(value) {}

RecordValue::RecordValue(const std::uint16_t value) : storage_(value) {}

RecordValue::RecordValue(const std::int32_t value) : storage_(value) {}

RecordValue::RecordValue(const std::uint32_t value) : storage_(value) {}

RecordValue::RecordValue(const std::int64_t value) : storage_(value) {}

RecordValue::RecordValue(const std::uint64_t value) : storage_(value) {}

RecordValue::RecordValue(const float value) : storage_(value) {}

RecordValue::RecordValue(const double value) : storage_(value) {}

RecordValue::RecordValue(std::string value) : storage_(std::move(value)) {}

RecordValue::RecordValue(const char* value) : storage_(std::string(value)) {}

RecordValue::RecordValue(const std::complex<float> value) : storage_(value) {}

RecordValue::RecordValue(const std::complex<double> value) : storage_(value) {}

RecordValue::RecordValue(int16_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<int16_array>(storage_));
}

RecordValue::RecordValue(uint16_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<uint16_array>(storage_));
}

RecordValue::RecordValue(int32_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<int32_array>(storage_));
}

RecordValue::RecordValue(uint32_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<uint32_array>(storage_));
}

RecordValue::RecordValue(int64_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<int64_array>(storage_));
}

RecordValue::RecordValue(uint64_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<uint64_array>(storage_));
}

RecordValue::RecordValue(float_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<float_array>(storage_));
}

RecordValue::RecordValue(double_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<double_array>(storage_));
}

RecordValue::RecordValue(string_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<string_array>(storage_));
}

RecordValue::RecordValue(complex64_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<complex64_array>(storage_));
}

RecordValue::RecordValue(complex128_array value) : storage_(std::move(value)) {
    validate_array_shape(std::get<complex128_array>(storage_));
}

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

bool Record::remove(const std::string_view key) {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const entry& item) { return item.first == key; });
    if (it == entries_.end()) {
        return false;
    }
    entries_.erase(it);
    return true;
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
