#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include "casacore_mini/mdspan_compat.hpp"

namespace casacore_mini {

/// @file
/// @brief Shape, index, stride, and slice types for multidimensional
///   Fortran-order (column-major) lattice storage.
///
/// The view type for lattice data is `std::mdspan` with `layout_left`
/// (column-major / Fortran order), matching casacore's Array convention.

/// Multidimensional shape/index vector compatible with casacore `IPosition`.
///
/// Stores axis extents or axis indices as a small vector of signed 64-bit
/// integers. Negative values are valid for index-offset semantics but not for
/// shape extents.
class IPosition {
  public:
    /// Construct empty (rank-0) position.
    IPosition() = default;
    /// Construct with explicit rank, all axes set to `initial_value`.
    explicit IPosition(std::size_t ndim, std::int64_t initial_value = 0);
    /// Construct from initializer list: `IPosition{4, 5, 6}`.
    IPosition(std::initializer_list<std::int64_t> il);
    /// Construct from a vector.
    explicit IPosition(std::vector<std::int64_t> values);

    /// Number of dimensions.
    [[nodiscard]] std::size_t ndim() const noexcept {
        return values_.size();
    }
    /// Same as `ndim()`.
    [[nodiscard]] std::size_t size() const noexcept {
        return values_.size();
    }
    /// True if rank is 0.
    [[nodiscard]] bool empty() const noexcept {
        return values_.empty();
    }

    /// Mutable axis access.
    std::int64_t& operator[](std::size_t axis) {
        return values_[axis];
    }
    /// Const axis access.
    const std::int64_t& operator[](std::size_t axis) const {
        return values_[axis];
    }

    /// Product of all axis extents. Returns 1 for rank-0.
    [[nodiscard]] std::int64_t product() const noexcept;

    /// Const view as span.
    [[nodiscard]] std::span<const std::int64_t> as_span() const noexcept {
        return {values_.data(), values_.size()};
    }
    /// Convert to unsigned-extent vector (for RecordArray interop).
    [[nodiscard]] std::vector<std::uint64_t> to_unsigned() const;
    /// Construct from unsigned-extent vector.
    [[nodiscard]] static IPosition from_unsigned(std::span<const std::uint64_t> u);

    /// Element-wise equality.
    [[nodiscard]] bool operator==(const IPosition& other) const = default;
    [[nodiscard]] bool operator!=(const IPosition& other) const = default;

    /// Const iterator support.
    [[nodiscard]] auto begin() const noexcept {
        return values_.begin();
    }
    [[nodiscard]] auto end() const noexcept {
        return values_.end();
    }
    /// Mutable iterator support.
    auto begin() noexcept {
        return values_.begin();
    }
    auto end() noexcept {
        return values_.end();
    }

    /// Raw data pointer for mdspan extent construction.
    [[nodiscard]] const std::int64_t* data() const noexcept {
        return values_.data();
    }

    /// String representation for diagnostics: "(a, b, c)".
    [[nodiscard]] std::string to_string() const;

  private:
    std::vector<std::int64_t> values_;
};

/// Describes a multidimensional slice: start, length, stride per axis.
///
/// Equivalent to casacore's `Slicer`. All three vectors must have the same
/// rank. Stride values must be >= 1.
class Slicer {
  public:
    /// Construct a full-extent slicer for the given shape.
    static Slicer full(const IPosition& shape);
    /// Construct from explicit start, length, stride.
    /// @throws std::invalid_argument if ranks differ or strides < 1.
    Slicer(IPosition start, IPosition length, IPosition stride);
    /// Construct with unit stride.
    Slicer(IPosition start, const IPosition& length);

    [[nodiscard]] const IPosition& start() const noexcept {
        return start_;
    }
    [[nodiscard]] const IPosition& length() const noexcept {
        return length_;
    }
    [[nodiscard]] const IPosition& stride() const noexcept {
        return stride_;
    }
    [[nodiscard]] std::size_t ndim() const noexcept {
        return start_.ndim();
    }

    [[nodiscard]] bool operator==(const Slicer& other) const = default;

  private:
    IPosition start_;
    IPosition length_;
    IPosition stride_;
};

// ── mdspan type aliases ────────────────────────────────────────────────

/// Dynamic-rank mdspan extent type used throughout the lattice layer.
/// We use a fixed maximum rank of 8 (matching casacore practice).
inline constexpr std::size_t kMaxLatticeRank = 8;

/// Dynamic-extent mdspan with Fortran (column-major) layout.
///
/// This is the primary view type for lattice data. Data pointers are
/// non-owning; ownership lives in `LatticeArray<T>`.
template <typename T, std::size_t Rank>
using LatticeSpan = mdspan_compat::mdspan<T, mdspan_compat::dextents<std::size_t, Rank>,
                                          mdspan_compat::layout_left>;

/// Const version.
template <typename T, std::size_t Rank>
using ConstLatticeSpan = mdspan_compat::mdspan<const T, mdspan_compat::dextents<std::size_t, Rank>,
                                               mdspan_compat::layout_left>;

// ── free functions ─────────────────────────────────────────────────────

/// Compute Fortran-order (column-major) strides for a given shape.
///
/// stride[0] = 1, stride[i] = stride[i-1] * shape[i-1].
[[nodiscard]] IPosition fortran_strides(const IPosition& shape);

/// Linearize a multidimensional index to a flat Fortran-order offset.
///
/// @pre `index.ndim() == strides.ndim()`
[[nodiscard]] std::int64_t linear_index(const IPosition& index, const IPosition& strides) noexcept;

/// Convert a flat Fortran-order offset back to a multidimensional index.
///
/// @pre `offset >= 0 && offset < shape.product()`
[[nodiscard]] IPosition delinearize(std::int64_t offset, const IPosition& shape);

/// Validate that every element of `index` is in range `[0, shape[i])`.
///
/// @throws std::out_of_range with descriptive message on failure.
void validate_index(const IPosition& index, const IPosition& shape);

/// Validate a slicer against a shape: start + (length-1)*stride < shape per axis.
///
/// @throws std::out_of_range on invalid slice.
void validate_slicer(const Slicer& slicer, const IPosition& shape);

/// Copy elements between strided Fortran-order arrays using mdspan-style
/// stride arithmetic.
///
/// Copies `count` elements (the product of `slice_shape`) from `src` to `dst`,
/// where `src` is a flat Fortran-order array with strides `src_strides` and
/// elements are read at offsets `src_start[d] * src_strides[d]` with increments
/// of `src_step[d] * src_strides[d]`. The destination is densely packed in
/// Fortran order.
///
/// This replaces manual IPosition-based index iteration with direct stride
/// computation, matching how mdspan layout_left maps indices to offsets.
template <typename T>
void strided_fortran_copy(const T* src, const IPosition& src_strides, const IPosition& src_start,
                          const IPosition& src_step, T* dst, const IPosition& slice_shape) {
    const auto ndim = slice_shape.ndim();
    const auto n = static_cast<std::size_t>(slice_shape.product());
    if (n == 0)
        return;

    // Current position in slice coordinates (all zeros initially).
    std::vector<std::int64_t> pos(ndim, 0);

    // Compute initial source offset from start position.
    std::int64_t src_offset = 0;
    for (std::size_t d = 0; d < ndim; ++d) {
        src_offset += src_start[d] * src_strides[d];
    }

    for (std::size_t i = 0; i < n; ++i) {
        dst[i] = src[static_cast<std::size_t>(src_offset)];

        // Advance position in Fortran order (innermost axis first).
        for (std::size_t d = 0; d < ndim; ++d) {
            ++pos[d];
            src_offset += src_step[d] * src_strides[d];
            if (pos[d] < slice_shape[d])
                break;
            // Wrap: subtract the full extent advanced on this axis.
            src_offset -= pos[d] * src_step[d] * src_strides[d];
            pos[d] = 0;
        }
    }
}

/// Scatter elements from a dense Fortran-order source into a strided
/// destination array. Inverse of `strided_fortran_copy`.
template <typename T>
void strided_fortran_scatter(const T* src, T* dst, const IPosition& dst_strides,
                             const IPosition& dst_start, const IPosition& dst_step,
                             const IPosition& slice_shape) {
    const auto ndim = slice_shape.ndim();
    const auto n = static_cast<std::size_t>(slice_shape.product());
    if (n == 0)
        return;

    std::vector<std::int64_t> pos(ndim, 0);

    std::int64_t dst_offset = 0;
    for (std::size_t d = 0; d < ndim; ++d) {
        dst_offset += dst_start[d] * dst_strides[d];
    }

    for (std::size_t i = 0; i < n; ++i) {
        dst[static_cast<std::size_t>(dst_offset)] = src[i];

        for (std::size_t d = 0; d < ndim; ++d) {
            ++pos[d];
            dst_offset += dst_step[d] * dst_strides[d];
            if (pos[d] < slice_shape[d])
                break;
            dst_offset -= pos[d] * dst_step[d] * dst_strides[d];
            pos[d] = 0;
        }
    }
}

/// Create a `ConstLatticeSpan` (mdspan view) over a flat data array.
///
/// @tparam N   Compile-time rank of the resulting mdspan.
/// @param data Pointer to flat Fortran-order data.
/// @param shape Shape vector whose ndim must equal N.
/// @throws std::invalid_argument if shape.ndim() != N.
template <typename T, std::size_t N>
[[nodiscard]] ConstLatticeSpan<T, N> make_const_lattice_span(const T* data,
                                                             const IPosition& shape) {
    if (shape.ndim() != N) {
        throw std::invalid_argument("make_const_lattice_span: shape rank != N");
    }
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return ConstLatticeSpan<T, N>(data, static_cast<std::size_t>(shape[I])...);
    }(std::make_index_sequence<N>{});
}

/// Create a `LatticeSpan` (mutable mdspan view) over a flat data array.
template <typename T, std::size_t N>
[[nodiscard]] LatticeSpan<T, N> make_lattice_span(T* data, const IPosition& shape) {
    if (shape.ndim() != N) {
        throw std::invalid_argument("make_lattice_span: shape rank != N");
    }
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return LatticeSpan<T, N>(data, static_cast<std::size_t>(shape[I])...);
    }(std::make_index_sequence<N>{});
}

} // namespace casacore_mini
