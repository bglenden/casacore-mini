#include "casacore_mini/lattice.hpp"

#include <complex>
#include <filesystem>
#include <stdexcept>

namespace casacore_mini {

// ── PagedArray<float> specialization ──────────────────────────────────
// PagedArray is backed by a casacore table with a single "map" column.
// For now we provide explicit instantiations for float and double.

// Helper: read a flat cell from the table and wrap in LatticeArray.
template <>
LatticeArray<float> PagedArray<float>::get() const {
    auto vals = table_->read_array_float_cell("map", 0);
    return LatticeArray<float>(IPosition(shape_), std::move(vals));
}

template <>
LatticeArray<float> PagedArray<float>::get_slice(const Slicer& slicer) const {
    // Read full then extract slice (simple but correct approach for now).
    auto full = get();
    return full.get_slice(slicer);
}

template <>
float PagedArray<float>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedArray<float>::put(const LatticeArray<float>& data) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    table_->write_array_float_cell("map", 0, data.flat());
}

template <>
void PagedArray<float>::put_slice(const LatticeArray<float>& data,
                                  const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    table_->write_array_float_cell("map", 0, full.flat());
}

template <>
void PagedArray<float>::put_at(const float& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    table_->write_array_float_cell("map", 0, full.flat());
}

// ── PagedArray<double> specialization ─────────────────────────────────

template <>
LatticeArray<double> PagedArray<double>::get() const {
    auto vals = table_->read_array_double_cell("map", 0);
    return LatticeArray<double>(IPosition(shape_), std::move(vals));
}

template <>
LatticeArray<double> PagedArray<double>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <>
double PagedArray<double>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedArray<double>::put(const LatticeArray<double>& data) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    table_->write_array_double_cell("map", 0, data.flat());
}

template <>
void PagedArray<double>::put_slice(const LatticeArray<double>& data,
                                   const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    table_->write_array_double_cell("map", 0, full.flat());
}

template <>
void PagedArray<double>::put_at(const double& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    table_->write_array_double_cell("map", 0, full.flat());
}

// ── PagedArray<complex<float>> specialization ─────────────────────────

template <>
LatticeArray<std::complex<float>> PagedArray<std::complex<float>>::get() const {
    auto vals = table_->read_array_complex_cell("map", 0);
    return LatticeArray<std::complex<float>>(IPosition(shape_), std::move(vals));
}

template <>
LatticeArray<std::complex<float>>
PagedArray<std::complex<float>>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <>
std::complex<float>
PagedArray<std::complex<float>>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedArray<std::complex<float>>::put(
    const LatticeArray<std::complex<float>>& data) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    // Table API uses std::vector<std::complex<float>> with shape.
    std::vector<std::int32_t> sh;
    for (std::size_t i = 0; i < data.shape().ndim(); ++i) {
        sh.push_back(static_cast<std::int32_t>(data.shape()[i]));
    }
    table_->write_array_complex_cell("map", 0, data.flat(), sh);
}

template <>
void PagedArray<std::complex<float>>::put_slice(
    const LatticeArray<std::complex<float>>& data, const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    put(full);
}

template <>
void PagedArray<std::complex<float>>::put_at(
    const std::complex<float>& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("PagedArray is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    put(full);
}

// ── PagedArray constructor/common methods (all types) ─────────────────

static DataType lattice_data_type_for_float() { return DataType::tp_float; }
static DataType lattice_data_type_for_double() { return DataType::tp_double; }
static DataType lattice_data_type_for_complex() { return DataType::tp_complex; }

template <typename T>
static DataType lattice_data_type();

template <>
DataType lattice_data_type<float>() { return lattice_data_type_for_float(); }
template <>
DataType lattice_data_type<double>() { return lattice_data_type_for_double(); }
template <>
DataType lattice_data_type<std::complex<float>>() { return lattice_data_type_for_complex(); }

template <typename T>
PagedArray<T>::PagedArray(IPosition shape, const std::filesystem::path& path)
    : shape_(std::move(shape)), path_(path), writable_(true) {
    // Create a table with one array column "map".
    TableColumnSpec col;
    col.name = "map";
    col.data_type = lattice_data_type<T>();
    col.kind = ColumnKind::array;
    for (std::size_t i = 0; i < shape_.ndim(); ++i) {
        col.shape.push_back(shape_[i]);
    }

    TableCreateOptions opts;
    opts.sm_type = "TiledShapeStMan";
    opts.sm_group = "TiledShapeStMan";
    // Compute tile shape: up to 32 along each axis.
    for (std::size_t i = 0; i < shape_.ndim(); ++i) {
        opts.tile_shape.push_back(std::min(shape_[i], std::int64_t{32}));
    }
    // Add a row dimension for tiling.
    opts.tile_shape.push_back(1);

    table_ = std::make_shared<Table>(Table::create(path_, {col}, 1, opts));

    // Initialize with zeros.
    auto zeros = LatticeArray<T>(IPosition(shape_));
    put(zeros);
}

template <typename T>
PagedArray<T>::PagedArray(const std::filesystem::path& path, bool writable)
    : path_(path), writable_(writable) {
    table_ = writable_
                 ? std::make_shared<Table>(Table::open_rw(path_))
                 : std::make_shared<Table>(Table::open(path_));
    // Read shape from column descriptor.
    auto* cd = table_->find_column_desc("map");
    if (!cd) throw std::runtime_error("PagedArray: no 'map' column in table");
    shape_ = IPosition(cd->shape);
}

template <typename T>
LatticeArray<bool> PagedArray<T>::get_mask() const {
    return LatticeArray<bool>(shape_, true);
}

template <typename T>
LatticeArray<bool> PagedArray<T>::get_mask_slice(const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
void PagedArray<T>::flush() {
    if (table_ && writable_) table_->flush();
}

template <typename T>
std::string PagedArray<T>::table_name() const {
    return table_ ? table_->table_name() : "";
}

template <typename T>
const std::filesystem::path& PagedArray<T>::table_path() const {
    return path_;
}

// Explicit instantiations.
template class PagedArray<float>;
template class PagedArray<double>;
template class PagedArray<std::complex<float>>;

} // namespace casacore_mini
