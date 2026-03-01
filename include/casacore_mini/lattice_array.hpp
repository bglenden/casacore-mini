#pragma once

#include "casacore_mini/lattice_shape.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Reference-counted owning multidimensional array with mdspan views.
///
/// `LatticeArray<T>` owns a flat `std::vector<T>` in Fortran (column-major)
/// order and provides `std::mdspan` views over it. Copies share the
/// underlying storage (copy-on-write semantics via `std::shared_ptr`).

/// Reference-counted multidimensional array with Fortran-order storage.
///
/// Storage is shared between copies until mutation is requested via
/// `make_unique()`. The primary data access interface is through mdspan
/// views returned by `view()` and `const_view()`.
template <typename T>
class LatticeArray {
  public:
    /// Construct an empty (rank-0, zero-element) array.
    LatticeArray() : shape_{}, storage_(std::make_shared<std::vector<T>>()) {}

    /// Construct array with given shape, all elements value-initialized.
    explicit LatticeArray(IPosition shape)
        : shape_(std::move(shape)),
          storage_(std::make_shared<std::vector<T>>(
              static_cast<std::size_t>(shape_.product()))) {}

    /// Construct array with given shape and initial value.
    LatticeArray(IPosition shape, T value)
        : shape_(std::move(shape)),
          storage_(std::make_shared<std::vector<T>>(
              static_cast<std::size_t>(shape_.product()), value)) {}

    /// Construct from shape and pre-existing flat data (takes ownership).
    /// @throws std::invalid_argument if data size != shape product.
    LatticeArray(IPosition shape, std::vector<T> data)
        : shape_(std::move(shape)),
          storage_(std::make_shared<std::vector<T>>(std::move(data))) {
        if (static_cast<std::int64_t>(storage_->size()) != shape_.product()) {
            throw std::invalid_argument(
                "LatticeArray: data size (" + std::to_string(storage_->size()) +
                ") != shape product (" + std::to_string(shape_.product()) + ")");
        }
    }

    /// Shape of the array.
    [[nodiscard]] const IPosition& shape() const noexcept { return shape_; }

    /// Number of dimensions (rank).
    [[nodiscard]] std::size_t ndim() const noexcept { return shape_.ndim(); }

    /// Total number of elements.
    [[nodiscard]] std::size_t nelements() const noexcept {
        return storage_->size();
    }

    /// True if the underlying storage is exclusively owned (use_count == 1).
    [[nodiscard]] bool is_unique() const noexcept {
        return storage_.use_count() == 1;
    }

    /// Ensure this array has exclusive ownership of its storage.
    /// Performs a deep copy if shared.
    void make_unique() {
        if (!is_unique()) {
            storage_ = std::make_shared<std::vector<T>>(*storage_);
        }
    }

    /// Const pointer to the flat Fortran-order data.
    [[nodiscard]] const T* data() const noexcept { return storage_->data(); }

    /// Mutable pointer to the flat data. Caller must ensure unique ownership
    /// (call `make_unique()` first if the array might be shared).
    [[nodiscard]] T* mutable_data() noexcept {
        assert(is_unique() && "mutable_data() called on shared storage");
        return storage_->data();
    }

    /// Const reference to the underlying flat vector.
    [[nodiscard]] const std::vector<T>& flat() const noexcept {
        return *storage_;
    }

    /// Return a rank-N mdspan view over the data (const).
    ///
    /// Usage: `auto v = arr.const_view<3>();` gives a 3D column-major view.
    /// @pre `N == ndim()`
    template <std::size_t N>
    [[nodiscard]] ConstLatticeSpan<T, N> const_view() const {
        check_rank<N>();
        return make_const_span<N>(std::make_index_sequence<N>{});
    }

    /// Return a rank-N mdspan view over the data (mutable).
    ///
    /// @pre `N == ndim()` and `is_unique()`
    template <std::size_t N>
    [[nodiscard]] LatticeSpan<T, N> view() {
        check_rank<N>();
        assert(is_unique() && "view() called on shared storage");
        return make_span<N>(std::make_index_sequence<N>{});
    }

    /// Element access by flat index (returns by value to support vector<bool>).
    [[nodiscard]] T operator[](std::size_t i) const { return (*storage_)[i]; }

    /// Read a single element by multidimensional index.
    [[nodiscard]] T at(const IPosition& index) const {
        validate_index(index, shape_);
        auto strides = fortran_strides(shape_);
        auto offset = static_cast<std::size_t>(linear_index(index, strides));
        return (*storage_)[offset];
    }

    /// Write a single element by multidimensional index.
    /// @pre `is_unique()`
    void put(const IPosition& index, const T& value) {
        assert(is_unique() && "put() called on shared storage");
        validate_index(index, shape_);
        auto strides = fortran_strides(shape_);
        auto offset = static_cast<std::size_t>(linear_index(index, strides));
        (*storage_)[offset] = value;
    }

    /// Extract a contiguous slice into a new LatticeArray.
    ///
    /// Copies data from the region specified by `slicer` into a new array
    /// whose shape is `slicer.length()`. Uses mdspan-style strided copy
    /// with Fortran-order stride arithmetic.
    [[nodiscard]] LatticeArray<T> get_slice(const Slicer& slicer) const {
        validate_slicer(slicer, shape_);
        const auto& sl = slicer.length();
        auto n = static_cast<std::size_t>(sl.product());
        std::vector<T> result(n);

        if constexpr (std::is_same_v<T, bool>) {
            // vector<bool> has no .data(); use element-wise access.
            get_slice_elementwise(slicer, result);
        } else {
            strided_fortran_copy(storage_->data(), fortran_strides(shape_),
                                 slicer.start(), slicer.stride(),
                                 result.data(), sl);
        }
        return LatticeArray<T>(IPosition(sl), std::move(result));
    }

    /// Write a slice from `source` into this array at `slicer` positions.
    ///
    /// @pre `is_unique()` and `source.shape() == slicer.length()`
    void put_slice(const LatticeArray<T>& source, const Slicer& slicer) {
        assert(is_unique() && "put_slice() called on shared storage");
        validate_slicer(slicer, shape_);

        if constexpr (std::is_same_v<T, bool>) {
            put_slice_elementwise(source, slicer);
        } else {
            strided_fortran_scatter(source.flat().data(), storage_->data(),
                                    fortran_strides(shape_),
                                    slicer.start(), slicer.stride(),
                                    slicer.length());
        }
    }

    [[nodiscard]] bool operator==(const LatticeArray& other) const {
        return shape_ == other.shape_ && *storage_ == *other.storage_;
    }

  private:
    template <std::size_t N>
    void check_rank() const {
        if (N != ndim()) {
            throw std::invalid_argument(
                "mdspan rank " + std::to_string(N) +
                " does not match array ndim " + std::to_string(ndim()));
        }
    }

    template <std::size_t N, std::size_t... I>
    [[nodiscard]] ConstLatticeSpan<T, N> make_const_span(
        std::index_sequence<I...> /*seq*/) const {
        return ConstLatticeSpan<T, N>(
            storage_->data(),
            static_cast<std::size_t>(shape_[I])...);
    }

    template <std::size_t N, std::size_t... I>
    [[nodiscard]] LatticeSpan<T, N> make_span(
        std::index_sequence<I...> /*seq*/) {
        return LatticeSpan<T, N>(
            storage_->data(),
            static_cast<std::size_t>(shape_[I])...);
    }

    /// Element-wise slice copy for vector<bool> (no .data() pointer).
    void get_slice_elementwise(const Slicer& slicer, std::vector<T>& result) const {
        const auto& sl = slicer.length();
        const auto& ss = slicer.start();
        const auto& st = slicer.stride();
        auto n = static_cast<std::size_t>(sl.product());
        auto src_strides = fortran_strides(shape_);

        IPosition idx(sl.ndim(), 0);
        for (std::size_t i = 0; i < n; ++i) {
            IPosition src_idx(sl.ndim());
            for (std::size_t d = 0; d < sl.ndim(); ++d) {
                src_idx[d] = ss[d] + idx[d] * st[d];
            }
            result[i] = (*storage_)[static_cast<std::size_t>(
                linear_index(src_idx, src_strides))];
            for (std::size_t d = 0; d < sl.ndim(); ++d) {
                if (++idx[d] < sl[d]) break;
                idx[d] = 0;
            }
        }
    }

    /// Element-wise slice scatter for vector<bool>.
    void put_slice_elementwise(const LatticeArray<T>& source, const Slicer& slicer) {
        const auto& sl = slicer.length();
        const auto& ss = slicer.start();
        const auto& st = slicer.stride();
        auto src_strides = fortran_strides(source.shape());
        auto dst_strides = fortran_strides(shape_);
        auto n = static_cast<std::size_t>(sl.product());

        IPosition idx(sl.ndim(), 0);
        for (std::size_t i = 0; i < n; ++i) {
            IPosition dst_idx(sl.ndim());
            for (std::size_t d = 0; d < sl.ndim(); ++d) {
                dst_idx[d] = ss[d] + idx[d] * st[d];
            }
            (*storage_)[static_cast<std::size_t>(linear_index(dst_idx, dst_strides))] =
                source.flat()[static_cast<std::size_t>(linear_index(idx, src_strides))];
            for (std::size_t d = 0; d < sl.ndim(); ++d) {
                if (++idx[d] < sl[d]) break;
                idx[d] = 0;
            }
        }
    }

    IPosition shape_;
    std::shared_ptr<std::vector<T>> storage_;
};

} // namespace casacore_mini
