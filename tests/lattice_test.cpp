#include "casacore_mini/lattice.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>

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

// ── ArrayLattice tests ─────────────────────────────────────────────────

static void test_array_lattice_basic() {
    ArrayLattice<float> lat(IPosition{4, 5});
    CHECK(lat.ndim() == 2);
    CHECK(lat.nelements() == 20);
    CHECK(lat.is_writable());
    CHECK(!lat.is_paged());
    CHECK(!lat.has_pixel_mask());

    // All zeros initially.
    CHECK(lat.get_at(IPosition{0, 0}) == 0.0f);

    lat.put_at(42.0f, IPosition{1, 2});
    CHECK(lat.get_at(IPosition{1, 2}) == 42.0f);
}

static void test_array_lattice_from_data() {
    LatticeArray<int> arr(IPosition{3, 3}, 7);
    ArrayLattice<int> lat(std::move(arr));
    CHECK(lat.get_at(IPosition{0, 0}) == 7);
    CHECK(lat.get_at(IPosition{2, 2}) == 7);
}

static void test_array_lattice_put_get() {
    ArrayLattice<double> lat(IPosition{3, 4}, 1.0);
    auto data = lat.get();
    CHECK(data.nelements() == 12);
    CHECK(data.at(IPosition{0, 0}) == 1.0);

    // Modify and put back.
    data.make_unique();
    data.put(IPosition{2, 3}, 99.0);
    lat.put(data);
    CHECK(lat.get_at(IPosition{2, 3}) == 99.0);
}

static void test_array_lattice_slice() {
    ArrayLattice<float> lat(IPosition{6, 6});
    // Fill with sequential values.
    for (std::int64_t j = 0; j < 6; ++j) {
        for (std::int64_t i = 0; i < 6; ++i) {
            lat.put_at(static_cast<float>(i + j * 6), IPosition{i, j});
        }
    }

    auto slice = lat.get_slice(Slicer(IPosition{1, 1}, IPosition{2, 3}));
    CHECK(slice.shape() == (IPosition{2, 3}));
    CHECK(slice.at(IPosition{0, 0}) == 7.0f);  // (1,1) in original
    CHECK(slice.at(IPosition{1, 0}) == 8.0f);  // (2,1)
    CHECK(slice.at(IPosition{0, 2}) == 19.0f); // (1,3)
}

static void test_array_lattice_put_slice() {
    ArrayLattice<int> lat(IPosition{5, 5}, 0);
    LatticeArray<int> patch(IPosition{2, 2}, 99);
    lat.put_slice(patch, Slicer(IPosition{1, 1}, IPosition{2, 2}));
    CHECK(lat.get_at(IPosition{0, 0}) == 0);
    CHECK(lat.get_at(IPosition{1, 1}) == 99);
    CHECK(lat.get_at(IPosition{2, 2}) == 99);
}

static void test_array_lattice_set() {
    ArrayLattice<float> lat(IPosition{3, 3}, 0.0f);
    lat.set(5.0f);
    CHECK(lat.get_at(IPosition{0, 0}) == 5.0f);
    CHECK(lat.get_at(IPosition{2, 2}) == 5.0f);
}

static void test_array_lattice_apply() {
    ArrayLattice<float> lat(IPosition{2, 2}, 3.0f);
    lat.apply([](float x) { return x * 2.0f; });
    CHECK(lat.get_at(IPosition{0, 0}) == 6.0f);
    CHECK(lat.get_at(IPosition{1, 1}) == 6.0f);
}

static void test_array_lattice_mask() {
    ArrayLattice<float> lat(IPosition{3, 3});
    CHECK(!lat.is_masked());
    auto mask = lat.get_mask();
    CHECK(mask.shape() == (IPosition{3, 3}));
    // All true (no masking).
    CHECK(mask.at(IPosition{0, 0}) == true);
}

// ── SubLattice tests ──────────────────────────────────────────────────

static void test_sublattice_read() {
    ArrayLattice<float> parent(IPosition{10, 10});
    for (std::int64_t j = 0; j < 10; ++j) {
        for (std::int64_t i = 0; i < 10; ++i) {
            parent.put_at(static_cast<float>(i * 10 + j), IPosition{i, j});
        }
    }

    SubLattice<float> sub(parent, Slicer(IPosition{2, 3}, IPosition{4, 5}));
    CHECK(sub.shape() == (IPosition{4, 5}));
    CHECK(sub.is_writable());
    // sub(0,0) -> parent(2,3) -> 2*10+3 = 23
    CHECK(sub.get_at(IPosition{0, 0}) == 23.0f);
    // sub(3,4) -> parent(5,7) -> 5*10+7 = 57
    CHECK(sub.get_at(IPosition{3, 4}) == 57.0f);
}

static void test_sublattice_write() {
    ArrayLattice<int> parent(IPosition{8, 8}, 0);
    SubLattice<int> sub(parent, Slicer(IPosition{2, 2}, IPosition{3, 3}));
    sub.put_at(42, IPosition{1, 1});
    // Should have written to parent(3,3).
    CHECK(parent.get_at(IPosition{3, 3}) == 42);
}

static void test_sublattice_readonly() {
    ArrayLattice<float> parent(IPosition{4, 4});
    const auto& cref = parent;
    SubLattice<float> sub(cref, Slicer(IPosition{0, 0}, IPosition{2, 2}));
    CHECK(!sub.is_writable());

    bool caught = false;
    try {
        sub.put_at(1.0f, IPosition{0, 0});
    } catch (const std::runtime_error&) {
        caught = true;
    }
    CHECK(caught);
}

// ── TempLattice tests ─────────────────────────────────────────────────

static void test_temp_lattice_memory() {
    // Small enough to stay in memory.
    TempLattice<float> lat(IPosition{4, 4}, 1024 * 1024);
    CHECK(!lat.is_paged());
    CHECK(lat.is_writable());

    lat.put_at(7.0f, IPosition{1, 2});
    CHECK(lat.get_at(IPosition{1, 2}) == 7.0f);
}

// ── LatticeIterator tests ─────────────────────────────────────────────

static void test_iterator_basic() {
    ArrayLattice<float> lat(IPosition{6, 4});
    for (std::int64_t j = 0; j < 4; ++j) {
        for (std::int64_t i = 0; i < 6; ++i) {
            lat.put_at(static_cast<float>(i + j * 6), IPosition{i, j});
        }
    }

    // Iterate with 3x2 cursor.
    LatticeIterator<float> iter(lat, IPosition{3, 2});
    int chunk_count = 0;
    while (!iter.at_end()) {
        auto chunk = iter.cursor();
        auto cs = iter.cursor_shape();
        // All chunks should be 3x2 for a 6x4 lattice with 3x2 cursor.
        CHECK(cs == (IPosition{3, 2}));
        CHECK(chunk.shape() == cs);
        ++chunk_count;
        ++iter;
    }
    // 6/3 * 4/2 = 2 * 2 = 4 chunks.
    CHECK(chunk_count == 4);
}

static void test_iterator_edge_chunks() {
    ArrayLattice<int> lat(IPosition{5, 3}, 1);

    // Iterate with 3x2 cursor over 5x3 → edge chunks will be smaller.
    LatticeIterator<int> iter(lat, IPosition{3, 2});
    int chunk_count = 0;
    while (!iter.at_end()) {
        auto cs = iter.cursor_shape();
        // Check edge handling.
        if (iter.position()[0] == 3) CHECK(cs[0] == 2); // Last column: 5-3=2
        if (iter.position()[1] == 2) CHECK(cs[1] == 1); // Last row: 3-2=1
        ++chunk_count;
        ++iter;
    }
    // ceil(5/3) * ceil(3/2) = 2 * 2 = 4 chunks.
    CHECK(chunk_count == 4);
}

static void test_iterator_write() {
    ArrayLattice<float> lat(IPosition{4, 4}, 0.0f);
    LatticeIterator<float> iter(lat, IPosition{2, 2});
    float val = 1.0f;
    while (!iter.at_end()) {
        auto cs = iter.cursor_shape();
        LatticeArray<float> chunk(cs, val);
        iter.write_cursor(chunk);
        val += 1.0f;
        ++iter;
    }
    // First chunk (0:1, 0:1) should be 1.0.
    CHECK(lat.get_at(IPosition{0, 0}) == 1.0f);
    CHECK(lat.get_at(IPosition{1, 1}) == 1.0f);
    // Second chunk (2:3, 0:1) should be 2.0.
    CHECK(lat.get_at(IPosition{2, 0}) == 2.0f);
    // Third chunk (0:1, 2:3) should be 3.0.
    CHECK(lat.get_at(IPosition{0, 2}) == 3.0f);
    // Fourth chunk (2:3, 2:3) should be 4.0.
    CHECK(lat.get_at(IPosition{2, 2}) == 4.0f);
}

static void test_iterator_reset() {
    ArrayLattice<int> lat(IPosition{4, 4}, 1);
    LatticeIterator<int> iter(lat, IPosition{4, 4});
    CHECK(!iter.at_end());
    ++iter;
    CHECK(iter.at_end());
    iter.reset();
    CHECK(!iter.at_end());
}

// ── Main ───────────────────────────────────────────────────────────────

int main() {
    test_array_lattice_basic();
    test_array_lattice_from_data();
    test_array_lattice_put_get();
    test_array_lattice_slice();
    test_array_lattice_put_slice();
    test_array_lattice_set();
    test_array_lattice_apply();
    test_array_lattice_mask();

    test_sublattice_read();
    test_sublattice_write();
    test_sublattice_readonly();

    test_temp_lattice_memory();

    test_iterator_basic();
    test_iterator_edge_chunks();
    test_iterator_write();
    test_iterator_reset();

    std::cout << "lattice_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
