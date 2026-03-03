// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/lattice_expr.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <stdexcept>

namespace casacore_mini {

// ── Binary op name ───────────────────────────────────────────────────

const char* lel_binary_op_name(LelBinaryOp op) {
    switch (op) {
    case LelBinaryOp::add: return "+";
    case LelBinaryOp::sub: return "-";
    case LelBinaryOp::mul: return "*";
    case LelBinaryOp::div_op: return "/";
    case LelBinaryOp::eq: return "==";
    case LelBinaryOp::ne: return "!=";
    case LelBinaryOp::gt: return ">";
    case LelBinaryOp::ge: return ">=";
    case LelBinaryOp::lt: return "<";
    case LelBinaryOp::le: return "<=";
    case LelBinaryOp::logical_and: return "&&";
    case LelBinaryOp::logical_or: return "||";
    }
    return "?";
}

// ── Helpers: broadcast scalar to lattice ─────────────────────────────

/// Expand a scalar result to match a lattice shape.
template <typename T>
static LatticeArray<T> broadcast(const LelResult<T>& scalar,
                                 const IPosition& target_shape) {
    T val = scalar.values.flat()[0];
    return LatticeArray<T>(target_shape, val);
}

/// Combine two optional masks (AND logic).
static std::optional<LatticeArray<bool>>
combine_masks(const std::optional<LatticeArray<bool>>& a,
              const std::optional<LatticeArray<bool>>& b,
              const IPosition& shape) {
    if (!a && !b) return std::nullopt;
    if (a && !b) return a;
    if (!a && b) return b;
    // Both present: AND them.
    LatticeArray<bool> result(shape, false);
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), shape);
        result.put(pos, a->flat()[i] && b->flat()[i]);
    }
    return result;
}

// ── LelBinary<T> implementation ──────────────────────────────────────

template <typename T>
std::optional<IPosition> LelBinary<T>::shape() const {
    auto ls = left_->shape();
    auto rs = right_->shape();
    if (ls && rs) {
        if (*ls != *rs) throw std::runtime_error("LEL: shape mismatch in binary op");
        return ls;
    }
    if (ls) return ls;
    if (rs) return rs;
    return std::nullopt; // both scalar
}

template <typename T>
LelResult<T> LelBinary<T>::eval() const {
    auto lr = left_->eval();
    auto rr = right_->eval();

    auto sh = shape();
    if (!sh) {
        // Both scalar.
        T lv = lr.values.flat()[0];
        T rv = rr.values.flat()[0];
        T res{};
        switch (op_) {
        case LelBinaryOp::add: res = lv + rv; break;
        case LelBinaryOp::sub: res = lv - rv; break;
        case LelBinaryOp::mul: res = lv * rv; break;
        case LelBinaryOp::div_op: res = lv / rv; break;
        default: throw std::runtime_error("LEL: invalid op for arithmetic binary");
        }
        return {LatticeArray<T>(IPosition{1}, res), std::nullopt};
    }

    // At least one is a lattice.
    auto la = left_->is_scalar() ? broadcast(lr, *sh) : lr.values;
    auto ra = right_->is_scalar() ? broadcast(rr, *sh) : rr.values;

    LatticeArray<T> result(*sh, T{});
    result.make_unique();
    auto nel = result.nelements();

    for (std::size_t i = 0; i < nel; ++i) {
        T lv = la.flat()[i];
        T rv = ra.flat()[i];
        T res{};
        switch (op_) {
        case LelBinaryOp::add: res = lv + rv; break;
        case LelBinaryOp::sub: res = lv - rv; break;
        case LelBinaryOp::mul: res = lv * rv; break;
        case LelBinaryOp::div_op: res = lv / rv; break;
        default: throw std::runtime_error("LEL: invalid op for arithmetic binary");
        }
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, res);
    }

    auto mask = combine_masks(
        left_->is_scalar() ? std::nullopt : lr.mask,
        right_->is_scalar() ? std::nullopt : rr.mask,
        *sh);
    return {std::move(result), std::move(mask)};
}

// Specialization for bool (logical AND/OR).
template <>
LelResult<bool> LelBinary<bool>::eval() const {
    auto lr = left_->eval();
    auto rr = right_->eval();
    auto sh = shape();

    if (!sh) {
        bool lv = lr.values.flat()[0];
        bool rv = rr.values.flat()[0];
        bool res = false;
        switch (op_) {
        case LelBinaryOp::logical_and: res = lv && rv; break;
        case LelBinaryOp::logical_or: res = lv || rv; break;
        default: throw std::runtime_error("LEL: invalid op for bool binary");
        }
        return {LatticeArray<bool>(IPosition{1}, res), std::nullopt};
    }

    auto la = left_->is_scalar() ? broadcast(lr, *sh) : lr.values;
    auto ra = right_->is_scalar() ? broadcast(rr, *sh) : rr.values;

    LatticeArray<bool> result(*sh, false);
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        bool lv = la.flat()[i];
        bool rv = ra.flat()[i];
        bool res = false;
        switch (op_) {
        case LelBinaryOp::logical_and: res = lv && rv; break;
        case LelBinaryOp::logical_or: res = lv || rv; break;
        default: throw std::runtime_error("LEL: invalid op for bool binary");
        }
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, res);
    }

    auto mask = combine_masks(
        left_->is_scalar() ? std::nullopt : lr.mask,
        right_->is_scalar() ? std::nullopt : rr.mask,
        *sh);
    return {std::move(result), std::move(mask)};
}

// ── LelCompare<T> implementation ─────────────────────────────────────

template <typename T>
std::optional<IPosition> LelCompare<T>::shape() const {
    auto ls = left_->shape();
    auto rs = right_->shape();
    if (ls && rs) {
        if (*ls != *rs) throw std::runtime_error("LEL: shape mismatch in compare");
        return ls;
    }
    if (ls) return ls;
    if (rs) return rs;
    return std::nullopt;
}

template <typename T>
LelResult<bool> LelCompare<T>::eval() const {
    auto lr = left_->eval();
    auto rr = right_->eval();
    auto sh = shape();

    auto compare_one = [this](T lv, T rv) -> bool {
        switch (op_) {
        case LelBinaryOp::eq: return lv == rv;
        case LelBinaryOp::ne: return lv != rv;
        case LelBinaryOp::gt: return lv > rv;
        case LelBinaryOp::ge: return lv >= rv;
        case LelBinaryOp::lt: return lv < rv;
        case LelBinaryOp::le: return lv <= rv;
        default: throw std::runtime_error("LEL: invalid comparison op");
        }
    };

    if (!sh) {
        bool res = compare_one(lr.values.flat()[0], rr.values.flat()[0]);
        return {LatticeArray<bool>(IPosition{1}, res), std::nullopt};
    }

    auto la = left_->is_scalar() ? broadcast(lr, *sh) : lr.values;
    auto ra = right_->is_scalar() ? broadcast(rr, *sh) : rr.values;

    LatticeArray<bool> result(*sh, false);
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        bool res = compare_one(la.flat()[i], ra.flat()[i]);
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, res);
    }

    auto mask = combine_masks(
        left_->is_scalar() ? std::nullopt : lr.mask,
        right_->is_scalar() ? std::nullopt : rr.mask,
        *sh);
    return {std::move(result), std::move(mask)};
}

// ── LelUnary<T> implementation ───────────────────────────────────────

template <typename T>
LelResult<T> LelUnary<T>::eval() const {
    auto r = operand_->eval();
    auto sh = r.values.shape();
    LatticeArray<T> result(sh, T{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        T v = r.values.flat()[i];
        T res{};
        switch (op_) {
        case LelUnaryOp::negate: res = -v; break;
        case LelUnaryOp::identity: res = v; break;
        default: throw std::runtime_error("LEL: invalid unary op");
        }
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, res);
    }
    return {std::move(result), r.mask};
}

// Bool specialization for logical NOT.
template <>
LelResult<bool> LelUnary<bool>::eval() const {
    auto r = operand_->eval();
    auto sh = r.values.shape();
    LatticeArray<bool> result(sh, false);
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        bool v = r.values.flat()[i];
        bool res = false;
        switch (op_) {
        case LelUnaryOp::logical_not: res = !v; break;
        case LelUnaryOp::identity: res = v; break;
        default: throw std::runtime_error("LEL: invalid unary bool op");
        }
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, res);
    }
    return {std::move(result), r.mask};
}

// ── LelFunc1<T> implementation ───────────────────────────────────────

template <typename T>
LelResult<T> LelFunc1<T>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<T> result(sh, T{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, fn_(r.values.flat()[i]));
    }
    return {std::move(result), r.mask};
}

// ── LelFunc2<T> implementation ───────────────────────────────────────

template <typename T>
std::optional<IPosition> LelFunc2<T>::shape() const {
    auto s1 = arg1_->shape();
    auto s2 = arg2_->shape();
    if (s1 && s2) {
        if (*s1 != *s2) throw std::runtime_error("LEL: shape mismatch in func2");
        return s1;
    }
    if (s1) return s1;
    if (s2) return s2;
    return std::nullopt;
}

template <typename T>
LelResult<T> LelFunc2<T>::eval() const {
    auto r1 = arg1_->eval();
    auto r2 = arg2_->eval();
    auto sh = shape();

    if (!sh) {
        T res = fn_(r1.values.flat()[0], r2.values.flat()[0]);
        return {LatticeArray<T>(IPosition{1}, res), std::nullopt};
    }

    auto a1 = arg1_->is_scalar() ? broadcast(r1, *sh) : r1.values;
    auto a2 = arg2_->is_scalar() ? broadcast(r2, *sh) : r2.values;

    LatticeArray<T> result(*sh, T{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, fn_(a1.flat()[i], a2.flat()[i]));
    }

    auto mask = combine_masks(
        arg1_->is_scalar() ? std::nullopt : r1.mask,
        arg2_->is_scalar() ? std::nullopt : r2.mask,
        *sh);
    return {std::move(result), std::move(mask)};
}

// ── LelReduce<T> implementation ──────────────────────────────────────

template <typename T>
LelResult<T> LelReduce<T>::eval() const {
    auto r = arg_->eval();
    T val = fn_(r.values, r.mask);
    return {LatticeArray<T>(IPosition{1}, val), std::nullopt};
}

// ── LelBoolReduce<R> implementation ──────────────────────────────────

template <typename R>
LelResult<R> LelBoolReduce<R>::eval() const {
    auto r = arg_->eval();
    R val = fn_(r.values, r.mask);
    return {LatticeArray<R>(IPosition{1}, val), std::nullopt};
}

// ── LelIif<T> implementation ─────────────────────────────────────────

template <typename T>
std::optional<IPosition> LelIif<T>::shape() const {
    auto cs = cond_->shape();
    auto ts = true_val_->shape();
    auto fs = false_val_->shape();
    if (cs) return cs;
    if (ts) return ts;
    if (fs) return fs;
    return std::nullopt;
}

template <typename T>
LelResult<T> LelIif<T>::eval() const {
    auto cr = cond_->eval();
    auto tr = true_val_->eval();
    auto fr = false_val_->eval();
    auto sh = shape();

    if (!sh) {
        T res = cr.values.flat()[0] ? tr.values.flat()[0] : fr.values.flat()[0];
        return {LatticeArray<T>(IPosition{1}, res), std::nullopt};
    }

    auto ca = cond_->is_scalar() ? broadcast(cr, *sh) : cr.values;
    auto ta = true_val_->is_scalar() ? broadcast(tr, *sh) : tr.values;
    auto fa = false_val_->is_scalar() ? broadcast(fr, *sh) : fr.values;

    LatticeArray<T> result(*sh, T{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, ca.flat()[i] ? ta.flat()[i] : fa.flat()[i]);
    }

    // Combine masks from all three operands.
    auto mask = combine_masks(
        cond_->is_scalar() ? std::nullopt : cr.mask,
        combine_masks(
            true_val_->is_scalar() ? std::nullopt : tr.mask,
            false_val_->is_scalar() ? std::nullopt : fr.mask,
            *sh),
        *sh);
    return {std::move(result), std::move(mask)};
}

// ── Math function factories ──────────────────────────────────────────

template <typename T>
std::shared_ptr<LelNode<T>>
lel_math1(LelFunc id, std::shared_ptr<LelNode<T>> arg) {
    std::function<T(T)> fn;
    switch (id) {
    case LelFunc::sin_f: fn = [](T v) { return static_cast<T>(std::sin(v)); }; break;
    case LelFunc::cos_f: fn = [](T v) { return static_cast<T>(std::cos(v)); }; break;
    case LelFunc::tan_f: fn = [](T v) { return static_cast<T>(std::tan(v)); }; break;
    case LelFunc::asin_f: fn = [](T v) { return static_cast<T>(std::asin(v)); }; break;
    case LelFunc::acos_f: fn = [](T v) { return static_cast<T>(std::acos(v)); }; break;
    case LelFunc::atan_f: fn = [](T v) { return static_cast<T>(std::atan(v)); }; break;
    case LelFunc::exp_f: fn = [](T v) { return static_cast<T>(std::exp(v)); }; break;
    case LelFunc::log_f: fn = [](T v) { return static_cast<T>(std::log(v)); }; break;
    case LelFunc::log10_f: fn = [](T v) { return static_cast<T>(std::log10(v)); }; break;
    case LelFunc::sqrt_f: fn = [](T v) { return static_cast<T>(std::sqrt(v)); }; break;
    case LelFunc::abs_f: fn = [](T v) { return static_cast<T>(std::abs(v)); }; break;
    case LelFunc::ceil_f: fn = [](T v) { return static_cast<T>(std::ceil(v)); }; break;
    case LelFunc::floor_f: fn = [](T v) { return static_cast<T>(std::floor(v)); }; break;
    case LelFunc::round_f: fn = [](T v) { return static_cast<T>(std::round(v)); }; break;
    case LelFunc::sign_f: fn = [](T v) { return v > T{} ? T{1} : (v < T{} ? T{-1} : T{}); }; break;
    default: throw std::runtime_error("LEL: unknown 1-arg function");
    }
    return std::make_shared<LelFunc1<T>>(id, std::move(fn), std::move(arg));
}

template <typename T>
std::shared_ptr<LelNode<T>>
lel_math2(LelFunc id, std::shared_ptr<LelNode<T>> arg1,
          std::shared_ptr<LelNode<T>> arg2) {
    std::function<T(T, T)> fn;
    switch (id) {
    case LelFunc::atan2_f: fn = [](T a, T b) { return static_cast<T>(std::atan2(a, b)); }; break;
    case LelFunc::pow_f: fn = [](T a, T b) { return static_cast<T>(std::pow(a, b)); }; break;
    case LelFunc::fmod_f: fn = [](T a, T b) { return static_cast<T>(std::fmod(a, b)); }; break;
    case LelFunc::min2_f: fn = [](T a, T b) { return std::min(a, b); }; break;
    case LelFunc::max2_f: fn = [](T a, T b) { return std::max(a, b); }; break;
    default: throw std::runtime_error("LEL: unknown 2-arg function");
    }
    return std::make_shared<LelFunc2<T>>(id, std::move(fn),
                                          std::move(arg1), std::move(arg2));
}

// ── Reduction factories ──────────────────────────────────────────────

template <typename T>
std::shared_ptr<LelNode<T>>
lel_reduce(LelFunc id, std::shared_ptr<LelNode<T>> arg) {
    using reduce_fn = std::function<T(const LatticeArray<T>&,
                                      const std::optional<LatticeArray<bool>>&)>;

    auto is_unmasked = [](const std::optional<LatticeArray<bool>>& mask,
                          std::size_t i) -> bool {
        return !mask || mask->flat()[i];
    };

    reduce_fn fn;
    switch (id) {
    case LelFunc::sum_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T s{};
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) s += v.flat()[i];
            }
            return s;
        };
        break;
    case LelFunc::min_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T best = std::numeric_limits<T>::max();
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && v.flat()[i] < best) best = v.flat()[i];
            }
            return best;
        };
        break;
    case LelFunc::max_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T best = std::numeric_limits<T>::lowest();
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && v.flat()[i] > best) best = v.flat()[i];
            }
            return best;
        };
        break;
    case LelFunc::mean_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T s{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) { s += v.flat()[i]; ++n; }
            }
            return n > 0 ? static_cast<T>(s / static_cast<T>(n)) : T{};
        };
        break;
    case LelFunc::median_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            std::vector<T> vals;
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) vals.push_back(v.flat()[i]);
            }
            if (vals.empty()) return T{};
            std::sort(vals.begin(), vals.end());
            auto mid = vals.size() / 2;
            if (vals.size() % 2 == 0) {
                return static_cast<T>((vals[mid - 1] + vals[mid]) / T{2});
            }
            return vals[mid];
        };
        break;
    case LelFunc::variance_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T s{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) { s += v.flat()[i]; ++n; }
            }
            if (n < 2) return T{};
            T mean = s / static_cast<T>(n);
            T var{};
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) {
                    T diff = v.flat()[i] - mean;
                    var += diff * diff;
                }
            }
            return var / static_cast<T>(n - 1);
        };
        break;
    case LelFunc::stddev_r:
        fn = [is_unmasked](const LatticeArray<T>& v,
                           const std::optional<LatticeArray<bool>>& m) -> T {
            T s{};
            std::size_t n = 0;
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) { s += v.flat()[i]; ++n; }
            }
            if (n < 2) return T{};
            T mean = s / static_cast<T>(n);
            T var{};
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i)) {
                    T diff = v.flat()[i] - mean;
                    var += diff * diff;
                }
            }
            return static_cast<T>(std::sqrt(var / static_cast<T>(n - 1)));
        };
        break;
    default:
        throw std::runtime_error("LEL: unknown reduction function");
    }
    return std::make_shared<LelReduce<T>>(id, std::move(fn), std::move(arg));
}

// ── Bool reduction factory ────────────────────────────────────────────

template <typename R>
std::shared_ptr<LelNode<R>>
lel_bool_reduce(LelFunc id, std::shared_ptr<LelNode<bool>> arg) {
    using reduce_fn = std::function<R(const LatticeArray<bool>&,
                                      const std::optional<LatticeArray<bool>>&)>;

    auto is_unmasked = [](const std::optional<LatticeArray<bool>>& mask,
                          std::size_t i) -> bool {
        return !mask || mask->flat()[i];
    };

    reduce_fn fn;
    switch (id) {
    case LelFunc::ntrue_r:
        fn = [is_unmasked](const LatticeArray<bool>& v,
                           const std::optional<LatticeArray<bool>>& m) -> R {
            R count{};
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && v.flat()[i]) count += R{1};
            }
            return count;
        };
        break;
    case LelFunc::nfalse_r:
        fn = [is_unmasked](const LatticeArray<bool>& v,
                           const std::optional<LatticeArray<bool>>& m) -> R {
            R count{};
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && !v.flat()[i]) count += R{1};
            }
            return count;
        };
        break;
    case LelFunc::any_r:
        fn = [is_unmasked](const LatticeArray<bool>& v,
                           const std::optional<LatticeArray<bool>>& m) -> R {
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && v.flat()[i]) return R{1};
            }
            return R{};
        };
        break;
    case LelFunc::all_r:
        fn = [is_unmasked](const LatticeArray<bool>& v,
                           const std::optional<LatticeArray<bool>>& m) -> R {
            for (std::size_t i = 0; i < v.nelements(); ++i) {
                if (is_unmasked(m, i) && !v.flat()[i]) return R{};
            }
            return R{1};
        };
        break;
    default:
        throw std::runtime_error("LEL: unknown bool reduction function");
    }
    return std::make_shared<LelBoolReduce<R>>(id, std::move(fn), std::move(arg));
}

// ── LelMaskExtract<T> implementation ─────────────────────────────────

template <typename T>
LelResult<bool> LelMaskExtract<T>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    if (r.mask) {
        return {*r.mask, std::nullopt};
    }
    // No mask → all true.
    return {LatticeArray<bool>(sh, true), std::nullopt};
}

// ── Complex node implementations ─────────────────────────────────────

template <typename R>
LelResult<R> LelReal<R>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<R> result(sh, R{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, r.values.flat()[i].real());
    }
    return {std::move(result), r.mask};
}

template <typename R>
LelResult<R> LelImag<R>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<R> result(sh, R{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, r.values.flat()[i].imag());
    }
    return {std::move(result), r.mask};
}

template <typename R>
LelResult<std::complex<R>> LelConj<R>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<std::complex<R>> result(sh, std::complex<R>{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, std::conj(r.values.flat()[i]));
    }
    return {std::move(result), r.mask};
}

template <typename R>
LelResult<R> LelArg<R>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<R> result(sh, R{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, std::arg(r.values.flat()[i]));
    }
    return {std::move(result), r.mask};
}

template <typename R>
LelResult<R> LelComplexAbs<R>::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<R> result(sh, R{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, std::abs(r.values.flat()[i]));
    }
    return {std::move(result), r.mask};
}

template <typename R>
std::optional<IPosition> LelFormComplex<R>::shape() const {
    auto s1 = re_->shape();
    auto s2 = im_->shape();
    if (s1 && s2) {
        if (*s1 != *s2) throw std::runtime_error("LEL: shape mismatch in formComplex");
        return s1;
    }
    if (s1) return s1;
    if (s2) return s2;
    return std::nullopt;
}

template <typename R>
LelResult<std::complex<R>> LelFormComplex<R>::eval() const {
    auto rr = re_->eval();
    auto ir = im_->eval();
    auto sh = shape();

    if (!sh) {
        std::complex<R> val(rr.values.flat()[0], ir.values.flat()[0]);
        return {LatticeArray<std::complex<R>>(IPosition{1}, val), std::nullopt};
    }

    auto ra = re_->is_scalar() ? broadcast(rr, *sh) : rr.values;
    auto ia = im_->is_scalar() ? broadcast(ir, *sh) : ir.values;

    LatticeArray<std::complex<R>> result(*sh, std::complex<R>{});
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), *sh);
        result.put(pos, std::complex<R>(ra.flat()[i], ia.flat()[i]));
    }

    auto mask = combine_masks(
        re_->is_scalar() ? std::nullopt : rr.mask,
        im_->is_scalar() ? std::nullopt : ir.mask,
        *sh);
    return {std::move(result), std::move(mask)};
}

// ── LelPromoteFloat implementation ───────────────────────────────────

LelResult<double> LelPromoteFloat::eval() const {
    auto r = arg_->eval();
    auto sh = r.values.shape();
    LatticeArray<double> result(sh, 0.0);
    result.make_unique();
    for (std::size_t i = 0; i < result.nelements(); ++i) {
        auto pos = delinearize(static_cast<std::int64_t>(i), sh);
        result.put(pos, static_cast<double>(r.values.flat()[i]));
    }
    std::optional<LatticeArray<bool>> mask;
    if (r.mask) mask = *r.mask;
    return {std::move(result), std::move(mask)};
}

// ── LEL Parser ───────────────────────────────────────────────────────

namespace {

class LelParser {
  public:
    LelParser(const std::string& input, const LelSymbolTable& symbols)
        : input_(input), symbols_(symbols) {}

    LatticeExprNode parse() {
        skip_ws();
        if (pos_ >= input_.size()) {
            throw LelParseError("empty expression", 0);
        }
        auto result = parse_or();
        skip_ws();
        if (pos_ < input_.size()) {
            throw LelParseError("unexpected character '" +
                                std::string(1, input_[pos_]) + "'", pos_);
        }
        return result;
    }

  private:
    const std::string& input_;
    const LelSymbolTable& symbols_;
    std::size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(input_[pos_])) ++pos_;
    }

    char peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char advance() {
        return input_[pos_++];
    }

    bool match(char c) {
        skip_ws();
        if (peek() == c) { ++pos_; return true; }
        return false;
    }

    bool match(const std::string& s) {
        skip_ws();
        if (input_.compare(pos_, s.size(), s) == 0) {
            pos_ += s.size();
            return true;
        }
        return false;
    }

    void expect(char c) {
        skip_ws();
        if (peek() != c) {
            throw LelParseError(
                std::string("expected '") + c + "' but got '" +
                (peek() ? std::string(1, peek()) : "EOF") + "'", pos_);
        }
        ++pos_;
    }

    // ── Grammar: or → and (|| and)*
    LatticeExprNode parse_or() {
        auto left = parse_and();
        while (true) {
            skip_ws();
            if (match("||")) {
                auto right = parse_and();
                left = make_logical(LelBinaryOp::logical_or, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // ── and → comparison (&& comparison)*
    LatticeExprNode parse_and() {
        auto left = parse_comparison();
        while (true) {
            skip_ws();
            if (match("&&")) {
                auto right = parse_comparison();
                left = make_logical(LelBinaryOp::logical_and, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // ── comparison → additive (cmp_op additive)?
    LatticeExprNode parse_comparison() {
        auto left = parse_additive();
        skip_ws();
        LelBinaryOp op{};
        bool found = false;
        if (match("==")) { op = LelBinaryOp::eq; found = true; }
        else if (match("!=")) { op = LelBinaryOp::ne; found = true; }
        else if (match(">=")) { op = LelBinaryOp::ge; found = true; }
        else if (match("<=")) { op = LelBinaryOp::le; found = true; }
        else if (match(">")) { op = LelBinaryOp::gt; found = true; }
        else if (match("<")) { op = LelBinaryOp::lt; found = true; }
        if (found) {
            auto right = parse_additive();
            return make_compare(op, left, right);
        }
        return left;
    }

    // ── additive → multiplicative ((+|-) multiplicative)*
    LatticeExprNode parse_additive() {
        auto left = parse_multiplicative();
        while (true) {
            skip_ws();
            if (match("+")) {
                auto right = parse_multiplicative();
                left = make_arith(LelBinaryOp::add, left, right);
            } else if (peek() == '-' && pos_ + 1 < input_.size() &&
                       !std::isdigit(input_[pos_ + 1]) && input_[pos_ + 1] != '.') {
                ++pos_;
                auto right = parse_multiplicative();
                left = make_arith(LelBinaryOp::sub, left, right);
            } else if (match("-")) {
                auto right = parse_multiplicative();
                left = make_arith(LelBinaryOp::sub, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // ── multiplicative → unary ((*|/) unary)*
    LatticeExprNode parse_multiplicative() {
        auto left = parse_unary();
        while (true) {
            skip_ws();
            if (match("*")) {
                auto right = parse_unary();
                left = make_arith(LelBinaryOp::mul, left, right);
            } else if (match("/")) {
                auto right = parse_unary();
                left = make_arith(LelBinaryOp::div_op, left, right);
            } else {
                break;
            }
        }
        return left;
    }

    // ── unary → (! | -) unary | primary
    LatticeExprNode parse_unary() {
        skip_ws();
        if (match("!")) {
            auto operand = parse_unary();
            if (operand.result_type() != LelType::boolean) {
                throw LelParseError("logical NOT requires boolean operand", pos_);
            }
            return LatticeExprNode(lel_not(operand.as<bool>()));
        }
        if (peek() == '-') {
            // Unary minus: careful not to eat negative number literals here.
            // Check if next char after '-' starts a number (and we're not after
            // a primary). We parse it as unary negate on primary.
            ++pos_;
            skip_ws();
            auto operand = parse_primary();
            return make_negate(operand);
        }
        return parse_primary();
    }

    // ── primary → number | bool_lit | function_call | identifier | '(' expr ')'
    LatticeExprNode parse_primary() {
        skip_ws();
        if (pos_ >= input_.size()) {
            throw LelParseError("unexpected end of expression", pos_);
        }

        // Parenthesized expression.
        if (peek() == '(') {
            ++pos_;
            auto inner = parse_or();
            expect(')');
            return inner;
        }

        // Number literal.
        if (std::isdigit(peek()) || peek() == '.') {
            return parse_number();
        }

        // Identifier or function call or bool literal.
        if (std::isalpha(peek()) || peek() == '_') {
            return parse_identifier_or_call();
        }

        throw LelParseError("unexpected character '" +
                            std::string(1, peek()) + "'", pos_);
    }

    LatticeExprNode parse_number() {
        auto start = pos_;
        while (pos_ < input_.size() &&
               (std::isdigit(input_[pos_]) || input_[pos_] == '.' ||
                input_[pos_] == 'e' || input_[pos_] == 'E' ||
                ((input_[pos_] == '+' || input_[pos_] == '-') && pos_ > start &&
                 (input_[pos_-1] == 'e' || input_[pos_-1] == 'E')))) {
            ++pos_;
        }
        std::string num_str = input_.substr(start, pos_ - start);
        try {
            double val = std::stod(num_str);
            // Use float by default for integer-like or float-like values.
            // Use double if the literal has enough precision.
            return LatticeExprNode(lel_scalar(static_cast<float>(val)));
        } catch (...) {
            throw LelParseError("invalid number '" + num_str + "'", start);
        }
    }

    LatticeExprNode parse_identifier_or_call() {
        auto start = pos_;
        while (pos_ < input_.size() &&
               (std::isalnum(input_[pos_]) || input_[pos_] == '_')) {
            ++pos_;
        }
        std::string name = input_.substr(start, pos_ - start);

        // Boolean literals.
        if (name == "T" || name == "TRUE" || name == "true") {
            return LatticeExprNode(lel_scalar(true));
        }
        if (name == "F" || name == "FALSE" || name == "false") {
            return LatticeExprNode(lel_scalar(false));
        }

        skip_ws();

        // Function call.
        if (peek() == '(') {
            return parse_function_call(name, start);
        }

        // Lattice reference.
        if (!symbols_.has(name)) {
            throw LelParseError("undefined lattice reference '" + name + "'", start);
        }
        return symbols_.get(name);
    }

    LatticeExprNode parse_function_call(const std::string& name,
                                        std::size_t name_pos) {
        expect('(');
        std::vector<LatticeExprNode> args;
        skip_ws();
        if (peek() != ')') {
            args.push_back(parse_or());
            while (match(',')) {
                args.push_back(parse_or());
            }
        }
        expect(')');

        // 1-arg math functions.
        static const std::unordered_map<std::string, LelFunc> math1_map = {
            {"sin", LelFunc::sin_f}, {"cos", LelFunc::cos_f},
            {"tan", LelFunc::tan_f}, {"asin", LelFunc::asin_f},
            {"acos", LelFunc::acos_f}, {"atan", LelFunc::atan_f},
            {"exp", LelFunc::exp_f}, {"log", LelFunc::log_f},
            {"log10", LelFunc::log10_f}, {"sqrt", LelFunc::sqrt_f},
            {"abs", LelFunc::abs_f}, {"sign", LelFunc::sign_f},
            {"round", LelFunc::round_f}, {"ceil", LelFunc::ceil_f},
            {"floor", LelFunc::floor_f},
        };

        // 2-arg math functions.
        static const std::unordered_map<std::string, LelFunc> math2_map = {
            {"atan2", LelFunc::atan2_f}, {"pow", LelFunc::pow_f},
            {"fmod", LelFunc::fmod_f},
        };

        // Reductions (1-arg, produce scalar).
        static const std::unordered_map<std::string, LelFunc> reduce_map = {
            {"sum", LelFunc::sum_r}, {"mean", LelFunc::mean_r},
            {"median", LelFunc::median_r}, {"variance", LelFunc::variance_r},
            {"stddev", LelFunc::stddev_r},
        };

        // Bool reductions.
        static const std::unordered_map<std::string, LelFunc> bool_reduce_map = {
            {"ntrue", LelFunc::ntrue_r}, {"nfalse", LelFunc::nfalse_r},
            {"any", LelFunc::any_r}, {"all", LelFunc::all_r},
        };

        // min/max: 1-arg = reduction, 2-arg = element-wise.
        if (name == "min" || name == "max") {
            if (args.size() == 1) {
                LelFunc id = (name == "min") ? LelFunc::min_r : LelFunc::max_r;
                return apply_reduce(id, args[0]);
            }
            if (args.size() == 2) {
                LelFunc id = (name == "min") ? LelFunc::min2_f : LelFunc::max2_f;
                return apply_math2(id, args[0], args[1], name_pos);
            }
            throw LelParseError("min/max expects 1 or 2 arguments", name_pos);
        }

        // iif(cond, true, false)
        if (name == "iif") {
            if (args.size() != 3) {
                throw LelParseError("iif requires exactly 3 arguments", name_pos);
            }
            if (args[0].result_type() != LelType::boolean) {
                throw LelParseError("iif condition must be boolean", name_pos);
            }
            auto cond = args[0].as<bool>();
            // true and false branches must have same type.
            if (args[1].result_type() == LelType::float32) {
                return LatticeExprNode(
                    lel_iif(cond, args[1].as<float>(), args[2].as<float>()));
            }
            if (args[1].result_type() == LelType::float64) {
                return LatticeExprNode(
                    lel_iif(cond, args[1].as<double>(), args[2].as<double>()));
            }
            if (args[1].result_type() == LelType::boolean) {
                return LatticeExprNode(
                    lel_iif(cond, args[1].as<bool>(), args[2].as<bool>()));
            }
            throw LelParseError("unsupported iif result type", name_pos);
        }

        // value(expr)
        if (name == "value") {
            if (args.size() != 1) {
                throw LelParseError("value requires exactly 1 argument", name_pos);
            }
            auto& a = args[0];
            if (a.result_type() == LelType::float32)
                return LatticeExprNode(lel_value(a.as<float>()));
            if (a.result_type() == LelType::float64)
                return LatticeExprNode(lel_value(a.as<double>()));
            if (a.result_type() == LelType::boolean)
                return LatticeExprNode(lel_value(a.as<bool>()));
            throw LelParseError("unsupported type for value()", name_pos);
        }

        // mask(expr)
        if (name == "mask") {
            if (args.size() != 1) {
                throw LelParseError("mask requires exactly 1 argument", name_pos);
            }
            auto& a = args[0];
            if (a.result_type() == LelType::float32)
                return LatticeExprNode(lel_mask(a.as<float>()));
            if (a.result_type() == LelType::float64)
                return LatticeExprNode(lel_mask(a.as<double>()));
            if (a.result_type() == LelType::boolean)
                return LatticeExprNode(lel_mask(a.as<bool>()));
            throw LelParseError("unsupported type for mask()", name_pos);
        }

        // 1-arg math.
        auto it1 = math1_map.find(name);
        if (it1 != math1_map.end()) {
            if (args.size() != 1) {
                throw LelParseError(name + " requires exactly 1 argument", name_pos);
            }
            return apply_math1(it1->second, args[0]);
        }

        // 2-arg math.
        auto it2 = math2_map.find(name);
        if (it2 != math2_map.end()) {
            if (args.size() != 2) {
                throw LelParseError(name + " requires exactly 2 arguments", name_pos);
            }
            return apply_math2(it2->second, args[0], args[1], name_pos);
        }

        // Reductions.
        auto itr = reduce_map.find(name);
        if (itr != reduce_map.end()) {
            if (args.size() != 1) {
                throw LelParseError(name + " requires exactly 1 argument", name_pos);
            }
            return apply_reduce(itr->second, args[0]);
        }

        // Bool reductions.
        auto itb = bool_reduce_map.find(name);
        if (itb != bool_reduce_map.end()) {
            if (args.size() != 1) {
                throw LelParseError(name + " requires exactly 1 argument", name_pos);
            }
            if (args[0].result_type() != LelType::boolean) {
                throw LelParseError(name + " requires boolean argument", name_pos);
            }
            return LatticeExprNode(
                lel_bool_reduce<float>(itb->second, args[0].as<bool>()));
        }

        throw LelParseError("unknown function '" + name + "'", name_pos);
    }

    // ── Type dispatch helpers ────────────────────────────────────────

    LatticeExprNode apply_math1(LelFunc id, LatticeExprNode& a) {
        if (a.result_type() == LelType::float32)
            return LatticeExprNode(lel_math1(id, a.as<float>()));
        if (a.result_type() == LelType::float64)
            return LatticeExprNode(lel_math1(id, a.as<double>()));
        throw LelParseError("math function requires numeric argument", pos_);
    }

    LatticeExprNode apply_math2(LelFunc id, LatticeExprNode& a,
                                LatticeExprNode& b, std::size_t err_pos) {
        auto at = a.result_type();
        auto bt = b.result_type();
        if (at == LelType::float32 && bt == LelType::float32)
            return LatticeExprNode(lel_math2(id, a.as<float>(), b.as<float>()));
        if (at == LelType::float64 && bt == LelType::float64)
            return LatticeExprNode(lel_math2(id, a.as<double>(), b.as<double>()));
        // Promote float→double if mixed.
        if (at == LelType::float32 && bt == LelType::float64)
            return LatticeExprNode(
                lel_math2(id, lel_promote_float(a.as<float>()), b.as<double>()));
        if (at == LelType::float64 && bt == LelType::float32)
            return LatticeExprNode(
                lel_math2(id, a.as<double>(), lel_promote_float(b.as<float>())));
        throw LelParseError("type mismatch in 2-arg function", err_pos);
    }

    LatticeExprNode apply_reduce(LelFunc id, LatticeExprNode& a) {
        if (a.result_type() == LelType::float32)
            return LatticeExprNode(lel_reduce(id, a.as<float>()));
        if (a.result_type() == LelType::float64)
            return LatticeExprNode(lel_reduce(id, a.as<double>()));
        throw LelParseError("reduction requires numeric argument", pos_);
    }

    LatticeExprNode make_arith(LelBinaryOp op, LatticeExprNode& left,
                               LatticeExprNode& right) {
        auto lt = left.result_type();
        auto rt = right.result_type();
        if (lt == LelType::float32 && rt == LelType::float32) {
            std::shared_ptr<LelNode<float>> node;
            switch (op) {
            case LelBinaryOp::add: node = lel_add(left.as<float>(), right.as<float>()); break;
            case LelBinaryOp::sub: node = lel_sub(left.as<float>(), right.as<float>()); break;
            case LelBinaryOp::mul: node = lel_mul(left.as<float>(), right.as<float>()); break;
            case LelBinaryOp::div_op: node = lel_div(left.as<float>(), right.as<float>()); break;
            default: throw LelParseError("invalid arithmetic op", pos_);
            }
            return LatticeExprNode(node);
        }
        if (lt == LelType::float64 && rt == LelType::float64) {
            std::shared_ptr<LelNode<double>> node;
            switch (op) {
            case LelBinaryOp::add: node = lel_add(left.as<double>(), right.as<double>()); break;
            case LelBinaryOp::sub: node = lel_sub(left.as<double>(), right.as<double>()); break;
            case LelBinaryOp::mul: node = lel_mul(left.as<double>(), right.as<double>()); break;
            case LelBinaryOp::div_op: node = lel_div(left.as<double>(), right.as<double>()); break;
            default: throw LelParseError("invalid arithmetic op", pos_);
            }
            return LatticeExprNode(node);
        }
        // Mixed float/double → promote to double.
        if ((lt == LelType::float32 || lt == LelType::float64) &&
            (rt == LelType::float32 || rt == LelType::float64)) {
            auto dl = lt == LelType::float32
                          ? lel_promote_float(left.as<float>())
                          : left.as<double>();
            auto dr = rt == LelType::float32
                          ? lel_promote_float(right.as<float>())
                          : right.as<double>();
            std::shared_ptr<LelNode<double>> node;
            switch (op) {
            case LelBinaryOp::add: node = lel_add(dl, dr); break;
            case LelBinaryOp::sub: node = lel_sub(dl, dr); break;
            case LelBinaryOp::mul: node = lel_mul(dl, dr); break;
            case LelBinaryOp::div_op: node = lel_div(dl, dr); break;
            default: throw LelParseError("invalid arithmetic op", pos_);
            }
            return LatticeExprNode(node);
        }
        if (lt == LelType::boolean || rt == LelType::boolean) {
            throw LelParseError("arithmetic on boolean operands is not allowed", pos_);
        }
        throw LelParseError("unsupported types for arithmetic", pos_);
    }

    LatticeExprNode make_compare(LelBinaryOp op, LatticeExprNode& left,
                                 LatticeExprNode& right) {
        auto lt = left.result_type();
        auto rt = right.result_type();
        if (lt == LelType::float32 && rt == LelType::float32)
            return LatticeExprNode(lel_compare(op, left.as<float>(), right.as<float>()));
        if (lt == LelType::float64 && rt == LelType::float64)
            return LatticeExprNode(lel_compare(op, left.as<double>(), right.as<double>()));
        // Mixed float/double.
        if ((lt == LelType::float32 || lt == LelType::float64) &&
            (rt == LelType::float32 || rt == LelType::float64)) {
            auto dl = lt == LelType::float32
                          ? lel_promote_float(left.as<float>())
                          : left.as<double>();
            auto dr = rt == LelType::float32
                          ? lel_promote_float(right.as<float>())
                          : right.as<double>();
            return LatticeExprNode(lel_compare(op, dl, dr));
        }
        throw LelParseError("comparison requires matching numeric types", pos_);
    }

    LatticeExprNode make_logical(LelBinaryOp op, LatticeExprNode& left,
                                 LatticeExprNode& right) {
        if (left.result_type() != LelType::boolean ||
            right.result_type() != LelType::boolean) {
            throw LelParseError("logical operators require boolean operands", pos_);
        }
        if (op == LelBinaryOp::logical_and)
            return LatticeExprNode(lel_and(left.as<bool>(), right.as<bool>()));
        return LatticeExprNode(lel_or(left.as<bool>(), right.as<bool>()));
    }

    LatticeExprNode make_negate(LatticeExprNode& operand) {
        if (operand.result_type() == LelType::float32)
            return LatticeExprNode(lel_negate(operand.as<float>()));
        if (operand.result_type() == LelType::float64)
            return LatticeExprNode(lel_negate(operand.as<double>()));
        throw LelParseError("unary negate requires numeric operand", pos_);
    }
};

} // anonymous namespace

LatticeExprNode
lel_parse(const std::string& expr, const LelSymbolTable& symbols) {
    LelParser parser(expr, symbols);
    return parser.parse();
}

// ── Explicit template instantiations ─────────────────────────────────

template class LelBinary<float>;
template class LelBinary<double>;
template class LelBinary<bool>;
template class LelCompare<float>;
template class LelCompare<double>;
template class LelUnary<float>;
template class LelUnary<double>;
template class LelFunc1<float>;
template class LelFunc1<double>;
template class LelFunc2<float>;
template class LelFunc2<double>;
template class LelReduce<float>;
template class LelReduce<double>;
template class LelBoolReduce<float>;
template class LelBoolReduce<double>;
template class LelIif<float>;
template class LelIif<double>;
template class LelIif<bool>;

// value/mask extractors
template class LelMaskExtract<float>;
template class LelMaskExtract<double>;
template class LelMaskExtract<bool>;

// Complex nodes
template class LelReal<float>;
template class LelReal<double>;
template class LelImag<float>;
template class LelImag<double>;
template class LelConj<float>;
template class LelConj<double>;
template class LelArg<float>;
template class LelArg<double>;
template class LelComplexAbs<float>;
template class LelComplexAbs<double>;
template class LelFormComplex<float>;
template class LelFormComplex<double>;
template class LelBinary<std::complex<float>>;
template class LelBinary<std::complex<double>>;

template std::shared_ptr<LelNode<float>>
lel_math1(LelFunc, std::shared_ptr<LelNode<float>>);
template std::shared_ptr<LelNode<double>>
lel_math1(LelFunc, std::shared_ptr<LelNode<double>>);

template std::shared_ptr<LelNode<float>>
lel_math2(LelFunc, std::shared_ptr<LelNode<float>>, std::shared_ptr<LelNode<float>>);
template std::shared_ptr<LelNode<double>>
lel_math2(LelFunc, std::shared_ptr<LelNode<double>>, std::shared_ptr<LelNode<double>>);

template std::shared_ptr<LelNode<float>>
lel_reduce(LelFunc, std::shared_ptr<LelNode<float>>);
template std::shared_ptr<LelNode<double>>
lel_reduce(LelFunc, std::shared_ptr<LelNode<double>>);

template std::shared_ptr<LelNode<float>>
lel_bool_reduce(LelFunc, std::shared_ptr<LelNode<bool>>);
template std::shared_ptr<LelNode<double>>
lel_bool_reduce(LelFunc, std::shared_ptr<LelNode<bool>>);

} // namespace casacore_mini
