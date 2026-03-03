// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>

// CI currently builds with Clang 18 + libstdc++ on Ubuntu 24.04, where the
// standard <mdspan> header is not available yet. Fall back to
// <experimental/mdspan> or a small local shim while keeping a single
// compatibility namespace.
#if __has_include(<mdspan>)
#include <mdspan>
namespace casacore_mini::mdspan_compat {
using std::dextents;
using std::layout_left;
using std::mdspan;
} // namespace casacore_mini::mdspan_compat
#elif __has_include(<experimental/mdspan>)
#include <experimental/mdspan>
namespace casacore_mini::mdspan_compat {
using std::experimental::dextents;
using std::experimental::layout_left;
using std::experimental::mdspan;
} // namespace casacore_mini::mdspan_compat
#else
namespace casacore_mini::mdspan_compat {

template <typename IndexType, std::size_t Rank> struct dextents {
    using index_type = IndexType;
    static constexpr std::size_t rank() noexcept {
        return Rank;
    }
};

struct layout_left {};

template <typename T, typename Extents, typename LayoutPolicy = layout_left> class mdspan {
  public:
    static constexpr std::size_t kRank = Extents::rank();
    using index_type = typename Extents::index_type;

    template <typename... Dims>
    explicit mdspan(T* data, Dims... dims)
        : data_(data), extents_{static_cast<std::size_t>(dims)...} {
        static_assert(sizeof...(Dims) == kRank, "mdspan rank mismatch");
        if constexpr (kRank > 0) {
            strides_[0] = 1;
            for (std::size_t i = 1; i < kRank; ++i) {
                strides_[i] = strides_[i - 1] * extents_[i - 1];
            }
        }
    }

    [[nodiscard]] static constexpr std::size_t rank() noexcept {
        return kRank;
    }

    [[nodiscard]] constexpr std::size_t extent(std::size_t dim) const noexcept {
        return extents_[dim];
    }

    template <typename... Indices> [[nodiscard]] T& operator[](Indices... indices) const {
        static_assert(sizeof...(Indices) == kRank, "mdspan index rank mismatch");
        std::array<std::size_t, kRank> idx{static_cast<std::size_t>(indices)...};
        std::size_t offset = 0;
        for (std::size_t i = 0; i < kRank; ++i) {
            offset += idx[i] * strides_[i];
        }
        return data_[offset];
    }

  private:
    T* data_;
    std::array<std::size_t, kRank> extents_{};
    std::array<std::size_t, kRank> strides_{};
};

} // namespace casacore_mini::mdspan_compat
#endif
