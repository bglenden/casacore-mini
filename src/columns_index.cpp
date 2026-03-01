#include "casacore_mini/columns_index.hpp"

#include <stdexcept>

namespace casacore_mini {

namespace {

/// Compare two CellValues for ordering. Returns -1, 0, or 1.
int cell_compare(const CellValue& a, const CellValue& b) {
    if (a.index() != b.index()) {
        return a.index() < b.index() ? -1 : 1;
    }
    return std::visit(
        [&b](const auto& va) -> int {
            using T = std::decay_t<decltype(va)>;
            const auto& vb = std::get<T>(b);
            if constexpr (std::is_same_v<T, std::complex<float>> ||
                          std::is_same_v<T, std::complex<double>>) {
                if (va.real() < vb.real())
                    return -1;
                if (va.real() > vb.real())
                    return 1;
                if (va.imag() < vb.imag())
                    return -1;
                if (va.imag() > vb.imag())
                    return 1;
                return 0;
            } else {
                if (va < vb)
                    return -1;
                if (vb < va)
                    return 1;
                return 0;
            }
        },
        a);
}

} // namespace

bool ColumnsIndex::KeyCompare::operator()(const IndexKey& a, const IndexKey& b) const {
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        int cmp = cell_compare(a[i], b[i]);
        if (cmp < 0)
            return true;
        if (cmp > 0)
            return false;
    }
    return a.size() < b.size();
}

ColumnsIndex::ColumnsIndex(Table& table, std::vector<std::string> key_columns)
    : table_(&table), key_columns_(std::move(key_columns)) {
    if (key_columns_.empty()) {
        throw std::runtime_error("ColumnsIndex: must have at least one key column");
    }
    build();
}

void ColumnsIndex::build() {
    index_.clear();
    for (std::uint64_t r = 0; r < table_->nrow(); ++r) {
        auto key = make_key_from_row(r);
        index_[std::move(key)].push_back(r);
    }
}

ColumnsIndex::IndexKey ColumnsIndex::make_key(const Record& rec) const {
    IndexKey key;
    key.reserve(key_columns_.size());
    for (const auto& col : key_columns_) {
        const auto* val = rec.find(col);
        if (!val) {
            throw std::runtime_error("ColumnsIndex: key column '" + col + "' not found in Record");
        }
        // Convert RecordValue storage to CellValue.
        std::visit(
            [&key](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, std::int32_t> ||
                              std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::int64_t> ||
                              std::is_same_v<T, float> || std::is_same_v<T, double> ||
                              std::is_same_v<T, std::string> ||
                              std::is_same_v<T, std::complex<float>> ||
                              std::is_same_v<T, std::complex<double>>) {
                    key.emplace_back(v);
                } else {
                    throw std::runtime_error("ColumnsIndex: unsupported RecordValue type for key");
                }
            },
            val->storage());
    }
    return key;
}

ColumnsIndex::IndexKey ColumnsIndex::make_key_from_row(std::uint64_t row) const {
    IndexKey key;
    key.reserve(key_columns_.size());
    for (const auto& col : key_columns_) {
        key.push_back(table_->read_scalar_cell(col, row));
    }
    return key;
}

std::vector<std::uint64_t> ColumnsIndex::get_row_numbers(const Record& key) const {
    auto k = make_key(key);
    auto it = index_.find(k);
    if (it == index_.end())
        return {};
    return it->second;
}

std::uint64_t ColumnsIndex::get_row_number(const Record& key) const {
    auto rows = get_row_numbers(key);
    if (rows.empty()) {
        throw std::runtime_error("ColumnsIndex: key not found");
    }
    if (rows.size() > 1) {
        throw std::runtime_error("ColumnsIndex: key is not unique (" + std::to_string(rows.size()) +
                                 " matches)");
    }
    return rows[0];
}

bool ColumnsIndex::contains(const Record& key) const {
    auto k = make_key(key);
    return index_.count(k) > 0;
}

void ColumnsIndex::rebuild() {
    build();
}

std::size_t ColumnsIndex::unique_key_count() const {
    return index_.size();
}

} // namespace casacore_mini
