#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace casacore_mini;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__               \
                      << "]: " #cond << "\n";                                  \
            ++g_fail;                                                          \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (false)

// ── IPosition tests ────────────────────────────────────────────────────

static void test_iposition_basic() {
    IPosition empty;
    CHECK(empty.ndim() == 0);
    CHECK(empty.empty());
    CHECK(empty.product() == 1);

    IPosition p3(3, 5);
    CHECK(p3.ndim() == 3);
    CHECK(p3[0] == 5);
    CHECK(p3[1] == 5);
    CHECK(p3[2] == 5);
    CHECK(p3.product() == 125);

    IPosition il{4, 5, 6};
    CHECK(il.ndim() == 3);
    CHECK(il[0] == 4);
    CHECK(il[1] == 5);
    CHECK(il[2] == 6);
    CHECK(il.product() == 120);
}

static void test_iposition_equality() {
    IPosition a{2, 3, 4};
    IPosition b{2, 3, 4};
    IPosition c{2, 3, 5};
    CHECK(a == b);
    CHECK(a != c);
}

static void test_iposition_unsigned_roundtrip() {
    IPosition original{10, 20, 30};
    auto u = original.to_unsigned();
    CHECK(u.size() == 3);
    CHECK(u[0] == 10);
    auto back = IPosition::from_unsigned(u);
    CHECK(back == original);
}

static void test_iposition_to_string() {
    IPosition p{3, 4, 5};
    CHECK(p.to_string() == "(3, 4, 5)");
    IPosition empty;
    CHECK(empty.to_string() == "()");
}

// ── Slicer tests ───────────────────────────────────────────────────────

static void test_slicer_full() {
    IPosition shape{10, 20, 30};
    auto s = Slicer::full(shape);
    CHECK(s.ndim() == 3);
    CHECK(s.start() == IPosition(3, 0));
    CHECK(s.length() == shape);
    CHECK(s.stride() == IPosition(3, 1));
}

static void test_slicer_custom() {
    Slicer s(IPosition{2, 3}, IPosition{4, 5}, IPosition{1, 2});
    CHECK(s.start()[0] == 2);
    CHECK(s.length()[1] == 5);
    CHECK(s.stride()[1] == 2);
}

static void test_slicer_rank_mismatch() {
    bool caught = false;
    try {
        Slicer s(IPosition{0, 0}, IPosition{1, 1, 1});
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    CHECK(caught);
}

static void test_slicer_bad_stride() {
    bool caught = false;
    try {
        Slicer s(IPosition{0}, IPosition{5}, IPosition{0});
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    CHECK(caught);
}

// ── Stride/index tests ────────────────────────────────────────────────

static void test_fortran_strides() {
    IPosition shape{4, 5, 6};
    auto strides = fortran_strides(shape);
    CHECK(strides[0] == 1);
    CHECK(strides[1] == 4);
    CHECK(strides[2] == 20);
}

static void test_linear_index_roundtrip() {
    IPosition shape{3, 4, 5};
    auto strides = fortran_strides(shape);
    IPosition idx{1, 2, 3};
    auto offset = linear_index(idx, strides);
    // Fortran order: 1 + 2*3 + 3*3*4 = 1 + 6 + 36 = 43
    CHECK(offset == 43);
    auto back = delinearize(offset, shape);
    CHECK(back == idx);
}

static void test_delinearize_zero() {
    IPosition shape{4, 5};
    auto idx = delinearize(0, shape);
    CHECK(idx == (IPosition{0, 0}));
}

static void test_validate_index_ok() {
    IPosition shape{4, 5, 6};
    validate_index(IPosition{0, 0, 0}, shape);
    validate_index(IPosition{3, 4, 5}, shape);
    // Should not throw.
    CHECK(true);
}

static void test_validate_index_out_of_range() {
    IPosition shape{4, 5, 6};
    bool caught = false;
    try {
        validate_index(IPosition{4, 0, 0}, shape);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    CHECK(caught);

    caught = false;
    try {
        validate_index(IPosition{0, -1, 0}, shape);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    CHECK(caught);
}

static void test_validate_slicer_ok() {
    IPosition shape{10, 20};
    Slicer s(IPosition{2, 3}, IPosition{5, 10});
    validate_slicer(s, shape);
    CHECK(true);
}

static void test_validate_slicer_overflow() {
    IPosition shape{10, 20};
    bool caught = false;
    try {
        Slicer s(IPosition{8, 0}, IPosition{5, 5});
        validate_slicer(s, shape);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    CHECK(caught);
}

// ── LatticeArray tests ─────────────────────────────────────────────────

static void test_lattice_array_construction() {
    LatticeArray<float> empty;
    CHECK(empty.ndim() == 0);
    CHECK(empty.nelements() == 0);

    LatticeArray<float> arr(IPosition{3, 4});
    CHECK(arr.ndim() == 2);
    CHECK(arr.nelements() == 12);
    CHECK(arr.shape() == (IPosition{3, 4}));

    LatticeArray<double> filled(IPosition{2, 3}, 1.5);
    CHECK(filled.nelements() == 6);
    CHECK(filled[0] == 1.5);
    CHECK(filled[5] == 1.5);
}

static void test_lattice_array_from_data() {
    std::vector<int> data{1, 2, 3, 4, 5, 6};
    LatticeArray<int> arr(IPosition{2, 3}, std::move(data));
    CHECK(arr.nelements() == 6);
    CHECK(arr[0] == 1);
    CHECK(arr[5] == 6);
}

static void test_lattice_array_bad_data_size() {
    bool caught = false;
    try {
        LatticeArray<int> arr(IPosition{2, 3}, std::vector<int>{1, 2, 3});
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    CHECK(caught);
}

static void test_lattice_array_shared_storage() {
    LatticeArray<float> a(IPosition{4, 5}, 1.0f);
    auto b = a;  // Shared copy.
    CHECK(!a.is_unique());
    CHECK(!b.is_unique());
    CHECK(a.data() == b.data());  // Same underlying pointer.

    b.make_unique();
    CHECK(b.is_unique());
    CHECK(a.data() != b.data());  // Now separate.
    CHECK(a == b);  // Still equal values.
}

static void test_lattice_array_at_and_put() {
    LatticeArray<int> arr(IPosition{3, 4}, 0);
    CHECK(arr.at(IPosition{0, 0}) == 0);

    arr.put(IPosition{1, 2}, 42);
    CHECK(arr.at(IPosition{1, 2}) == 42);
    // Fortran order: offset = 1 + 2*3 = 7
    CHECK(arr[7] == 42);
}

static void test_lattice_array_mdspan_view() {
    LatticeArray<float> arr(IPosition{3, 4}, 0.0f);
    // Fill with known pattern.
    for (std::size_t i = 0; i < arr.nelements(); ++i) {
        arr.mutable_data()[i] = static_cast<float>(i);
    }

    auto v = arr.const_view<2>();
    CHECK(v.extent(0) == 3);
    CHECK(v.extent(1) == 4);
    // mdspan layout_left: v[i, j] at offset i + j*3
    // Wrap multi-index access to avoid preprocessor comma splitting.
    CHECK((v[0, 0]) == 0.0f);
    CHECK((v[1, 0]) == 1.0f);
    CHECK((v[0, 1]) == 3.0f);
    CHECK((v[2, 3]) == 11.0f);  // 2 + 3*3 = 11
}

static void test_lattice_array_mdspan_mutable_view() {
    LatticeArray<double> arr(IPosition{2, 2}, 0.0);
    auto v = arr.view<2>();
    v[0, 0] = 1.0;
    v[1, 0] = 2.0;
    v[0, 1] = 3.0;
    v[1, 1] = 4.0;
    CHECK(arr.at(IPosition{0, 0}) == 1.0);
    CHECK(arr.at(IPosition{1, 0}) == 2.0);
    CHECK(arr.at(IPosition{0, 1}) == 3.0);
    CHECK(arr.at(IPosition{1, 1}) == 4.0);
}

static void test_lattice_array_3d_mdspan() {
    LatticeArray<int> arr(IPosition{2, 3, 4}, 0);
    auto v = arr.view<3>();
    v[1, 2, 3] = 99;
    // Fortran offset: 1 + 2*2 + 3*2*3 = 1 + 4 + 18 = 23
    CHECK(arr[23] == 99);
    CHECK(arr.at(IPosition{1, 2, 3}) == 99);
}

static void test_lattice_array_get_slice() {
    // 4x4 array with sequential values.
    std::vector<float> data(16);
    for (std::size_t i = 0; i < 16; ++i) data[i] = static_cast<float>(i);
    LatticeArray<float> arr(IPosition{4, 4}, std::move(data));

    // Extract 2x2 sub-block starting at (1,1).
    auto sub = arr.get_slice(Slicer(IPosition{1, 1}, IPosition{2, 2}));
    CHECK(sub.shape() == (IPosition{2, 2}));
    // Original layout (4x4 Fortran):
    //   col0: 0,1,2,3  col1: 4,5,6,7  col2: 8,9,10,11  col3: 12,13,14,15
    // Sub at (1,1) size (2,2):
    //   (1,1)=5, (2,1)=6, (1,2)=9, (2,2)=10
    CHECK(sub.at(IPosition{0, 0}) == 5.0f);
    CHECK(sub.at(IPosition{1, 0}) == 6.0f);
    CHECK(sub.at(IPosition{0, 1}) == 9.0f);
    CHECK(sub.at(IPosition{1, 1}) == 10.0f);
}

static void test_lattice_array_get_slice_strided() {
    // 6x6 array.
    std::vector<int> data(36);
    for (std::size_t i = 0; i < 36; ++i) data[i] = static_cast<int>(i);
    LatticeArray<int> arr(IPosition{6, 6}, std::move(data));

    // Extract every other element: start (0,0), length (3,3), stride (2,2).
    auto sub = arr.get_slice(Slicer(IPosition{0, 0}, IPosition{3, 3}, IPosition{2, 2}));
    CHECK(sub.shape() == (IPosition{3, 3}));
    // (0,0)=0, (2,0)=2, (4,0)=4, (0,2)=12, (2,2)=14, (4,2)=16, etc.
    CHECK(sub.at(IPosition{0, 0}) == 0);
    CHECK(sub.at(IPosition{1, 0}) == 2);
    CHECK(sub.at(IPosition{2, 0}) == 4);
    CHECK(sub.at(IPosition{0, 1}) == 12);
}

static void test_lattice_array_put_slice() {
    LatticeArray<int> arr(IPosition{4, 4}, 0);
    LatticeArray<int> patch(IPosition{2, 2}, 99);
    arr.put_slice(patch, Slicer(IPosition{1, 1}, IPosition{2, 2}));
    CHECK(arr.at(IPosition{0, 0}) == 0);
    CHECK(arr.at(IPosition{1, 1}) == 99);
    CHECK(arr.at(IPosition{2, 1}) == 99);
    CHECK(arr.at(IPosition{1, 2}) == 99);
    CHECK(arr.at(IPosition{2, 2}) == 99);
    CHECK(arr.at(IPosition{3, 3}) == 0);
}

static void test_lattice_array_rank_mismatch_view() {
    LatticeArray<float> arr(IPosition{3, 4});
    bool caught = false;
    try {
        auto v = arr.const_view<3>();  // Wrong rank.
        (void)v;
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    CHECK(caught);
}

// ── Main ───────────────────────────────────────────────────────────────

int main() {
    test_iposition_basic();
    test_iposition_equality();
    test_iposition_unsigned_roundtrip();
    test_iposition_to_string();

    test_slicer_full();
    test_slicer_custom();
    test_slicer_rank_mismatch();
    test_slicer_bad_stride();

    test_fortran_strides();
    test_linear_index_roundtrip();
    test_delinearize_zero();
    test_validate_index_ok();
    test_validate_index_out_of_range();
    test_validate_slicer_ok();
    test_validate_slicer_overflow();

    test_lattice_array_construction();
    test_lattice_array_from_data();
    test_lattice_array_bad_data_size();
    test_lattice_array_shared_storage();
    test_lattice_array_at_and_put();
    test_lattice_array_mdspan_view();
    test_lattice_array_mdspan_mutable_view();
    test_lattice_array_3d_mdspan();
    test_lattice_array_get_slice();
    test_lattice_array_get_slice_strided();
    test_lattice_array_put_slice();
    test_lattice_array_rank_mismatch_view();

    std::cout << "lattice_shape_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
