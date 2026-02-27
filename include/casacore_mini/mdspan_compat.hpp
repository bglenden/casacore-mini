#pragma once

// CI currently builds with Clang 18 + libstdc++ on Ubuntu 24.04, where the
// standard <mdspan> header is not available yet. Fall back to
// <experimental/mdspan> while keeping a single compatibility namespace.
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
#error "No mdspan implementation available (<mdspan> or <experimental/mdspan>)"
#endif
