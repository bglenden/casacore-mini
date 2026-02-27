#include "casacore_mini/lattice_expr.hpp"

#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>

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

#define CHECK_NEAR(a, b, tol)                                                  \
    CHECK(std::abs(static_cast<double>(a) - static_cast<double>(b)) < (tol))

#define CHECK_THROWS(expr)                                                     \
    do {                                                                       \
        bool threw = false;                                                    \
        try { (void)(expr); } catch (...) { threw = true; }                    \
        CHECK(threw);                                                          \
    } while (false)

#define CHECK_THROWS_AS(expr, ExType)                                          \
    do {                                                                       \
        bool threw = false;                                                    \
        try { (void)(expr); } catch (const ExType&) { threw = true; }          \
        CHECK(threw);                                                          \
    } while (false)

// ── Helpers ──────────────────────────────────────────────────────────

static LatticeArray<float> make_8x8() {
    IPosition sh{8, 8};
    LatticeArray<float> arr(sh, 0.0f);
    arr.make_unique();
    for (std::int64_t j = 0; j < 8; ++j) {
        for (std::int64_t i = 0; i < 8; ++i) {
            arr.put(IPosition{i, j},
                    static_cast<float>(i + j * 8 + 1)); // 1..64
        }
    }
    return arr;
}

static LatticeArray<float> make_8x8_alt() {
    IPosition sh{8, 8};
    LatticeArray<float> arr(sh, 0.0f);
    arr.make_unique();
    for (std::int64_t j = 0; j < 8; ++j) {
        for (std::int64_t i = 0; i < 8; ++i) {
            arr.put(IPosition{i, j},
                    static_cast<float>((i + j * 8 + 1) * 3)); // 3,6,...,192
        }
    }
    return arr;
}

static LatticeArray<float> make_16x16_partial_mask(
    LatticeArray<bool>& out_mask) {
    IPosition sh{16, 16};
    LatticeArray<float> arr(sh, 0.0f);
    arr.make_unique();
    out_mask = LatticeArray<bool>(sh, true);
    out_mask.make_unique();
    for (std::int64_t j = 0; j < 16; ++j) {
        for (std::int64_t i = 0; i < 16; ++i) {
            float val = static_cast<float>(i + j * 16 + 1);
            arr.put(IPosition{i, j}, val);
            // Mask out every 4th element.
            if ((i + j * 16) % 4 == 0) {
                out_mask.put(IPosition{i, j}, false);
            }
        }
    }
    return arr;
}

// ── Category 1: Scalar/lattice arithmetic ────────────────────────────

static void test_cat1_lattice_lattice_arith() {
    auto a = lel_array(make_8x8());
    auto b = lel_array(make_8x8_alt());

    // +
    auto r_add = lel_add(a, b)->eval();
    CHECK_NEAR(r_add.values.flat()[0], 4.0f, 1e-6);  // 1+3
    CHECK_NEAR(r_add.values.flat()[63], 256.0f, 1e-6); // 64+192

    // -
    auto r_sub = lel_sub(b, a)->eval();
    CHECK_NEAR(r_sub.values.flat()[0], 2.0f, 1e-6);  // 3-1

    // *
    auto r_mul = lel_mul(a, b)->eval();
    CHECK_NEAR(r_mul.values.flat()[0], 3.0f, 1e-6);  // 1*3

    // /
    auto r_div = lel_div(b, a)->eval();
    CHECK_NEAR(r_div.values.flat()[0], 3.0f, 1e-6);  // 3/1
    CHECK_NEAR(r_div.values.flat()[1], 3.0f, 1e-6);  // 6/2
}

static void test_cat1_scalar_lattice_arith() {
    auto a = lel_array(make_8x8());
    auto s = lel_scalar(10.0f);

    // scalar + lattice
    auto r1 = lel_add(s, a)->eval();
    CHECK_NEAR(r1.values.flat()[0], 11.0f, 1e-6);

    // lattice - scalar
    auto r2 = lel_sub(a, s)->eval();
    CHECK_NEAR(r2.values.flat()[0], -9.0f, 1e-6);

    // scalar * lattice
    auto r3 = lel_mul(lel_scalar(0.5f), a)->eval();
    CHECK_NEAR(r3.values.flat()[0], 0.5f, 1e-6);

    // lattice / scalar
    auto r4 = lel_div(a, lel_scalar(2.0f))->eval();
    CHECK_NEAR(r4.values.flat()[0], 0.5f, 1e-6);
}

static void test_cat1_unary_plus_minus() {
    auto a = lel_array(make_8x8());
    auto neg = lel_negate(a)->eval();
    CHECK_NEAR(neg.values.flat()[0], -1.0f, 1e-6);
    CHECK_NEAR(neg.values.flat()[63], -64.0f, 1e-6);
}

static void test_cat1_div_by_zero_ieee() {
    auto a = lel_array(make_8x8());
    auto z = lel_scalar(0.0f);
    auto r = lel_div(a, z)->eval();
    CHECK(std::isinf(r.values.flat()[0]));

    // 0/0 → NaN
    auto z2 = lel_scalar(0.0f);
    auto r2 = lel_div(z, z2)->eval();
    CHECK(std::isnan(r2.values.flat()[0]));
}

static void test_cat1_special_scalars() {
    auto a = lel_array(make_8x8());
    // 0.0
    auto r0 = lel_mul(a, lel_scalar(0.0f))->eval();
    CHECK_NEAR(r0.values.flat()[0], 0.0f, 1e-6);
    // 1.0
    auto r1 = lel_mul(a, lel_scalar(1.0f))->eval();
    CHECK_NEAR(r1.values.flat()[0], 1.0f, 1e-6);
    // -2.5
    auto rn = lel_mul(a, lel_scalar(-2.5f))->eval();
    CHECK_NEAR(rn.values.flat()[0], -2.5f, 1e-6);
}

// ── Category 2: Comparison and logical operators ─────────────────────

static void test_cat2_comparison_ops() {
    auto a = lel_array(make_8x8());
    auto s = lel_scalar(32.0f);

    // ==
    auto eq = lel_compare(LelBinaryOp::eq, a, s)->eval();
    CHECK(eq.values.flat()[31] == true);  // 32 == 32
    CHECK(eq.values.flat()[0] == false);

    // !=
    auto ne = lel_compare(LelBinaryOp::ne, a, s)->eval();
    CHECK(ne.values.flat()[31] == false);
    CHECK(ne.values.flat()[0] == true);

    // >
    auto gt = lel_compare(LelBinaryOp::gt, a, s)->eval();
    CHECK(gt.values.flat()[32] == true);  // 33 > 32
    CHECK(gt.values.flat()[31] == false);

    // >=
    auto ge = lel_compare(LelBinaryOp::ge, a, s)->eval();
    CHECK(ge.values.flat()[31] == true);  // 32 >= 32

    // <
    auto lt = lel_compare(LelBinaryOp::lt, a, s)->eval();
    CHECK(lt.values.flat()[0] == true);   // 1 < 32
    CHECK(lt.values.flat()[31] == false);

    // <=
    auto le = lel_compare(LelBinaryOp::le, a, s)->eval();
    CHECK(le.values.flat()[31] == true);
    CHECK(le.values.flat()[32] == false);
}

static void test_cat2_logical_ops() {
    auto a = lel_array(make_8x8());
    auto gt10 = lel_compare(LelBinaryOp::gt, a, lel_scalar(10.0f));
    auto lt50 = lel_compare(LelBinaryOp::lt, a, lel_scalar(50.0f));

    // && : true for 11..49
    auto and_r = lel_and(gt10, lt50)->eval();
    CHECK(and_r.values.flat()[10] == true);  // 11
    CHECK(and_r.values.flat()[48] == true);  // 49
    CHECK(and_r.values.flat()[0] == false);  // 1
    CHECK(and_r.values.flat()[49] == false); // 50

    // ||
    auto or_r = lel_or(gt10, lt50)->eval();
    CHECK(or_r.values.flat()[0] == true);   // 1 < 50
    CHECK(or_r.values.flat()[63] == true);  // 64 > 10

    // !
    auto not_r = lel_not(gt10)->eval();
    CHECK(not_r.values.flat()[0] == true);
    CHECK(not_r.values.flat()[10] == false);
}

// ── Category 3: Mask-aware boolean evaluation ────────────────────────

static void test_cat3_iif_with_mask() {
    LatticeArray<bool> mask;
    auto data = make_16x16_partial_mask(mask);
    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));

    auto cond = lel_compare(LelBinaryOp::gt, node, lel_scalar(128.0f));
    auto true_val = lel_scalar(1.0f);
    auto false_val = lel_scalar(0.0f);
    auto iif_expr = lel_iif(cond, true_val, false_val);
    auto r = iif_expr->eval();
    // Values > 128 get 1.0, <= 128 get 0.0.
    CHECK_NEAR(r.values.flat()[0], 0.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[255], 1.0f, 1e-6); // 256 > 128
    // Mask should propagate from data node.
    CHECK(r.mask.has_value());
    CHECK(r.mask->flat()[0] == false); // masked
}

static void test_cat3_value_extract() {
    IPosition sh{4, 4};
    LatticeArray<float> data(sh, 5.0f);
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    mask.put(IPosition{0, 0}, false);

    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));
    auto val = lel_value(node);
    auto r = val->eval();
    CHECK_NEAR(r.values.flat()[0], 5.0f, 1e-6);
    CHECK(!r.mask.has_value()); // mask stripped
}

static void test_cat3_mask_extract() {
    IPosition sh{4, 4};
    LatticeArray<float> data(sh, 1.0f);
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    mask.put(IPosition{0, 0}, false);
    mask.put(IPosition{1, 0}, false);

    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));
    auto m = lel_mask(node);
    auto r = m->eval();
    CHECK(r.values.flat()[0] == false);
    CHECK(r.values.flat()[1] == false);
    CHECK(r.values.flat()[2] == true);

    // No mask → all true
    auto node2 = lel_array(LatticeArray<float>(sh, 1.0f));
    auto m2 = lel_mask(node2);
    auto r2 = m2->eval();
    CHECK(r2.values.flat()[0] == true);
}

// ── Category 4: Reductions ───────────────────────────────────────────

static void test_cat4_reductions_with_mask() {
    LatticeArray<bool> mask;
    auto data = make_16x16_partial_mask(mask);
    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));

    // Count unmasked elements.
    std::size_t n_unmasked = 0;
    float total = 0.0f;
    for (std::size_t i = 0; i < 256; ++i) {
        if (mask.flat()[i]) {
            total += data.flat()[i];
            ++n_unmasked;
        }
    }
    // 256 total, every 4th masked → 192 unmasked
    CHECK(n_unmasked == 192);

    auto sum_r = lel_reduce(LelFunc::sum_r, node)->eval();
    CHECK_NEAR(sum_r.values.flat()[0], total, 1.0);

    auto mean_r = lel_reduce(LelFunc::mean_r, node)->eval();
    CHECK_NEAR(mean_r.values.flat()[0], total / 192.0f, 0.1);

    auto min_r = lel_reduce(LelFunc::min_r, node)->eval();
    // Minimum unmasked value: element 1 (index 0 is masked, index 1 = value 2)
    CHECK_NEAR(min_r.values.flat()[0], 2.0f, 1e-6);

    auto max_r = lel_reduce(LelFunc::max_r, node)->eval();
    // Max unmasked value: 256 is at index 255, which is (255%4=3)≠0, so unmasked
    CHECK_NEAR(max_r.values.flat()[0], 256.0f, 1e-6);
}

static void test_cat4_bool_reductions() {
    auto a = lel_array(make_8x8());
    auto gt32 = lel_compare(LelBinaryOp::gt, a, lel_scalar(32.0f));

    auto ntrue = lel_bool_reduce<float>(LelFunc::ntrue_r, gt32)->eval();
    CHECK_NEAR(ntrue.values.flat()[0], 32.0f, 1e-6); // 33..64

    auto nfalse = lel_bool_reduce<float>(LelFunc::nfalse_r, gt32)->eval();
    CHECK_NEAR(nfalse.values.flat()[0], 32.0f, 1e-6);

    auto any_r = lel_bool_reduce<float>(LelFunc::any_r, gt32)->eval();
    CHECK_NEAR(any_r.values.flat()[0], 1.0f, 1e-6);

    auto all_false = lel_compare(LelBinaryOp::gt, a, lel_scalar(100.0f));
    auto any_f = lel_bool_reduce<float>(LelFunc::any_r, all_false)->eval();
    CHECK_NEAR(any_f.values.flat()[0], 0.0f, 1e-6);

    auto all_true = lel_compare(LelBinaryOp::gt, a, lel_scalar(0.0f));
    auto all_r = lel_bool_reduce<float>(LelFunc::all_r, all_true)->eval();
    CHECK_NEAR(all_r.values.flat()[0], 1.0f, 1e-6);
}

static void test_cat4_variance_stddev() {
    auto a = lel_array(make_8x8());
    auto var = lel_reduce(LelFunc::variance_r, a)->eval();
    auto sd = lel_reduce(LelFunc::stddev_r, a)->eval();
    // variance of 1..64 (sample) = 346.6667
    CHECK_NEAR(var.values.flat()[0], 346.667f, 0.1);
    CHECK_NEAR(sd.values.flat()[0], 18.619, 0.1);
}

// ── Category 5: Mathematical functions ───────────────────────────────

static void test_cat5_trig() {
    LatticeArray<float> data(IPosition{4}, 0.0f);
    data.make_unique();
    data.put(IPosition{0}, 0.0f);
    data.put(IPosition{1}, static_cast<float>(M_PI / 6));
    data.put(IPosition{2}, static_cast<float>(M_PI / 4));
    data.put(IPosition{3}, static_cast<float>(M_PI / 2));
    auto a = lel_array(data);

    auto sin_r = lel_math1(LelFunc::sin_f, a)->eval();
    CHECK_NEAR(sin_r.values.flat()[0], 0.0f, 1e-5);
    CHECK_NEAR(sin_r.values.flat()[1], 0.5f, 1e-5);
    CHECK_NEAR(sin_r.values.flat()[3], 1.0f, 1e-5);

    auto cos_r = lel_math1(LelFunc::cos_f, a)->eval();
    CHECK_NEAR(cos_r.values.flat()[0], 1.0f, 1e-5);
    CHECK_NEAR(cos_r.values.flat()[3], 0.0f, 1e-4);

    auto tan_r = lel_math1(LelFunc::tan_f, a)->eval();
    CHECK_NEAR(tan_r.values.flat()[0], 0.0f, 1e-5);
    CHECK_NEAR(tan_r.values.flat()[2], 1.0f, 1e-4); // tan(π/4) ≈ 1
}

static void test_cat5_inverse_trig() {
    LatticeArray<float> data(IPosition{3}, 0.0f);
    data.make_unique();
    data.put(IPosition{0}, 0.0f);
    data.put(IPosition{1}, 0.5f);
    data.put(IPosition{2}, 1.0f);
    auto a = lel_array(data);

    auto asin_r = lel_math1(LelFunc::asin_f, a)->eval();
    CHECK_NEAR(asin_r.values.flat()[0], 0.0f, 1e-5);
    CHECK_NEAR(asin_r.values.flat()[1], M_PI / 6, 1e-4);

    auto acos_r = lel_math1(LelFunc::acos_f, a)->eval();
    CHECK_NEAR(acos_r.values.flat()[2], 0.0f, 1e-5); // acos(1) = 0

    auto atan_r = lel_math1(LelFunc::atan_f, a)->eval();
    CHECK_NEAR(atan_r.values.flat()[2], M_PI / 4, 1e-4);
}

static void test_cat5_exp_log() {
    LatticeArray<float> data(IPosition{3}, 0.0f);
    data.make_unique();
    data.put(IPosition{0}, 0.0f);
    data.put(IPosition{1}, 1.0f);
    data.put(IPosition{2}, 2.0f);
    auto a = lel_array(data);

    auto exp_r = lel_math1(LelFunc::exp_f, a)->eval();
    CHECK_NEAR(exp_r.values.flat()[0], 1.0f, 1e-5);
    CHECK_NEAR(exp_r.values.flat()[1], std::exp(1.0f), 1e-4);

    auto log_r = lel_math1(LelFunc::log_f, lel_array(exp_r.values))->eval();
    CHECK_NEAR(log_r.values.flat()[0], 0.0f, 1e-5);
    CHECK_NEAR(log_r.values.flat()[1], 1.0f, 1e-4);

    // log of zero → -inf
    auto log0 = lel_math1(LelFunc::log_f, lel_scalar(0.0f))->eval();
    CHECK(std::isinf(log0.values.flat()[0]));

    // sqrt of negative → NaN
    auto sqrt_neg = lel_math1(LelFunc::sqrt_f, lel_scalar(-1.0f))->eval();
    CHECK(std::isnan(sqrt_neg.values.flat()[0]));
}

static void test_cat5_round_ceil_floor_sign() {
    LatticeArray<float> data(IPosition{4}, 0.0f);
    data.make_unique();
    data.put(IPosition{0}, 2.3f);
    data.put(IPosition{1}, -2.7f);
    data.put(IPosition{2}, 0.0f);
    data.put(IPosition{3}, 3.5f);
    auto a = lel_array(data);

    auto round_r = lel_math1(LelFunc::round_f, a)->eval();
    CHECK_NEAR(round_r.values.flat()[0], 2.0f, 1e-6);
    CHECK_NEAR(round_r.values.flat()[1], -3.0f, 1e-6);

    auto ceil_r = lel_math1(LelFunc::ceil_f, a)->eval();
    CHECK_NEAR(ceil_r.values.flat()[0], 3.0f, 1e-6);
    CHECK_NEAR(ceil_r.values.flat()[1], -2.0f, 1e-6);

    auto floor_r = lel_math1(LelFunc::floor_f, a)->eval();
    CHECK_NEAR(floor_r.values.flat()[0], 2.0f, 1e-6);
    CHECK_NEAR(floor_r.values.flat()[1], -3.0f, 1e-6);

    auto sign_r = lel_math1(LelFunc::sign_f, a)->eval();
    CHECK_NEAR(sign_r.values.flat()[0], 1.0f, 1e-6);
    CHECK_NEAR(sign_r.values.flat()[1], -1.0f, 1e-6);
    CHECK_NEAR(sign_r.values.flat()[2], 0.0f, 1e-6);
}

static void test_cat5_pow_fmod_atan2() {
    auto a = lel_array(make_8x8());
    auto s = lel_scalar(2.0f);
    auto pow_r = lel_math2(LelFunc::pow_f, a, s)->eval();
    CHECK_NEAR(pow_r.values.flat()[0], 1.0f, 1e-6);   // 1^2
    CHECK_NEAR(pow_r.values.flat()[2], 9.0f, 1e-6);    // 3^2

    LatticeArray<float> d1(IPosition{2}, 0.0f);
    d1.make_unique();
    d1.put(IPosition{0}, 7.0f);
    d1.put(IPosition{1}, 10.0f);
    LatticeArray<float> d2(IPosition{2}, 0.0f);
    d2.make_unique();
    d2.put(IPosition{0}, 3.0f);
    d2.put(IPosition{1}, 3.0f);

    auto fmod_r = lel_math2(LelFunc::fmod_f, lel_array(d1), lel_array(d2))->eval();
    CHECK_NEAR(fmod_r.values.flat()[0], 1.0f, 1e-6);  // 7 % 3
    CHECK_NEAR(fmod_r.values.flat()[1], 1.0f, 1e-6);  // 10 % 3

    auto atan2_r = lel_math2(LelFunc::atan2_f,
                             lel_scalar(1.0f), lel_scalar(1.0f))->eval();
    CHECK_NEAR(atan2_r.values.flat()[0], M_PI / 4, 1e-4);
}

static void test_cat5_min2_max2() {
    auto a = lel_array(make_8x8());
    auto b = lel_array(make_8x8_alt());
    auto mn = lel_math2(LelFunc::min2_f, a, b)->eval();
    auto mx = lel_math2(LelFunc::max2_f, a, b)->eval();
    CHECK_NEAR(mn.values.flat()[0], 1.0f, 1e-6);  // min(1,3)
    CHECK_NEAR(mx.values.flat()[0], 3.0f, 1e-6);  // max(1,3)
}

static void test_cat5_mask_through_function() {
    IPosition sh{4};
    LatticeArray<float> data(sh, 4.0f);
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    mask.put(IPosition{0}, false);

    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));
    auto sqrt_r = lel_math1(LelFunc::sqrt_f, node);
    auto r = sqrt_r->eval();
    CHECK_NEAR(r.values.flat()[1], 2.0f, 1e-6);
    CHECK(r.mask.has_value());
    CHECK(r.mask->flat()[0] == false); // mask propagated
    CHECK(r.mask->flat()[1] == true);
}

// ── Category 6: Scalar-lattice broadcasting ──────────────────────────

static void test_cat6_broadcast_patterns() {
    auto lat = lel_array(make_8x8());
    // 2.0 * lat + 1.0
    auto expr = lel_add(lel_mul(lel_scalar(2.0f), lat), lel_scalar(1.0f));
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 3.0f, 1e-6);   // 2*1+1
    CHECK_NEAR(r.values.flat()[63], 129.0f, 1e-6); // 2*64+1
}

static void test_cat6_shape_mismatch_error() {
    LatticeArray<float> a(IPosition{8, 8}, 1.0f);
    LatticeArray<float> b(IPosition{4, 4}, 2.0f);
    auto expr = lel_add(lel_array(a), lel_array(b));
    CHECK_THROWS(expr->eval());
}

static void test_cat6_nested_broadcast() {
    auto lat = lel_array(make_8x8());
    auto s1 = lel_scalar(2.0f);
    auto s2 = lel_scalar(3.0f);
    // (lat * s1) + (lat * s2)  → lat * 5
    auto expr = lel_add(lel_mul(lat, s1), lel_mul(lat, s2));
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 5.0f, 1e-6);    // 1*5
    CHECK_NEAR(r.values.flat()[63], 320.0f, 1e-6);  // 64*5
}

// ── Category 7: Complex number operations ────────────────────────────

static void test_cat7_form_complex() {
    LatticeArray<float> re(IPosition{4}, 0.0f);
    re.make_unique();
    re.put(IPosition{0}, 1.0f);
    re.put(IPosition{1}, 2.0f);
    re.put(IPosition{2}, 3.0f);
    re.put(IPosition{3}, 4.0f);

    LatticeArray<float> im(IPosition{4}, 0.0f);
    im.make_unique();
    im.put(IPosition{0}, 5.0f);
    im.put(IPosition{1}, 6.0f);
    im.put(IPosition{2}, 7.0f);
    im.put(IPosition{3}, 8.0f);

    auto c = lel_form_complex<float>(lel_array(re), lel_array(im));
    auto r = c->eval();
    CHECK_NEAR(r.values.flat()[0].real(), 1.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[0].imag(), 5.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[3].real(), 4.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[3].imag(), 8.0f, 1e-6);
}

static void test_cat7_real_imag() {
    LatticeArray<std::complex<float>> data(IPosition{4}, std::complex<float>{});
    data.make_unique();
    data.put(IPosition{0}, std::complex<float>(1.0f, 2.0f));
    data.put(IPosition{1}, std::complex<float>(3.0f, 4.0f));
    data.put(IPosition{2}, std::complex<float>(-1.0f, 0.5f));
    data.put(IPosition{3}, std::complex<float>(0.0f, -3.0f));

    auto node = lel_array(data);
    auto re = lel_real<float>(node)->eval();
    auto im = lel_imag<float>(node)->eval();

    CHECK_NEAR(re.values.flat()[0], 1.0f, 1e-6);
    CHECK_NEAR(re.values.flat()[1], 3.0f, 1e-6);
    CHECK_NEAR(im.values.flat()[0], 2.0f, 1e-6);
    CHECK_NEAR(im.values.flat()[1], 4.0f, 1e-6);
}

static void test_cat7_conj() {
    LatticeArray<std::complex<float>> data(IPosition{2}, std::complex<float>{});
    data.make_unique();
    data.put(IPosition{0}, std::complex<float>(3.0f, 4.0f));
    data.put(IPosition{1}, std::complex<float>(-1.0f, 2.0f));

    auto r = lel_conj<float>(lel_array(data))->eval();
    CHECK_NEAR(r.values.flat()[0].real(), 3.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[0].imag(), -4.0f, 1e-6);
    CHECK_NEAR(r.values.flat()[1].imag(), -2.0f, 1e-6);
}

static void test_cat7_abs_arg() {
    LatticeArray<std::complex<float>> data(IPosition{2}, std::complex<float>{});
    data.make_unique();
    data.put(IPosition{0}, std::complex<float>(3.0f, 4.0f));
    data.put(IPosition{1}, std::complex<float>(0.0f, 1.0f));

    auto abs_r = lel_complex_abs<float>(lel_array(data))->eval();
    CHECK_NEAR(abs_r.values.flat()[0], 5.0f, 1e-4); // |3+4i| = 5

    auto arg_r = lel_arg<float>(lel_array(data))->eval();
    CHECK_NEAR(arg_r.values.flat()[1], M_PI / 2, 1e-4); // arg(i) = π/2
}

static void test_cat7_complex_arithmetic() {
    using cf = std::complex<float>;
    LatticeArray<cf> a(IPosition{2}, cf{});
    a.make_unique();
    a.put(IPosition{0}, cf(1.0f, 2.0f));
    a.put(IPosition{1}, cf(3.0f, 4.0f));

    LatticeArray<cf> b(IPosition{2}, cf{});
    b.make_unique();
    b.put(IPosition{0}, cf(5.0f, 6.0f));
    b.put(IPosition{1}, cf(7.0f, 8.0f));

    auto add_r = lel_add(lel_array(a), lel_array(b))->eval();
    CHECK_NEAR(add_r.values.flat()[0].real(), 6.0f, 1e-6);
    CHECK_NEAR(add_r.values.flat()[0].imag(), 8.0f, 1e-6);

    auto mul_r = lel_mul(lel_array(a), lel_array(b))->eval();
    // (1+2i)*(5+6i) = 5+6i+10i+12i² = 5+16i-12 = -7+16i
    CHECK_NEAR(mul_r.values.flat()[0].real(), -7.0f, 1e-5);
    CHECK_NEAR(mul_r.values.flat()[0].imag(), 16.0f, 1e-5);
}

// ── Category 8: Malformed-expression diagnostics ─────────────────────

static void test_cat8_parse_errors() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    // Empty expression
    CHECK_THROWS_AS(lel_parse("", syms), LelParseError);

    // Unmatched parentheses
    CHECK_THROWS_AS(lel_parse("(lat + 1.0", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat + 1.0)", syms), LelParseError);

    // Missing operand
    CHECK_THROWS_AS(lel_parse("lat +", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("* lat", syms), LelParseError);

    // Unknown function
    CHECK_THROWS_AS(lel_parse("foobar(lat)", syms), LelParseError);

    // Undefined lattice reference
    CHECK_THROWS_AS(lel_parse("xyz + 1.0", syms), LelParseError);

    // Wrong argument count
    CHECK_THROWS_AS(lel_parse("sin(lat, lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("pow(lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("iif(T, 1.0)", syms), LelParseError);
}

static void test_cat8_type_errors() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    // Arithmetic on boolean
    auto bool_lat = lel_compare(LelBinaryOp::gt, lat, lel_scalar(0.0f));
    syms.define<bool>("bmask", bool_lat);
    CHECK_THROWS_AS(lel_parse("bmask + 1.0", syms), LelParseError);

    // Logical on float
    CHECK_THROWS_AS(lel_parse("lat && lat", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("!lat", syms), LelParseError);
}

static void test_cat8_shape_error_at_eval() {
    // Shape mismatch is detected at eval time (not parse time).
    LatticeArray<float> a(IPosition{4, 4}, 1.0f);
    LatticeArray<float> b(IPosition{3, 3}, 2.0f);
    auto expr = lel_add(lel_array(a), lel_array(b));
    CHECK_THROWS(expr->eval());
}

static void test_cat8_parse_error_position() {
    LelSymbolTable syms;
    try {
        (void)lel_parse("1.0 + @", syms);
        CHECK(false); // should have thrown
    } catch (const LelParseError& e) {
        CHECK(e.position() > 0); // error position is reported
    }
}

// ── Parser tests ─────────────────────────────────────────────────────

static void test_parser_arithmetic() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    // Simple scalar arithmetic
    auto r1 = lel_parse("2.0 + 3.0", syms);
    CHECK(r1.result_type() == LelType::float32);
    auto v1 = r1.as<float>()->eval();
    CHECK_NEAR(v1.values.flat()[0], 5.0f, 1e-6);

    // Scalar * lattice + scalar
    auto r2 = lel_parse("2.0 * lat + 1.0", syms);
    auto v2 = r2.as<float>()->eval();
    CHECK_NEAR(v2.values.flat()[0], 3.0f, 1e-6);   // 2*1+1
    CHECK_NEAR(v2.values.flat()[63], 129.0f, 1e-6); // 2*64+1

    // Operator precedence: a + b * c = a + (b*c)
    auto r3 = lel_parse("1.0 + 2.0 * 3.0", syms);
    auto v3 = r3.as<float>()->eval();
    CHECK_NEAR(v3.values.flat()[0], 7.0f, 1e-6);

    // Division
    auto r4 = lel_parse("lat / 2.0", syms);
    auto v4 = r4.as<float>()->eval();
    CHECK_NEAR(v4.values.flat()[0], 0.5f, 1e-6);

    // Parentheses
    auto r5 = lel_parse("(1.0 + 2.0) * 3.0", syms);
    auto v5 = r5.as<float>()->eval();
    CHECK_NEAR(v5.values.flat()[0], 9.0f, 1e-6);
}

static void test_parser_comparison_logical() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    // Comparison
    auto r1 = lel_parse("lat > 32.0", syms);
    CHECK(r1.result_type() == LelType::boolean);
    auto v1 = r1.as<bool>()->eval();
    CHECK(v1.values.flat()[32] == true);  // 33 > 32
    CHECK(v1.values.flat()[31] == false);

    // ==
    auto r2 = lel_parse("lat == 5.0", syms);
    auto v2 = r2.as<bool>()->eval();
    CHECK(v2.values.flat()[4] == true);

    // Logical operators
    auto r3 = lel_parse("lat > 10.0 && lat < 20.0", syms);
    auto v3 = r3.as<bool>()->eval();
    CHECK(v3.values.flat()[10] == true);  // 11
    CHECK(v3.values.flat()[19] == false); // 20
}

static void test_parser_functions() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    // sqrt
    auto r1 = lel_parse("sqrt(lat)", syms);
    auto v1 = r1.as<float>()->eval();
    CHECK_NEAR(v1.values.flat()[0], 1.0f, 1e-5);  // sqrt(1)
    CHECK_NEAR(v1.values.flat()[3], 2.0f, 1e-5);  // sqrt(4)

    // abs with negate
    auto r2 = lel_parse("abs(-1.0 * lat)", syms);
    auto v2 = r2.as<float>()->eval();
    CHECK_NEAR(v2.values.flat()[0], 1.0f, 1e-5);

    // pow
    auto r3 = lel_parse("pow(lat, 2.0)", syms);
    auto v3 = r3.as<float>()->eval();
    CHECK_NEAR(v3.values.flat()[0], 1.0f, 1e-5);
    CHECK_NEAR(v3.values.flat()[2], 9.0f, 1e-5);

    // Reductions
    auto r4 = lel_parse("sum(lat)", syms);
    auto v4 = r4.as<float>()->eval();
    CHECK_NEAR(v4.values.flat()[0], 2080.0f, 1e-2); // sum(1..64) = 2080

    auto r5 = lel_parse("min(lat)", syms);
    auto v5 = r5.as<float>()->eval();
    CHECK_NEAR(v5.values.flat()[0], 1.0f, 1e-6);

    auto r6 = lel_parse("max(lat)", syms);
    auto v6 = r6.as<float>()->eval();
    CHECK_NEAR(v6.values.flat()[0], 64.0f, 1e-6);

    // min/max element-wise 2-arg form
    auto r7 = lel_parse("min(lat, 10.0)", syms);
    auto v7 = r7.as<float>()->eval();
    CHECK_NEAR(v7.values.flat()[0], 1.0f, 1e-6);    // min(1,10)
    CHECK_NEAR(v7.values.flat()[20], 10.0f, 1e-6);   // min(21,10)

    // median reduction (contract coverage)
    auto r8 = lel_parse("median(lat)", syms);
    auto v8 = r8.as<float>()->eval();
    CHECK_NEAR(v8.values.flat()[0], 32.5f, 0.5);  // median(1..64)

    // mean reduction (contract coverage)
    auto r9 = lel_parse("mean(lat)", syms);
    auto v9 = r9.as<float>()->eval();
    CHECK_NEAR(v9.values.flat()[0], 32.5f, 0.1);  // mean(1..64) = 32.5

    // log10 function (contract coverage)
    auto r10 = lel_parse("log10(lat)", syms);
    auto v10 = r10.as<float>()->eval();
    CHECK_NEAR(v10.values.flat()[0], 0.0f, 1e-5);         // log10(1) = 0
    CHECK_NEAR(v10.values.flat()[9], 1.0f, 1e-5);         // log10(10) = 1
}

static void test_parser_iif() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    auto r = lel_parse("iif(lat > 32.0, 1.0, 0.0)", syms);
    auto v = r.as<float>()->eval();
    CHECK_NEAR(v.values.flat()[0], 0.0f, 1e-6);   // 1 <= 32
    CHECK_NEAR(v.values.flat()[32], 1.0f, 1e-6);   // 33 > 32
}

static void test_parser_bool_reductions() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    auto r1 = lel_parse("ntrue(lat > 32.0)", syms);
    auto v1 = r1.as<float>()->eval();
    CHECK_NEAR(v1.values.flat()[0], 32.0f, 1e-6);

    auto r2 = lel_parse("any(lat > 100.0)", syms);
    auto v2 = r2.as<float>()->eval();
    CHECK_NEAR(v2.values.flat()[0], 0.0f, 1e-6);

    auto r3 = lel_parse("all(lat > 0.0)", syms);
    auto v3 = r3.as<float>()->eval();
    CHECK_NEAR(v3.values.flat()[0], 1.0f, 1e-6);
}

static void test_parser_value_mask() {
    LelSymbolTable syms;
    IPosition sh{4, 4};
    LatticeArray<float> data(sh, 5.0f);
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    mask.put(IPosition{0, 0}, false);
    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));
    syms.define("lat", node);

    auto r1 = lel_parse("value(lat)", syms);
    auto v1 = r1.as<float>()->eval();
    CHECK(!v1.mask.has_value());

    auto r2 = lel_parse("mask(lat)", syms);
    auto v2 = r2.as<bool>()->eval();
    CHECK(v2.values.flat()[0] == false);
    CHECK(v2.values.flat()[1] == true);
}

static void test_parser_unary_minus() {
    LelSymbolTable syms;
    auto lat = lel_array(make_8x8());
    syms.define("lat", lat);

    auto r = lel_parse("-lat + 1.0", syms);
    auto v = r.as<float>()->eval();
    CHECK_NEAR(v.values.flat()[0], 0.0f, 1e-6);    // -1 + 1 = 0
    CHECK_NEAR(v.values.flat()[63], -63.0f, 1e-6);  // -64 + 1 = -63
}

static void test_parser_bool_literals() {
    LelSymbolTable syms;

    auto r1 = lel_parse("T", syms);
    CHECK(r1.result_type() == LelType::boolean);
    auto v1 = r1.as<bool>()->eval();
    CHECK(v1.values.flat()[0] == true);

    auto r2 = lel_parse("F", syms);
    auto v2 = r2.as<bool>()->eval();
    CHECK(v2.values.flat()[0] == false);
}

// ── Float→Double mixed promotion ─────────────────────────────────────

static void test_mixed_float_double_promotion() {
    auto f_node = lel_array(LatticeArray<float>(IPosition{4}, 2.0f));
    auto d_node = lel_array(LatticeArray<double>(IPosition{4}, 3.0));

    // Promote float to double and add.
    auto promoted = lel_promote_float(f_node);
    auto result = lel_add(promoted, d_node)->eval();
    CHECK_NEAR(result.values.flat()[0], 5.0, 1e-10);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    // Category 1: Scalar/lattice arithmetic
    test_cat1_lattice_lattice_arith();
    test_cat1_scalar_lattice_arith();
    test_cat1_unary_plus_minus();
    test_cat1_div_by_zero_ieee();
    test_cat1_special_scalars();

    // Category 2: Comparison and logical operators
    test_cat2_comparison_ops();
    test_cat2_logical_ops();

    // Category 3: Mask-aware boolean evaluation
    test_cat3_iif_with_mask();
    test_cat3_value_extract();
    test_cat3_mask_extract();

    // Category 4: Reductions
    test_cat4_reductions_with_mask();
    test_cat4_bool_reductions();
    test_cat4_variance_stddev();

    // Category 5: Mathematical functions
    test_cat5_trig();
    test_cat5_inverse_trig();
    test_cat5_exp_log();
    test_cat5_round_ceil_floor_sign();
    test_cat5_pow_fmod_atan2();
    test_cat5_min2_max2();
    test_cat5_mask_through_function();

    // Category 6: Scalar-lattice broadcasting
    test_cat6_broadcast_patterns();
    test_cat6_shape_mismatch_error();
    test_cat6_nested_broadcast();

    // Category 7: Complex number operations
    test_cat7_form_complex();
    test_cat7_real_imag();
    test_cat7_conj();
    test_cat7_abs_arg();
    test_cat7_complex_arithmetic();

    // Category 8: Malformed-expression diagnostics
    test_cat8_parse_errors();
    test_cat8_type_errors();
    test_cat8_shape_error_at_eval();
    test_cat8_parse_error_position();

    // Parser tests
    test_parser_arithmetic();
    test_parser_comparison_logical();
    test_parser_functions();
    test_parser_iif();
    test_parser_bool_reductions();
    test_parser_value_mask();
    test_parser_unary_minus();
    test_parser_bool_literals();

    // Mixed type promotion
    test_mixed_float_double_promotion();

    std::cout << "lel_parser_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
