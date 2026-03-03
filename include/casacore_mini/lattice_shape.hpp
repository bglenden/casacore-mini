// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
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

// ── IPosition ──────────────────────────────────────────────────────────

/// 
/// Multidimensional shape and index vector, compatible with casacore IPosition.
/// 
///
///
///
/// 
/// IPosition stores a small vector of signed 64-bit integers representing
/// either axis extents (a shape) or axis indices (a coordinate).  It is the
/// primary currency for specifying shapes, positions, strides, and slice
/// boundaries throughout the casacore-mini lattice layer.
///
/// Design notes:
///
///   - Values are stored in a `std::vector<int64_t>` so the rank
///        is a runtime quantity; there is no compile-time rank.
///   - Negative values are permitted for index-offset semantics, but must
///        not appear in shape extents (all extents must be >= 1).
///   - A rank-0 (empty) IPosition is valid and represents a scalar.
///   - `product()` returns 1 for rank-0, consistent with the
///        convention that an empty product equals the multiplicative identity.
///
///
/// The class mirrors casacore's `IPosition` but drops rarely-used
/// relational operators and AIPS++ string-formatting conventions in favour of
/// a simple `to_string()` diagnostic helper.
/// 
///
/// @par Example
/// Constructing shapes and performing index arithmetic:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // Shape for a 64 x 64 x 16 cube.
///   IPosition shape{64, 64, 16};
///   assert(shape.ndim() == 3);
///   assert(shape.product() == 64 * 64 * 16);
///
///   // Index a single voxel.
///   IPosition idx{10, 20, 5};
///   auto strides = fortran_strides(shape);
///   auto offset  = linear_index(idx, strides);
///
///   // Construct with explicit rank and fill value.
///   IPosition zeros(4, 0);   // four-dimensional zero index
///   IPosition ones (3, 1);   // unit strides placeholder
/// @endcode
/// 
///
/// @par Motivation
/// A uniform, rank-agnostic integer-vector type is necessary to express
/// shapes, indices, and strides without specialising every algorithm to a
/// fixed number of dimensions.  Signed 64-bit storage avoids silent overflow
/// when computing large array extents (e.g. a 65536^3 cube).
/// 
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

// ── Slicer ─────────────────────────────────────────────────────────────

/// 
/// Describes a multidimensional slice: start position, length, and stride
/// per axis.
/// 
///
///
///
/// 
/// A Slicer specifies a regular sub-region of a lattice or array by giving,
/// for each axis, three quantities:
///
///
///   - <b>start</b>  — the first element index on that axis (0-based).
///   - <b>length</b> — the number of output elements on that axis.
///   - <b>stride</b> — the step between consecutive input elements
///        (must be >= 1; stride 1 selects contiguous elements).
///
///
/// The last input element selected on axis <i>d</i> is
/// `start[d] + (length[d] - 1) * stride[d]`, which must be less
/// than the lattice extent on that axis.  All three IPosition vectors must
/// have the same rank.
///
/// Use `Slicer::full(shape)` to construct a slicer that covers the
/// entire array, or the two-argument constructor for unit-stride slices.
///
/// Equivalent to casacore's `Slicer` class but restricted to the
/// (start, length, stride) parameterisation; the (start, end, stride)
/// variant is not provided.
/// 
///
/// @par Example
/// Selecting a strided sub-region:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // Shape of the full lattice.
///   IPosition shape{128, 128, 64};
///
///   // Full-extent slicer (selects everything).
///   auto sl_all = Slicer::full(shape);
///
///   // Select columns 10..17 and rows 20..27 in plane 0, unit stride.
///   Slicer sl(IPosition{10, 20, 0}, IPosition{8, 8, 1});
///
///   // Every other element in x, every third in y, all of z.
///   Slicer sl_strided(
///       IPosition{0, 0, 0},
///       IPosition{64, 43, 64},
///       IPosition{2, 3, 1});
///
///   // Validate against the lattice shape.
///   validate_slicer(sl_strided, shape);
/// @endcode
/// 
///
/// @warning
/// The `length` field gives the number of *output* elements, not
/// the index of the last element.  Confusing length with end-index is a
/// common source of off-by-one errors.
/// 
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

/// 
/// Dynamic-extent mdspan with Fortran (column-major) layout.
/// 
///
/// 
/// This is the primary non-owning view type for lattice data.  Data pointers
/// are non-owning; lifetime and ownership live in `LatticeArray<T>`.
/// The `layout_left` policy ensures the first axis varies fastest,
/// matching casacore's Array storage convention.
///
/// The compile-time rank `Rank` must be known at the call site; use
/// `make_const_lattice_span<T, N>()` to construct a view from a flat
/// pointer and an IPosition shape.
/// 
template <typename T, std::size_t Rank>
using LatticeSpan = mdspan_compat::mdspan<T, mdspan_compat::dextents<std::size_t, Rank>,
                                          mdspan_compat::layout_left>;

/// Const version of LatticeSpan: non-owning read-only mdspan view with
/// Fortran (column-major) layout.
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

/// 
/// Copy elements between strided Fortran-order arrays using mdspan-style
/// stride arithmetic.
/// 
///
/// 
/// Copies `count` elements (the product of `slice_shape`) from `src` to
/// `dst`, where `src` is a flat Fortran-order array with strides
/// `src_strides` and elements are read at offsets
/// `src_start[d] * src_strides[d]` with increments of
/// `src_step[d] * src_strides[d]`.  The destination is densely
/// packed in Fortran order.
///
/// This replaces manual IPosition-based index iteration with direct stride
/// computation, matching how mdspan `layout_left` maps indices to
/// offsets.  It is the inner kernel of `LatticeArray::get_slice()`.
/// 
///
/// @warning
/// This function does not check bounds.  Call `validate_slicer()`
/// before invoking to ensure the selected region lies within `src`.
/// 
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

/// 
/// Scatter elements from a dense Fortran-order source into a strided
/// destination array.
/// 
///
/// 
/// Inverse of `strided_fortran_copy`: reads elements sequentially
/// from the dense `src` buffer and writes them into `dst` at positions
/// determined by `dst_start`, `dst_step`, and `dst_strides`.  This is the
/// inner kernel of `LatticeArray::put_slice()`.
/// 
///
/// @warning
/// This function does not check bounds.  Call `validate_slicer()`
/// before invoking to ensure the target region lies within `dst`.
/// 
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
