#pragma once

#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"
#include "casacore_mini/table.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Lattice class hierarchy: abstract base, in-memory, paged, sub, temp.
///
/// All lattice data is stored in Fortran (column-major) order and accessed
/// through `LatticeArray<T>` which provides `std::mdspan` views.

// ── Forward declarations ───────────────────────────────────────────────

template <typename T> class Lattice;
template <typename T> class MaskedLattice;
template <typename T> class ArrayLattice;
template <typename T> class PagedArray;
template <typename T> class SubLattice;
template <typename T> class TempLattice;
template <typename T> class LatticeConcat;
template <typename T> class RebinLattice;
template <typename T> class LatticeIterator;

// ── Lattice<T> ─────────────────────────────────────────────────────────

/// Abstract base class for all lattice types.
///
/// Defines the minimal interface for multidimensional data access.
/// Data is always in Fortran order (first axis varies fastest).
template <typename T>
class Lattice {
  public:
    virtual ~Lattice() = default;

    /// Shape of the lattice.
    [[nodiscard]] virtual const IPosition& shape() const = 0;
    /// Number of dimensions.
    [[nodiscard]] std::size_t ndim() const { return shape().ndim(); }
    /// Total number of elements.
    [[nodiscard]] std::int64_t nelements() const { return shape().product(); }

    /// Whether the lattice supports writing.
    [[nodiscard]] virtual bool is_writable() const = 0;
    /// Whether the lattice is backed by persistent storage.
    [[nodiscard]] virtual bool is_paged() const { return false; }

    /// Read the entire lattice into a LatticeArray.
    [[nodiscard]] virtual LatticeArray<T> get() const = 0;
    /// Read a slice of the lattice.
    [[nodiscard]] virtual LatticeArray<T> get_slice(const Slicer& slicer) const = 0;
    /// Read a single element.
    [[nodiscard]] virtual T get_at(const IPosition& where) const = 0;

    /// Write the entire lattice from a LatticeArray.
    virtual void put(const LatticeArray<T>& data) = 0;
    /// Write a slice into the lattice.
    virtual void put_slice(const LatticeArray<T>& data, const Slicer& slicer) = 0;
    /// Write a single element.
    virtual void put_at(const T& value, const IPosition& where) = 0;

    /// Set all elements to the given value.
    virtual void set(const T& value);

    /// Apply a function element-wise (in-place).
    virtual void apply(const std::function<T(T)>& func);

  protected:
    Lattice() = default;
    Lattice(const Lattice&) = default;
    Lattice& operator=(const Lattice&) = default;
    Lattice(Lattice&&) = default;
    Lattice& operator=(Lattice&&) = default;
};

// ── MaskedLattice<T> ──────────────────────────────────────────────────

/// Abstract lattice with an associated boolean mask.
///
/// Mask convention: `true` means the pixel is valid (not masked out).
template <typename T>
class MaskedLattice : public Lattice<T> {
  public:
    /// Whether the lattice has a pixel mask.
    [[nodiscard]] virtual bool is_masked() const { return has_pixel_mask(); }
    /// Whether a pixel mask is attached.
    [[nodiscard]] virtual bool has_pixel_mask() const = 0;
    /// Get the full pixel mask (true = valid).
    [[nodiscard]] virtual LatticeArray<bool> get_mask() const = 0;
    /// Get a slice of the pixel mask.
    [[nodiscard]] virtual LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const = 0;
};

// ── ArrayLattice<T> ───────────────────────────────────────────────────

/// In-memory lattice backed by a `LatticeArray<T>`.
///
/// This is the simplest concrete Lattice: all data lives in memory.
template <typename T>
class ArrayLattice : public MaskedLattice<T> {
  public:
    /// Construct from shape (value-initialized).
    explicit ArrayLattice(IPosition shape);
    /// Construct from shape and initial value.
    ArrayLattice(IPosition shape, T value);
    /// Construct from existing LatticeArray.
    explicit ArrayLattice(LatticeArray<T> data);

    [[nodiscard]] const IPosition& shape() const override { return data_.shape(); }
    [[nodiscard]] bool is_writable() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override { return data_; }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Direct access to the underlying LatticeArray.
    [[nodiscard]] const LatticeArray<T>& array() const { return data_; }
    [[nodiscard]] LatticeArray<T>& array() { return data_; }

  private:
    LatticeArray<T> data_;
};

// ── PagedArray<T> ─────────────────────────────────────────────────────

/// Disk-backed lattice stored as a single-column table with TiledShapeStMan.
///
/// Compatible with casacore's `PagedArray` table format: a table with
/// one array column ("map") backed by a tiled storage manager.
template <typename T>
class PagedArray : public MaskedLattice<T> {
  public:
    /// Create a new paged array on disk.
    PagedArray(IPosition shape, const std::filesystem::path& path);
    /// Open an existing paged array from disk (read-only by default).
    explicit PagedArray(const std::filesystem::path& path,
                        bool writable = false);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return writable_; }
    [[nodiscard]] bool is_paged() const override { return true; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Flush pending writes to disk.
    void flush();
    /// Table name (directory basename).
    [[nodiscard]] std::string table_name() const;
    /// Path to the table directory.
    [[nodiscard]] const std::filesystem::path& table_path() const;

  private:
    IPosition shape_;
    std::filesystem::path path_;
    bool writable_ = false;
    mutable std::shared_ptr<Table> table_;
};

// ── SubLattice<T> ─────────────────────────────────────────────────────

/// A view into a sub-region of another lattice.
///
/// Does not own data. Reads/writes are forwarded to the parent lattice.
template <typename T>
class SubLattice : public MaskedLattice<T> {
  public:
    /// Construct from a lattice and a slicer defining the sub-region.
    SubLattice(Lattice<T>& parent, Slicer slicer);
    /// Construct a read-only sub-lattice.
    SubLattice(const Lattice<T>& parent, Slicer slicer);

    [[nodiscard]] const IPosition& shape() const override { return sub_shape_; }
    [[nodiscard]] bool is_writable() const override { return writable_; }
    [[nodiscard]] bool is_paged() const override { return parent_->is_paged(); }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

  private:
    /// Map a local index into the parent coordinate space.
    [[nodiscard]] IPosition to_parent_index(const IPosition& local) const;
    /// Map a local slicer into the parent coordinate space.
    [[nodiscard]] Slicer to_parent_slicer(const Slicer& local) const;

    Lattice<T>* parent_rw_ = nullptr;
    const Lattice<T>* parent_ = nullptr;
    Slicer region_;
    IPosition sub_shape_;
    bool writable_ = false;
};

// ── TempLattice<T> ───────────────────────────────────────────────────

/// Temporary lattice that lives in memory (small) or on disk (large).
///
/// If the total size exceeds `max_memory_bytes`, data is stored in a
/// temporary table on disk. Otherwise, it behaves like an ArrayLattice.
template <typename T>
class TempLattice : public MaskedLattice<T> {
  public:
    /// Construct with given shape. If size > max_memory_bytes, use disk.
    TempLattice(IPosition shape, std::size_t max_memory_bytes = std::size_t{16} * 1024 * 1024);

    [[nodiscard]] const IPosition& shape() const override { return impl_->shape(); }
    [[nodiscard]] bool is_writable() const override { return true; }
    [[nodiscard]] bool is_paged() const override { return impl_->is_paged(); }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override { return impl_->get(); }
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override {
        return impl_->get_slice(slicer);
    }
    [[nodiscard]] T get_at(const IPosition& where) const override {
        return impl_->get_at(where);
    }

    void put(const LatticeArray<T>& data) override { impl_->put(data); }
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override {
        impl_->put_slice(data, slicer);
    }
    void put_at(const T& value, const IPosition& where) override {
        impl_->put_at(value, where);
    }

  private:
    std::unique_ptr<Lattice<T>> impl_;
};

// ── LatticeConcat<T> ──────────────────────────────────────────────────

/// Read-only concatenation of multiple lattices along one axis.
///
/// The concatenation axis must have the same shape for all non-concat
/// dimensions. The result shape on the concat axis is the sum of the
/// input shapes on that axis.
template <typename T>
class LatticeConcat : public MaskedLattice<T> {
  public:
    /// Construct from a list of lattices and the axis along which to
    /// concatenate.
    LatticeConcat(std::vector<const Lattice<T>*> lattices, std::size_t axis);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Number of component lattices.
    [[nodiscard]] std::size_t nlattices() const { return lattices_.size(); }
    /// Concatenation axis index.
    [[nodiscard]] std::size_t axis() const { return axis_; }

  private:
    /// Find which component lattice contains `global_pos` on the concat axis.
    /// Returns (lattice_index, local_pos_on_axis).
    [[nodiscard]] std::pair<std::size_t, std::int64_t>
    locate(std::int64_t global_pos) const;

    std::vector<const Lattice<T>*> lattices_;
    std::size_t axis_;
    IPosition shape_;
    /// Cumulative offsets on the concat axis: offsets_[i] is the start index
    /// of lattice i in the concatenated shape.
    std::vector<std::int64_t> offsets_;
};

// ── RebinLattice<T> ──────────────────────────────────────────────────

/// Read-only view that rebins (downsamples) a lattice by integer factors.
///
/// Each output pixel is the mean of a bin of input pixels. The output
/// shape is `ceil(input_shape[d] / factor[d])` for each dimension.
template <typename T>
class RebinLattice : public MaskedLattice<T> {
  public:
    /// Construct from a lattice and per-axis rebin factors.
    RebinLattice(const Lattice<T>& parent, IPosition factors);

    [[nodiscard]] const IPosition& shape() const override { return shape_; }
    [[nodiscard]] bool is_writable() const override { return false; }
    [[nodiscard]] bool has_pixel_mask() const override { return false; }
    [[nodiscard]] LatticeArray<bool> get_mask() const override;
    [[nodiscard]] LatticeArray<bool> get_mask_slice(
        const Slicer& slicer) const override;

    [[nodiscard]] LatticeArray<T> get() const override;
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const override;
    [[nodiscard]] T get_at(const IPosition& where) const override;

    void put(const LatticeArray<T>& data) override;
    void put_slice(const LatticeArray<T>& data, const Slicer& slicer) override;
    void put_at(const T& value, const IPosition& where) override;

    /// Rebin factors per axis.
    [[nodiscard]] const IPosition& factors() const { return factors_; }

  private:
    const Lattice<T>* parent_;
    IPosition factors_;
    IPosition shape_;
};

// ── LatticeIterator<T> ────────────────────────────────────────────────

/// Cursor-based iterator that walks through a lattice in tile-sized chunks.
///
/// Iterates in Fortran order (first axis varies fastest). At each step,
/// the cursor provides a `LatticeArray<T>` containing the current chunk.
template <typename T>
class LatticeIterator {
  public:
    /// Construct an iterator over the entire lattice with given cursor shape.
    LatticeIterator(Lattice<T>& lattice, IPosition cursor_shape);
    /// Construct a read-only iterator.
    LatticeIterator(const Lattice<T>& lattice, IPosition cursor_shape);

    /// Reset to the beginning.
    void reset();
    /// Advance to the next position.
    LatticeIterator& operator++();
    /// True if all positions have been visited.
    [[nodiscard]] bool at_end() const;

    /// Current cursor position (start index of the current chunk).
    [[nodiscard]] const IPosition& position() const { return position_; }
    /// Shape of the current cursor chunk (may be smaller at edges).
    [[nodiscard]] IPosition cursor_shape() const;
    /// Read-only access to the current chunk.
    [[nodiscard]] LatticeArray<T> cursor() const;
    /// Write updated data back to the current position (for writable lattices).
    void write_cursor(const LatticeArray<T>& data);

  private:
    Lattice<T>* lattice_rw_ = nullptr;
    const Lattice<T>* lattice_ = nullptr;
    IPosition requested_shape_;
    IPosition position_;
    bool writable_ = false;
};

// ── Template method implementations ───────────────────────────────────

template <typename T>
void Lattice<T>::set(const T& value) {
    auto data = LatticeArray<T>(shape(), value);
    put(data);
}

template <typename T>
void Lattice<T>::apply(const std::function<T(T)>& func) {
    auto data = get();
    data.make_unique();
    for (std::size_t i = 0; i < data.nelements(); ++i) {
        data.mutable_data()[i] = func(data[i]);
    }
    put(data);
}

// ── ArrayLattice<T> implementations ───────────────────────────────────

template <typename T>
ArrayLattice<T>::ArrayLattice(IPosition shape) : data_(std::move(shape)) {}

template <typename T>
ArrayLattice<T>::ArrayLattice(IPosition shape, T value)
    : data_(std::move(shape), value) {}

template <typename T>
ArrayLattice<T>::ArrayLattice(LatticeArray<T> data) : data_(std::move(data)) {}

template <typename T>
LatticeArray<bool> ArrayLattice<T>::get_mask() const {
    return LatticeArray<bool>(data_.shape(), true);
}

template <typename T>
LatticeArray<bool> ArrayLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> ArrayLattice<T>::get_slice(const Slicer& slicer) const {
    return data_.get_slice(slicer);
}

template <typename T>
T ArrayLattice<T>::get_at(const IPosition& where) const {
    return data_.at(where);
}

template <typename T>
void ArrayLattice<T>::put(const LatticeArray<T>& data) {
    data_ = data;
    data_.make_unique();
}

template <typename T>
void ArrayLattice<T>::put_slice(const LatticeArray<T>& data,
                                const Slicer& slicer) {
    data_.make_unique();
    data_.put_slice(data, slicer);
}

template <typename T>
void ArrayLattice<T>::put_at(const T& value, const IPosition& where) {
    data_.make_unique();
    data_.put(where, value);
}

// ── SubLattice<T> implementations ─────────────────────────────────────

template <typename T>
SubLattice<T>::SubLattice(Lattice<T>& parent, Slicer slicer)
    : parent_rw_(&parent),
      parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      writable_(parent.is_writable()) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
SubLattice<T>::SubLattice(const Lattice<T>& parent, Slicer slicer)
    : parent_(&parent),
      region_(std::move(slicer)),
      sub_shape_(region_.length()),
      writable_(false) {
    validate_slicer(region_, parent.shape());
}

template <typename T>
LatticeArray<bool> SubLattice<T>::get_mask() const {
    return LatticeArray<bool>(sub_shape_, true);
}

template <typename T>
LatticeArray<bool> SubLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> SubLattice<T>::get() const {
    return parent_->get_slice(region_);
}

template <typename T>
LatticeArray<T> SubLattice<T>::get_slice(const Slicer& slicer) const {
    return parent_->get_slice(to_parent_slicer(slicer));
}

template <typename T>
T SubLattice<T>::get_at(const IPosition& where) const {
    return parent_->get_at(to_parent_index(where));
}

template <typename T>
void SubLattice<T>::put(const LatticeArray<T>& data) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_slice(data, region_);
}

template <typename T>
void SubLattice<T>::put_slice(const LatticeArray<T>& data,
                              const Slicer& slicer) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_slice(data, to_parent_slicer(slicer));
}

template <typename T>
void SubLattice<T>::put_at(const T& value, const IPosition& where) {
    if (!writable_) throw std::runtime_error("SubLattice is read-only");
    parent_rw_->put_at(value, to_parent_index(where));
}

template <typename T>
IPosition SubLattice<T>::to_parent_index(const IPosition& local) const {
    IPosition result(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        result[d] = region_.start()[d] + local[d] * region_.stride()[d];
    }
    return result;
}

template <typename T>
Slicer SubLattice<T>::to_parent_slicer(const Slicer& local) const {
    IPosition start(local.ndim());
    IPosition stride(local.ndim());
    for (std::size_t d = 0; d < local.ndim(); ++d) {
        start[d] = region_.start()[d] + local.start()[d] * region_.stride()[d];
        stride[d] = local.stride()[d] * region_.stride()[d];
    }
    return Slicer(std::move(start), IPosition(local.length()), std::move(stride));
}

// ── TempLattice<T> implementations ───────────────────────────────────

template <typename T>
TempLattice<T>::TempLattice(IPosition shape, std::size_t max_memory_bytes) {
    auto total_bytes = static_cast<std::size_t>(shape.product()) * sizeof(T);
    if (total_bytes <= max_memory_bytes) {
        impl_ = std::make_unique<ArrayLattice<T>>(std::move(shape));
    } else {
        // For large data, use a temporary file on disk.
        auto tmp_dir = std::filesystem::temp_directory_path() / "casacore_mini_tmp";
        std::filesystem::create_directories(tmp_dir);
        // Generate a unique name.
        auto name = "tmp_lattice_" + std::to_string(
            std::hash<std::string>{}(shape.to_string() +
                                     std::to_string(reinterpret_cast<std::uintptr_t>(this))));
        auto path = tmp_dir / name;
        impl_ = std::make_unique<PagedArray<T>>(std::move(shape), path);
    }
}

template <typename T>
LatticeArray<bool> TempLattice<T>::get_mask() const {
    return LatticeArray<bool>(impl_->shape(), true);
}

template <typename T>
LatticeArray<bool> TempLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

// ── LatticeIterator<T> implementations ────────────────────────────────

template <typename T>
LatticeIterator<T>::LatticeIterator(
    Lattice<T>& lattice,
    IPosition cursor_shape) // NOLINT(performance-unnecessary-value-param)
    : lattice_rw_(&lattice),
      lattice_(&lattice),
      requested_shape_(std::move(cursor_shape)),
      position_(lattice.shape().ndim(), 0),
      writable_(true) {}

template <typename T>
LatticeIterator<T>::LatticeIterator(
    const Lattice<T>& lattice,
    IPosition cursor_shape) // NOLINT(performance-unnecessary-value-param)
    : lattice_(&lattice),
      requested_shape_(std::move(cursor_shape)),
      position_(lattice.shape().ndim(), 0),
      writable_(false) {}

template <typename T>
void LatticeIterator<T>::reset() {
    for (std::size_t i = 0; i < position_.ndim(); ++i) position_[i] = 0;
}

template <typename T>
LatticeIterator<T>& LatticeIterator<T>::operator++() {
    const auto& shape = lattice_->shape();
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        position_[d] += requested_shape_[d];
        if (position_[d] < shape[d]) return *this;
        position_[d] = 0;
    }
    // All dimensions wrapped → set to end sentinel.
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        position_[d] = shape[d];
    }
    return *this;
}

template <typename T>
bool LatticeIterator<T>::at_end() const {
    const auto& shape = lattice_->shape();
    for (std::size_t d = 0; d < position_.ndim(); ++d) {
        if (position_[d] >= shape[d]) return true;
    }
    return false;
}

template <typename T>
IPosition LatticeIterator<T>::cursor_shape() const {
    const auto& shape = lattice_->shape();
    IPosition cs(shape.ndim());
    for (std::size_t d = 0; d < shape.ndim(); ++d) {
        auto remaining = shape[d] - position_[d];
        cs[d] = std::min(requested_shape_[d], remaining);
    }
    return cs;
}

template <typename T>
LatticeArray<T> LatticeIterator<T>::cursor() const {
    auto cs = cursor_shape();
    return lattice_->get_slice(Slicer(IPosition(position_), cs));
}

template <typename T>
void LatticeIterator<T>::write_cursor(const LatticeArray<T>& data) {
    if (!writable_) throw std::runtime_error("LatticeIterator is read-only");
    auto cs = cursor_shape();
    lattice_rw_->put_slice(data, Slicer(IPosition(position_), cs));
}

// ── LatticeConcat<T> implementations ──────────────────────────────────

template <typename T>
LatticeConcat<T>::LatticeConcat(std::vector<const Lattice<T>*> lattices,
                                std::size_t axis)
    : lattices_(std::move(lattices)), axis_(axis) {
    if (lattices_.empty()) {
        throw std::runtime_error("LatticeConcat: no lattices given");
    }
    const auto ndim = lattices_[0]->shape().ndim();
    if (axis_ >= ndim) {
        throw std::runtime_error("LatticeConcat: axis out of range");
    }
    // Compute shape: same as first lattice except on the concat axis.
    shape_ = IPosition(lattices_[0]->shape());
    offsets_.reserve(lattices_.size());
    offsets_.push_back(0);
    for (std::size_t i = 1; i < lattices_.size(); ++i) {
        const auto& s = lattices_[i]->shape();
        if (s.ndim() != ndim) {
            throw std::runtime_error("LatticeConcat: mismatched ndim");
        }
        for (std::size_t d = 0; d < ndim; ++d) {
            if (d != axis_ && s[d] != shape_[d]) {
                throw std::runtime_error(
                    "LatticeConcat: mismatched shape on non-concat axis");
            }
        }
        offsets_.push_back(shape_[axis_]);
        shape_[axis_] += s[axis_];
    }
}

template <typename T>
LatticeArray<bool> LatticeConcat<T>::get_mask() const {
    return LatticeArray<bool>(shape_, true);
}

template <typename T>
LatticeArray<bool> LatticeConcat<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
std::pair<std::size_t, std::int64_t>
LatticeConcat<T>::locate(std::int64_t global_pos) const {
    for (std::size_t i = 0; i < lattices_.size(); ++i) {
        auto extent = lattices_[i]->shape()[axis_];
        if (global_pos < offsets_[i] + extent) {
            return {i, global_pos - offsets_[i]};
        }
    }
    throw std::runtime_error("LatticeConcat: position out of range");
}

template <typename T>
LatticeArray<T> LatticeConcat<T>::get() const {
    LatticeArray<T> result(shape_);
    // Copy each component lattice into the output.
    for (std::size_t i = 0; i < lattices_.size(); ++i) {
        auto src = lattices_[i]->get();
        IPosition start(shape_.ndim(), 0);
        start[axis_] = offsets_[i];
        result.put_slice(src, Slicer(std::move(start), lattices_[i]->shape()));
    }
    return result;
}

template <typename T>
LatticeArray<T> LatticeConcat<T>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <typename T>
T LatticeConcat<T>::get_at(const IPosition& where) const {
    auto [idx, local_pos] = locate(where[axis_]);
    IPosition local(where);
    local[axis_] = local_pos;
    return lattices_[idx]->get_at(local);
}

template <typename T>
void LatticeConcat<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

template <typename T>
void LatticeConcat<T>::put_slice(const LatticeArray<T>& /*data*/,
                                  const Slicer& /*slicer*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

template <typename T>
void LatticeConcat<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("LatticeConcat is read-only");
}

// ── RebinLattice<T> implementations ──────────────────────────────────

template <typename T>
RebinLattice<T>::RebinLattice(const Lattice<T>& parent, IPosition factors)
    : parent_(&parent), factors_(std::move(factors)) {
    const auto& ps = parent.shape();
    if (factors_.ndim() != ps.ndim()) {
        throw std::runtime_error("RebinLattice: factors ndim mismatch");
    }
    shape_ = IPosition(ps.ndim());
    for (std::size_t d = 0; d < ps.ndim(); ++d) {
        if (factors_[d] <= 0) {
            throw std::runtime_error("RebinLattice: factors must be positive");
        }
        shape_[d] = (ps[d] + factors_[d] - 1) / factors_[d];
    }
}

template <typename T>
LatticeArray<bool> RebinLattice<T>::get_mask() const {
    return LatticeArray<bool>(shape_, true);
}

template <typename T>
LatticeArray<bool> RebinLattice<T>::get_mask_slice(
    const Slicer& slicer) const {
    return LatticeArray<bool>(IPosition(slicer.length()), true);
}

template <typename T>
LatticeArray<T> RebinLattice<T>::get() const {
    auto src = parent_->get();
    LatticeArray<T> result(shape_);
    const auto ndim = shape_.ndim();
    const auto& ps = parent_->shape();

    // Iterate over every output element.
    auto total = shape_.product();
    for (std::int64_t flat = 0; flat < total; ++flat) {
        // Decompose flat index into output coordinates.
        IPosition out_pos(ndim);
        std::int64_t rem = flat;
        for (std::size_t d = 0; d < ndim; ++d) {
            out_pos[d] = rem % shape_[d];
            rem /= shape_[d];
        }

        // Average the bin in the source.
        double sum = 0.0;
        std::int64_t count = 0;

        // Compute bin bounds.
        std::vector<std::int64_t> lo(ndim);
        std::vector<std::int64_t> hi(ndim);
        for (std::size_t d = 0; d < ndim; ++d) {
            lo[d] = out_pos[d] * factors_[d];
            hi[d] = std::min(lo[d] + factors_[d], ps[d]);
        }

        // Iterate through the bin (works for any dimensionality).
        std::int64_t bin_total = 1;
        for (std::size_t d = 0; d < ndim; ++d) {
            bin_total *= (hi[d] - lo[d]);
        }
        for (std::int64_t b = 0; b < bin_total; ++b) {
            IPosition src_pos(ndim);
            std::int64_t brem = b;
            for (std::size_t d = 0; d < ndim; ++d) {
                auto extent = hi[d] - lo[d];
                src_pos[d] = lo[d] + brem % extent;
                brem /= extent;
            }
            sum += static_cast<double>(src.at(src_pos));
            ++count;
        }

        result.put(out_pos, count > 0 ? static_cast<T>(sum / static_cast<double>(count)) : T{});
    }
    return result;
}

template <typename T>
LatticeArray<T> RebinLattice<T>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <typename T>
T RebinLattice<T>::get_at(const IPosition& where) const {
    const auto ndim = shape_.ndim();
    const auto& ps = parent_->shape();

    double sum = 0.0;
    std::int64_t count = 0;

    std::vector<std::int64_t> lo(ndim);
    std::vector<std::int64_t> hi(ndim);
    for (std::size_t d = 0; d < ndim; ++d) {
        lo[d] = where[d] * factors_[d];
        hi[d] = std::min(lo[d] + factors_[d], ps[d]);
    }

    std::int64_t bin_total = 1;
    for (std::size_t d = 0; d < ndim; ++d) {
        bin_total *= (hi[d] - lo[d]);
    }
    for (std::int64_t b = 0; b < bin_total; ++b) {
        IPosition src_pos(ndim);
        std::int64_t brem = b;
        for (std::size_t d = 0; d < ndim; ++d) {
            auto extent = hi[d] - lo[d];
            src_pos[d] = lo[d] + brem % extent;
            brem /= extent;
        }
        sum += static_cast<double>(parent_->get_at(src_pos));
        ++count;
    }
    return count > 0 ? static_cast<T>(sum / static_cast<double>(count)) : T{};
}

template <typename T>
void RebinLattice<T>::put(const LatticeArray<T>& /*data*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

template <typename T>
void RebinLattice<T>::put_slice(const LatticeArray<T>& /*data*/,
                                 const Slicer& /*slicer*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

template <typename T>
void RebinLattice<T>::put_at(const T& /*value*/, const IPosition& /*where*/) {
    throw std::runtime_error("RebinLattice is read-only");
}

} // namespace casacore_mini
