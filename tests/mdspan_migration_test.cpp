/// @file mdspan_migration_test.cpp
/// @brief P12-W13 tests for mdspan foundation helpers and migration parity.

#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"
#include "casacore_mini/mdspan_compat.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace casacore_mini;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "FAIL: " << label << "\n";
    }
}

// ============================================================================
// Test: strided_fortran_copy (unit stride)
// ============================================================================
static void test_strided_copy_unit() {
    // 3x4 array in Fortran order, extract 2x2 subarray starting at (1,1)
    IPosition shape{3, 4};
    std::vector<double> data(12);
    for (int i = 0; i < 12; ++i) data[static_cast<std::size_t>(i)] = static_cast<double>(i);

    // Fortran order: data[col*3 + row]
    // (1,1) -> index 4, (2,1) -> 5, (1,2) -> 7, (2,2) -> 8
    IPosition src_strides = fortran_strides(shape);
    IPosition start{1, 1};
    IPosition step{1, 1};
    IPosition slice_shape{2, 2};

    std::vector<double> result(4);
    strided_fortran_copy(data.data(), src_strides, start, step, result.data(), slice_shape);

    check(result[0] == 4.0, "copy unit (0,0) = 4");
    check(result[1] == 5.0, "copy unit (1,0) = 5");
    check(result[2] == 7.0, "copy unit (0,1) = 7");
    check(result[3] == 8.0, "copy unit (0,1) = 8");
}

// ============================================================================
// Test: strided_fortran_copy (stride > 1)
// ============================================================================
static void test_strided_copy_step() {
    // 6x6 array, extract every other element: start=(0,0), step=(2,2), len=(3,3)
    IPosition shape{6, 6};
    std::vector<float> data(36);
    for (int i = 0; i < 36; ++i) data[static_cast<std::size_t>(i)] = static_cast<float>(i);

    IPosition src_strides = fortran_strides(shape);
    IPosition start{0, 0};
    IPosition step{2, 2};
    IPosition slice_shape{3, 3};

    std::vector<float> result(9);
    strided_fortran_copy(data.data(), src_strides, start, step, result.data(), slice_shape);

    // Expected: (0,0)=0, (2,0)=2, (4,0)=4, (0,2)=12, (2,2)=14, ...
    check(result[0] == 0.0F, "copy step (0,0) = 0");
    check(result[1] == 2.0F, "copy step (1,0) = 2");
    check(result[2] == 4.0F, "copy step (2,0) = 4");
    check(result[3] == 12.0F, "copy step (0,1) = 12");
    check(result[4] == 14.0F, "copy step (1,1) = 14");
}

// ============================================================================
// Test: strided_fortran_scatter
// ============================================================================
static void test_strided_scatter() {
    // 4x4 array, scatter 2x2 patch at (1,1)
    IPosition shape{4, 4};
    std::vector<double> dst(16, 0.0);
    std::vector<double> src = {10.0, 20.0, 30.0, 40.0};

    IPosition dst_strides = fortran_strides(shape);
    IPosition start{1, 1};
    IPosition step{1, 1};
    IPosition slice_shape{2, 2};

    strided_fortran_scatter(src.data(), dst.data(), dst_strides, start, step, slice_shape);

    // (1,1) -> index 5, (2,1) -> 6, (1,2) -> 9, (2,2) -> 10
    check(dst[5] == 10.0, "scatter (1,1) = 10");
    check(dst[6] == 20.0, "scatter (2,1) = 20");
    check(dst[9] == 30.0, "scatter (1,2) = 30");
    check(dst[10] == 40.0, "scatter (2,2) = 40");
    check(dst[0] == 0.0, "scatter: unchanged cells stay 0");
}

// ============================================================================
// Test: LatticeArray get_slice parity with mdspan-based impl
// ============================================================================
static void test_lattice_array_get_slice_parity() {
    // 4x3 array
    LatticeArray<double> arr(IPosition{4, 3});
    arr.make_unique();
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 4; ++i)
            arr.put(IPosition{i, j}, static_cast<double>(j * 10 + i));

    // Slice [1:2, 0:1] → 2x2
    auto sub = arr.get_slice(Slicer(IPosition{1, 0}, IPosition{2, 2}));
    check(sub.shape() == IPosition({2, 2}), "slice shape 2x2");
    check(sub.at(IPosition{0, 0}) == 1.0, "slice (0,0) = 1");
    check(sub.at(IPosition{1, 0}) == 2.0, "slice (1,0) = 2");
    check(sub.at(IPosition{0, 1}) == 11.0, "slice (0,1) = 11");
    check(sub.at(IPosition{1, 1}) == 12.0, "slice (1,1) = 12");
}

// ============================================================================
// Test: LatticeArray get_slice with stride parity
// ============================================================================
static void test_lattice_array_get_slice_stride_parity() {
    // 5x5 array
    LatticeArray<float> arr(IPosition{5, 5});
    arr.make_unique();
    for (int j = 0; j < 5; ++j)
        for (int i = 0; i < 5; ++i)
            arr.put(IPosition{i, j}, static_cast<float>(j * 10 + i));

    // Slice with stride 2: start=(0,0), len=(3,3), stride=(2,2)
    auto sub = arr.get_slice(Slicer(IPosition{0, 0}, IPosition{3, 3}, IPosition{2, 2}));
    check(sub.shape() == IPosition({3, 3}), "strided slice shape 3x3");
    check(sub.at(IPosition{0, 0}) == 0.0F, "strided (0,0) = 0");
    check(sub.at(IPosition{1, 0}) == 2.0F, "strided (1,0) = 2");
    check(sub.at(IPosition{2, 0}) == 4.0F, "strided (2,0) = 4");
    check(sub.at(IPosition{0, 1}) == 20.0F, "strided (0,1) = 20");
    check(sub.at(IPosition{1, 1}) == 22.0F, "strided (1,1) = 22");
}

// ============================================================================
// Test: LatticeArray put_slice parity
// ============================================================================
static void test_lattice_array_put_slice_parity() {
    LatticeArray<double> arr(IPosition{4, 4}, 0.0);
    arr.make_unique();
    LatticeArray<double> patch(IPosition{2, 2});
    patch.make_unique();
    patch.put(IPosition{0, 0}, 1.0);
    patch.put(IPosition{1, 0}, 2.0);
    patch.put(IPosition{0, 1}, 3.0);
    patch.put(IPosition{1, 1}, 4.0);

    arr.put_slice(patch, Slicer(IPosition{1, 1}, IPosition{2, 2}));
    check(arr.at(IPosition{1, 1}) == 1.0, "put_slice (1,1) = 1");
    check(arr.at(IPosition{2, 1}) == 2.0, "put_slice (2,1) = 2");
    check(arr.at(IPosition{1, 2}) == 3.0, "put_slice (1,2) = 3");
    check(arr.at(IPosition{2, 2}) == 4.0, "put_slice (2,2) = 4");
    check(arr.at(IPosition{0, 0}) == 0.0, "put_slice: unchanged (0,0) = 0");
}

// ============================================================================
// Test: 3D strided copy
// ============================================================================
static void test_3d_strided_copy() {
    // 3x3x3 array
    IPosition shape{3, 3, 3};
    std::vector<int> data(27);
    for (int i = 0; i < 27; ++i) data[static_cast<std::size_t>(i)] = i;

    IPosition src_strides = fortran_strides(shape);
    IPosition start{1, 1, 1};
    IPosition step{1, 1, 1};
    IPosition slice_shape{2, 2, 2};

    std::vector<int> result(8);
    strided_fortran_copy(data.data(), src_strides, start, step, result.data(), slice_shape);

    // (1,1,1)=13, (2,1,1)=14, (1,2,1)=16, (2,2,1)=17,
    // (1,1,2)=22, (2,1,2)=23, (1,2,2)=25, (2,2,2)=26
    check(result[0] == 13, "3D (0,0,0) = 13");
    check(result[1] == 14, "3D (1,0,0) = 14");
    check(result[2] == 16, "3D (0,1,0) = 16");
    check(result[3] == 17, "3D (1,1,0) = 17");
    check(result[4] == 22, "3D (0,0,1) = 22");
    check(result[7] == 26, "3D (1,1,1) = 26");
}

// ============================================================================
// Test: make_const_lattice_span
// ============================================================================
static void test_make_const_lattice_span() {
    std::vector<double> data = {1, 2, 3, 4, 5, 6};
    IPosition shape{2, 3};

    auto span = make_const_lattice_span<double, 2>(data.data(), shape);
    check(span.rank() == 2, "span rank = 2");
    check(span.extent(0) == 2, "span extent(0) = 2");
    check(span.extent(1) == 3, "span extent(1) = 3");
    // layout_left: (0,0)=1, (1,0)=2, (0,1)=3, (1,1)=4, (0,2)=5, (1,2)=6
    check(span[0, 0] == 1.0, "span[0,0] = 1");
    check(span[1, 0] == 2.0, "span[1,0] = 2");
    check(span[0, 1] == 3.0, "span[0,1] = 3");
    check(span[1, 2] == 6.0, "span[1,2] = 6");
}

// ============================================================================
// Test: make_lattice_span (mutable)
// ============================================================================
static void test_make_lattice_span() {
    std::vector<float> data(6, 0.0F);
    IPosition shape{3, 2};

    auto span = make_lattice_span<float, 2>(data.data(), shape);
    span[1, 1] = 42.0F;
    check(data[4] == 42.0F, "mutable span[1,1] writes to data[4]");
}

// ============================================================================
// Test: mdspan compat shim basic functionality
// ============================================================================
static void test_mdspan_compat_basic() {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    mdspan_compat::mdspan<int, mdspan_compat::dextents<std::size_t, 3>,
                          mdspan_compat::layout_left> span(data.data(), 2, 2, 2);

    check(span.rank() == 3, "mdspan rank = 3");
    check(span.extent(0) == 2, "mdspan extent(0) = 2");
    // layout_left: (0,0,0)=0, (1,0,0)=1, (0,1,0)=2, (1,1,0)=3, (0,0,1)=4...
    check(span[0, 0, 0] == 1, "mdspan[0,0,0] = data[0]");
    check(span[1, 0, 0] == 2, "mdspan[1,0,0] = data[1]");
    check(span[0, 1, 0] == 3, "mdspan[0,1,0] = data[2]");
    check(span[0, 0, 1] == 5, "mdspan[0,0,1] = data[4]");
}

int main() {
    test_strided_copy_unit();
    test_strided_copy_step();
    test_strided_scatter();
    test_lattice_array_get_slice_parity();
    test_lattice_array_get_slice_stride_parity();
    test_lattice_array_put_slice_parity();
    test_3d_strided_copy();
    test_make_const_lattice_span();
    test_make_lattice_span();
    test_mdspan_compat_basic();

    std::cout << "mdspan_migration_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
