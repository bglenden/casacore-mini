// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/lattice_array.hpp"
#include "casacore_mini/lattice_shape.hpp"

#include <cstddef>
#include <cstdint>
#include <complex>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Lattice Expression Language (LEL) evaluator engine.

/// <synopsis>
/// This header implements the Lattice Expression Language (LEL) evaluator,
/// a typed expression-tree engine that evaluates element-wise operations over
/// `LatticeArray` values.  It mirrors the role of casacore's `LatticeExpr`
/// and `LatticeExprNode` but is built on modern C++ templates and
/// `std::variant` for type erasure.
///
/// Architecture overview:
/// - `LelNode<T>` — abstract base; each node knows its result type and shape
///   and can evaluate itself via `eval()`.
/// - `LelScalar<T>` — scalar constant leaf node.
/// - `LelArrayRef<T>` — reference to an existing `LatticeArray` (leaf node).
/// - `LelBinary<T>` / `LelCompare<T>` — binary arithmetic and comparison.
/// - `LelUnary<T>` — unary negation / logical NOT.
/// - `LelFunc1<T>` / `LelFunc2<T>` — element-wise math functions.
/// - `LelReduce<T>` / `LelBoolReduce<R>` — whole-lattice reductions to scalar.
/// - `LelIif<T>` — element-wise conditional.
/// - Complex operation nodes: `LelReal`, `LelImag`, `LelConj`, `LelArg`,
///   `LelFormComplex`, `LelComplexAbs`.
/// - `LelValueExtract<T>` / `LelMaskExtract<T>` — mask-handling extractors.
/// - `LelPromoteFloat` — implicit float-to-double widening.
/// - `LatticeExprNode` — type-erased wrapper holding any `LelNode<T>`.
/// - `LatticeExpr<T>` — typed expression that can be evaluated.
/// - `LelSymbolTable` — name-to-node mapping for the text parser.
/// - `LelParseError` — exception type from the parser.
/// - `lel_parse()` — parses a LEL expression string into a `LatticeExprNode`.
///
/// Builder helpers (`lel_scalar`, `lel_array`, `lel_add`, etc.) make it
/// convenient to construct expression trees programmatically without dealing
/// directly with `std::make_shared`.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   // Programmatic: compute rms of a float lattice
///   auto arr = lel_array(my_lattice_array);
///   auto sq  = lel_mul(arr, arr);
///   auto s   = lel_reduce<float>(LelFunc::mean_r, sq);
///   LatticeExpr<float> expr(s);
///   auto result = expr.eval();  // result.values is a scalar
///
///   // Parser: evaluate a LEL expression string
///   LelSymbolTable syms;
///   syms.define("img", lel_array(my_array));
///   auto node = lel_parse("sqrt(img * img + 1.0)", syms);
/// </srcblock>
/// </example>
///
/// <motivation>
/// casacore's LEL is a complex C++ template library spanning dozens of files.
/// This reimplementation provides the subset of LEL needed for CASA image
/// arithmetic within a single header, using C++20 features (concepts,
/// if-constexpr, std::variant) to reduce boilerplate.
/// </motivation>

// ── Value types ──────────────────────────────────────────────────────

/// The scalar types supported in LEL expressions.
enum class LelType : std::uint8_t {
    float32,      ///< float
    float64,      ///< double
    complex64,    ///< std::complex<float>
    complex128,   ///< std::complex<double>
    boolean,      ///< bool
};

/// Result of evaluating an expression chunk: array + optional mask.
template <typename T>
struct LelResult {
    LatticeArray<T> values;
    std::optional<LatticeArray<bool>> mask; ///< nullopt = all unmasked.
};

// ── LelNode (abstract base) ──────────────────────────────────────────

/// <summary>
/// Abstract base class for LEL expression tree nodes.
/// </summary>
///
/// <use visibility=local/>
///
/// <synopsis>
/// Each `LelNode<T>` produces a `LelResult<T>` when `eval()` is called.
/// The node knows its result shape (or `std::nullopt` if it is a scalar)
/// and whether it is scalar (`is_scalar()`).
///
/// Leaf nodes hold constant values or references to existing lattice data.
/// Interior nodes combine child nodes using operators, functions, or
/// reductions.
/// </synopsis>
template <typename T>
class LelNode {
  public:
    virtual ~LelNode() = default;

    /// The shape of this node's result, or nullopt if scalar.
    [[nodiscard]] virtual std::optional<IPosition> shape() const = 0;

    /// Whether this node is a scalar (shapeless).
    [[nodiscard]] bool is_scalar() const { return !shape().has_value(); }

    /// Evaluate the expression and return the result.
    [[nodiscard]] virtual LelResult<T> eval() const = 0;
};

// ── LelScalar ────────────────────────────────────────────────────────

/// Scalar constant expression node.
template <typename T>
class LelScalar : public LelNode<T> {
  public:
    explicit LelScalar(T value) : value_(value) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return std::nullopt;
    }

    [[nodiscard]] LelResult<T> eval() const override {
        LatticeArray<T> arr(IPosition{1}, value_);
        return {std::move(arr), std::nullopt};
    }

    [[nodiscard]] T value() const { return value_; }

  private:
    T value_;
};

// ── LelArrayRef ──────────────────────────────────────────────────────

/// Reference to an existing LatticeArray (leaf node).
template <typename T>
class LelArrayRef : public LelNode<T> {
  public:
    LelArrayRef(LatticeArray<T> data,
                std::optional<LatticeArray<bool>> mask = std::nullopt)
        : data_(std::move(data)), mask_(std::move(mask)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return data_.shape();
    }

    [[nodiscard]] LelResult<T> eval() const override {
        return {data_, mask_};
    }

  private:
    LatticeArray<T> data_;
    std::optional<LatticeArray<bool>> mask_;
};

// ── Binary operations ────────────────────────────────────────────────

/// Binary operator enumeration.
enum class LelBinaryOp : std::uint8_t {
    add, sub, mul, div_op,
    eq, ne, gt, ge, lt, le,
    logical_and, logical_or,
};

/// Return the string representation of a binary op.
[[nodiscard]] const char* lel_binary_op_name(LelBinaryOp op);

// ── Unary operations ─────────────────────────────────────────────────

/// Unary operator enumeration.
enum class LelUnaryOp : std::uint8_t {
    negate, identity,
    logical_not,
};

// ── Function enumeration ─────────────────────────────────────────────

/// Built-in function enumeration.
enum class LelFunc : std::uint8_t {
    // Math (1-arg)
    sin_f, cos_f, tan_f,
    asin_f, acos_f, atan_f,
    exp_f, log_f, log10_f, sqrt_f,
    abs_f, sign_f, round_f, ceil_f, floor_f,
    // Math (2-arg)
    atan2_f, pow_f, fmod_f, min2_f, max2_f,
    // Complex
    real_f, imag_f, conj_f, arg_f,
    form_complex_f,
    // Reductions (0-d result)
    min_r, max_r, sum_r, mean_r, median_r,
    variance_r, stddev_r,
    ntrue_r, nfalse_r, any_r, all_r,
    // Mask operations
    value_f, mask_f,
    // Conditional
    iif_f,
};

// ── Binary expression node ───────────────────────────────────────────

/// Binary operation on two expression trees of the same type.
/// Handles scalar-lattice broadcasting.
template <typename T>
class LelBinary : public LelNode<T> {
  public:
    LelBinary(LelBinaryOp op,
              std::shared_ptr<LelNode<T>> left,
              std::shared_ptr<LelNode<T>> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override;
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    LelBinaryOp op_;
    std::shared_ptr<LelNode<T>> left_;
    std::shared_ptr<LelNode<T>> right_;
};

/// Binary comparison node: two T operands → bool result.
template <typename T>
class LelCompare : public LelNode<bool> {
  public:
    LelCompare(LelBinaryOp op,
               std::shared_ptr<LelNode<T>> left,
               std::shared_ptr<LelNode<T>> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override;
    [[nodiscard]] LelResult<bool> eval() const override;

  private:
    LelBinaryOp op_;
    std::shared_ptr<LelNode<T>> left_;
    std::shared_ptr<LelNode<T>> right_;
};

// ── Unary expression node ────────────────────────────────────────────

/// Unary operation on an expression tree.
template <typename T>
class LelUnary : public LelNode<T> {
  public:
    LelUnary(LelUnaryOp op, std::shared_ptr<LelNode<T>> operand)
        : op_(op), operand_(std::move(operand)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return operand_->shape();
    }
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    LelUnaryOp op_;
    std::shared_ptr<LelNode<T>> operand_;
};

// ── Function expression node ─────────────────────────────────────────

/// 1-argument math function applied element-wise.
template <typename T>
class LelFunc1 : public LelNode<T> {
  public:
    using func_type = std::function<T(T)>;

    LelFunc1(LelFunc id, func_type fn, std::shared_ptr<LelNode<T>> arg)
        : id_(id), fn_(std::move(fn)), arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    LelFunc id_;
    func_type fn_;
    std::shared_ptr<LelNode<T>> arg_;
};

/// 2-argument math function applied element-wise.
template <typename T>
class LelFunc2 : public LelNode<T> {
  public:
    using func_type = std::function<T(T, T)>;

    LelFunc2(LelFunc id, func_type fn,
             std::shared_ptr<LelNode<T>> arg1,
             std::shared_ptr<LelNode<T>> arg2)
        : id_(id), fn_(std::move(fn)),
          arg1_(std::move(arg1)), arg2_(std::move(arg2)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override;
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    LelFunc id_;
    func_type fn_;
    std::shared_ptr<LelNode<T>> arg1_;
    std::shared_ptr<LelNode<T>> arg2_;
};

// ── Reduction node ───────────────────────────────────────────────────

/// Reduction that collapses a lattice expression to a scalar.
template <typename T>
class LelReduce : public LelNode<T> {
  public:
    using reduce_type = std::function<T(const LatticeArray<T>&,
                                        const std::optional<LatticeArray<bool>>&)>;

    LelReduce(LelFunc id, reduce_type fn, std::shared_ptr<LelNode<T>> arg)
        : id_(id), fn_(std::move(fn)), arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return std::nullopt; // scalar result
    }
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    LelFunc id_;
    reduce_type fn_;
    std::shared_ptr<LelNode<T>> arg_;
};

/// Bool reduction (ntrue, nfalse, any, all) → float/double scalar result.
template <typename R>
class LelBoolReduce : public LelNode<R> {
  public:
    using reduce_type = std::function<R(const LatticeArray<bool>&,
                                        const std::optional<LatticeArray<bool>>&)>;

    LelBoolReduce(LelFunc id, reduce_type fn, std::shared_ptr<LelNode<bool>> arg)
        : id_(id), fn_(std::move(fn)), arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return std::nullopt;
    }
    [[nodiscard]] LelResult<R> eval() const override;

  private:
    LelFunc id_;
    reduce_type fn_;
    std::shared_ptr<LelNode<bool>> arg_;
};

// ── Conditional (iif) ────────────────────────────────────────────────

/// iif(condition, true_val, false_val)
template <typename T>
class LelIif : public LelNode<T> {
  public:
    LelIif(std::shared_ptr<LelNode<bool>> cond,
           std::shared_ptr<LelNode<T>> true_val,
           std::shared_ptr<LelNode<T>> false_val)
        : cond_(std::move(cond)),
          true_val_(std::move(true_val)),
          false_val_(std::move(false_val)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override;
    [[nodiscard]] LelResult<T> eval() const override;

  private:
    std::shared_ptr<LelNode<bool>> cond_;
    std::shared_ptr<LelNode<T>> true_val_;
    std::shared_ptr<LelNode<T>> false_val_;
};

// ── LatticeExprNode (type-erased wrapper) ────────────────────────────

/// <summary>
/// Type-erased expression node holding one of the typed LelNode<T> trees.
/// </summary>
///
/// <use visibility=export/>
///
/// <synopsis>
/// `LatticeExprNode` stores a `std::variant` over the five supported typed
/// node pointer types (`float`, `double`, `complex<float>`, `complex<double>`,
/// `bool`).  It exposes the `result_type()` discriminant and a typed accessor
/// `as<T>()` that returns the contained `shared_ptr<LelNode<T>>` or throws if
/// the type does not match.
///
/// `LatticeExprNode` is the return type of `lel_parse()` and the value type
/// stored in `LelSymbolTable`.
/// </synopsis>
class LatticeExprNode {
  public:
    LatticeExprNode() = default;

    template <typename T>
    explicit LatticeExprNode(std::shared_ptr<LelNode<T>> node)
        : type_(type_of<T>()), node_(std::move(node)) {}

    [[nodiscard]] LelType result_type() const { return type_; }

    template <typename T>
    [[nodiscard]] std::shared_ptr<LelNode<T>> as() const {
        return std::get<std::shared_ptr<LelNode<T>>>(node_);
    }

    [[nodiscard]] bool is_valid() const {
        return std::visit([](const auto& p) { return p != nullptr; }, node_);
    }

  private:
    template <typename T>
    static constexpr LelType type_of() {
        if constexpr (std::is_same_v<T, float>) return LelType::float32;
        else if constexpr (std::is_same_v<T, double>) return LelType::float64;
        else if constexpr (std::is_same_v<T, std::complex<float>>) return LelType::complex64;
        else if constexpr (std::is_same_v<T, std::complex<double>>) return LelType::complex128;
        else if constexpr (std::is_same_v<T, bool>) return LelType::boolean;
        else static_assert(sizeof(T) == 0, "unsupported LEL type");
    }

    LelType type_ = LelType::float32;
    std::variant<
        std::shared_ptr<LelNode<float>>,
        std::shared_ptr<LelNode<double>>,
        std::shared_ptr<LelNode<std::complex<float>>>,
        std::shared_ptr<LelNode<std::complex<double>>>,
        std::shared_ptr<LelNode<bool>>
    > node_{std::shared_ptr<LelNode<float>>(nullptr)};
};

// ── LatticeExpr<T> ──────────────────────────────────────────────────

/// <summary>
/// Typed lattice expression that can be evaluated.
/// </summary>
///
/// <use visibility=export/>
///
/// <synopsis>
/// `LatticeExpr<T>` wraps a `shared_ptr<LelNode<T>>` root and provides
/// `shape()` and `eval()` as the primary user-facing interface.  It is
/// analogous to casacore's `LatticeExpr<T>`.
///
/// Call `eval()` to obtain a `LelResult<T>` containing the computed values
/// and optional mask.
/// </synopsis>
template <typename T>
class LatticeExpr {
  public:
    LatticeExpr() = default;
    explicit LatticeExpr(std::shared_ptr<LelNode<T>> root)
        : root_(std::move(root)) {}

    /// The shape of the expression result, or nullopt if scalar.
    [[nodiscard]] std::optional<IPosition> shape() const {
        return root_ ? root_->shape() : std::nullopt;
    }

    /// Evaluate the full expression tree.
    [[nodiscard]] LelResult<T> eval() const {
        if (!root_) throw std::runtime_error("LatticeExpr: null root");
        return root_->eval();
    }

    /// Access the root node.
    [[nodiscard]] const std::shared_ptr<LelNode<T>>& root() const { return root_; }

  private:
    std::shared_ptr<LelNode<T>> root_;
};

// ── Builder helpers ──────────────────────────────────────────────────

/// Create a scalar constant expression.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>> lel_scalar(T value) {
    return std::make_shared<LelScalar<T>>(value);
}

/// Create a lattice reference expression.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_array(LatticeArray<T> data,
          std::optional<LatticeArray<bool>> mask = std::nullopt) {
    return std::make_shared<LelArrayRef<T>>(std::move(data), std::move(mask));
}

/// Create arithmetic binary expressions.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_add(std::shared_ptr<LelNode<T>> l, std::shared_ptr<LelNode<T>> r) {
    return std::make_shared<LelBinary<T>>(LelBinaryOp::add, std::move(l), std::move(r));
}

template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_sub(std::shared_ptr<LelNode<T>> l, std::shared_ptr<LelNode<T>> r) {
    return std::make_shared<LelBinary<T>>(LelBinaryOp::sub, std::move(l), std::move(r));
}

template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_mul(std::shared_ptr<LelNode<T>> l, std::shared_ptr<LelNode<T>> r) {
    return std::make_shared<LelBinary<T>>(LelBinaryOp::mul, std::move(l), std::move(r));
}

template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_div(std::shared_ptr<LelNode<T>> l, std::shared_ptr<LelNode<T>> r) {
    return std::make_shared<LelBinary<T>>(LelBinaryOp::div_op, std::move(l), std::move(r));
}

/// Create comparison expressions (result is bool).
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<bool>>
lel_compare(LelBinaryOp op,
            std::shared_ptr<LelNode<T>> l,
            std::shared_ptr<LelNode<T>> r) {
    return std::make_shared<LelCompare<T>>(op, std::move(l), std::move(r));
}

/// Create unary negate expression.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_negate(std::shared_ptr<LelNode<T>> operand) {
    return std::make_shared<LelUnary<T>>(LelUnaryOp::negate, std::move(operand));
}

/// Create logical NOT expression (bool only).
[[nodiscard]] inline std::shared_ptr<LelNode<bool>>
lel_not(std::shared_ptr<LelNode<bool>> operand) {
    return std::make_shared<LelUnary<bool>>(LelUnaryOp::logical_not, std::move(operand));
}

/// Create logical AND/OR expressions.
[[nodiscard]] inline std::shared_ptr<LelNode<bool>>
lel_and(std::shared_ptr<LelNode<bool>> l, std::shared_ptr<LelNode<bool>> r) {
    return std::make_shared<LelBinary<bool>>(LelBinaryOp::logical_and,
                                              std::move(l), std::move(r));
}

[[nodiscard]] inline std::shared_ptr<LelNode<bool>>
lel_or(std::shared_ptr<LelNode<bool>> l, std::shared_ptr<LelNode<bool>> r) {
    return std::make_shared<LelBinary<bool>>(LelBinaryOp::logical_or,
                                              std::move(l), std::move(r));
}

/// Create iif conditional.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_iif(std::shared_ptr<LelNode<bool>> cond,
        std::shared_ptr<LelNode<T>> true_val,
        std::shared_ptr<LelNode<T>> false_val) {
    return std::make_shared<LelIif<T>>(std::move(cond),
                                        std::move(true_val),
                                        std::move(false_val));
}

// ── Standard math function factories ─────────────────────────────────

/// Create standard 1-arg math function nodes.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_math1(LelFunc id, std::shared_ptr<LelNode<T>> arg);

/// Create standard 2-arg math function nodes.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_math2(LelFunc id, std::shared_ptr<LelNode<T>> arg1,
          std::shared_ptr<LelNode<T>> arg2);

/// Create reduction node.
template <typename T>
[[nodiscard]] std::shared_ptr<LelNode<T>>
lel_reduce(LelFunc id, std::shared_ptr<LelNode<T>> arg);

/// Create bool reduction node (ntrue, nfalse, any, all).
template <typename R>
[[nodiscard]] std::shared_ptr<LelNode<R>>
lel_bool_reduce(LelFunc id, std::shared_ptr<LelNode<bool>> arg);

// ── value() / mask() extractors ──────────────────────────────────────

/// value(expr): strips the mask and returns just the values.
template <typename T>
class LelValueExtract : public LelNode<T> {
  public:
    explicit LelValueExtract(std::shared_ptr<LelNode<T>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<T> eval() const override {
        auto r = arg_->eval();
        return {std::move(r.values), std::nullopt}; // discard mask
    }

  private:
    std::shared_ptr<LelNode<T>> arg_;
};

/// mask(expr): extracts the mask as a Bool lattice (true=unmasked).
template <typename T>
class LelMaskExtract : public LelNode<bool> {
  public:
    explicit LelMaskExtract(std::shared_ptr<LelNode<T>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<bool> eval() const override;

  private:
    std::shared_ptr<LelNode<T>> arg_;
};

/// Create value() extractor.
template <typename T>
[[nodiscard]] inline std::shared_ptr<LelNode<T>>
lel_value(std::shared_ptr<LelNode<T>> arg) {
    return std::make_shared<LelValueExtract<T>>(std::move(arg));
}

/// Create mask() extractor.
template <typename T>
[[nodiscard]] inline std::shared_ptr<LelNode<bool>>
lel_mask(std::shared_ptr<LelNode<T>> arg) {
    return std::make_shared<LelMaskExtract<T>>(std::move(arg));
}

// ── Complex operations ───────────────────────────────────────────────

/// Extract real part from complex lattice.
template <typename R>
class LelReal : public LelNode<R> {
  public:
    explicit LelReal(std::shared_ptr<LelNode<std::complex<R>>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<R> eval() const override;

  private:
    std::shared_ptr<LelNode<std::complex<R>>> arg_;
};

/// Extract imaginary part from complex lattice.
template <typename R>
class LelImag : public LelNode<R> {
  public:
    explicit LelImag(std::shared_ptr<LelNode<std::complex<R>>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<R> eval() const override;

  private:
    std::shared_ptr<LelNode<std::complex<R>>> arg_;
};

/// Conjugate of complex lattice.
template <typename R>
class LelConj : public LelNode<std::complex<R>> {
  public:
    explicit LelConj(std::shared_ptr<LelNode<std::complex<R>>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<std::complex<R>> eval() const override;

  private:
    std::shared_ptr<LelNode<std::complex<R>>> arg_;
};

/// arg() (phase angle) of complex lattice.
template <typename R>
class LelArg : public LelNode<R> {
  public:
    explicit LelArg(std::shared_ptr<LelNode<std::complex<R>>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<R> eval() const override;

  private:
    std::shared_ptr<LelNode<std::complex<R>>> arg_;
};

/// formComplex(re, im): construct complex from two real lattices.
template <typename R>
class LelFormComplex : public LelNode<std::complex<R>> {
  public:
    LelFormComplex(std::shared_ptr<LelNode<R>> re,
                   std::shared_ptr<LelNode<R>> im)
        : re_(std::move(re)), im_(std::move(im)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override;
    [[nodiscard]] LelResult<std::complex<R>> eval() const override;

  private:
    std::shared_ptr<LelNode<R>> re_;
    std::shared_ptr<LelNode<R>> im_;
};

/// Complex abs (magnitude).
template <typename R>
class LelComplexAbs : public LelNode<R> {
  public:
    explicit LelComplexAbs(std::shared_ptr<LelNode<std::complex<R>>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<R> eval() const override;

  private:
    std::shared_ptr<LelNode<std::complex<R>>> arg_;
};

/// Complex arithmetic binary (using the generic LelBinary<complex<R>>).
/// Explicit instantiations provided for complex<float> and complex<double>.

// Builder helpers for complex operations.
template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<R>>
lel_real(std::shared_ptr<LelNode<std::complex<R>>> arg) {
    return std::make_shared<LelReal<R>>(std::move(arg));
}

template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<R>>
lel_imag(std::shared_ptr<LelNode<std::complex<R>>> arg) {
    return std::make_shared<LelImag<R>>(std::move(arg));
}

template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<std::complex<R>>>
lel_conj(std::shared_ptr<LelNode<std::complex<R>>> arg) {
    return std::make_shared<LelConj<R>>(std::move(arg));
}

template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<R>>
lel_arg(std::shared_ptr<LelNode<std::complex<R>>> arg) {
    return std::make_shared<LelArg<R>>(std::move(arg));
}

template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<R>>
lel_complex_abs(std::shared_ptr<LelNode<std::complex<R>>> arg) {
    return std::make_shared<LelComplexAbs<R>>(std::move(arg));
}

template <typename R>
[[nodiscard]] inline std::shared_ptr<LelNode<std::complex<R>>>
lel_form_complex(std::shared_ptr<LelNode<R>> re,
                 std::shared_ptr<LelNode<R>> im) {
    return std::make_shared<LelFormComplex<R>>(std::move(re), std::move(im));
}

// ── Float→Double promotion ───────────────────────────────────────────

/// Promote a float node to double.
class LelPromoteFloat : public LelNode<double> {
  public:
    explicit LelPromoteFloat(std::shared_ptr<LelNode<float>> arg)
        : arg_(std::move(arg)) {}

    [[nodiscard]] std::optional<IPosition> shape() const override {
        return arg_->shape();
    }
    [[nodiscard]] LelResult<double> eval() const override;

  private:
    std::shared_ptr<LelNode<float>> arg_;
};

[[nodiscard]] inline std::shared_ptr<LelNode<double>>
lel_promote_float(std::shared_ptr<LelNode<float>> arg) {
    return std::make_shared<LelPromoteFloat>(std::move(arg));
}

// ── LEL Parser ───────────────────────────────────────────────────────

/// <summary>
/// Error thrown when LEL expression parsing fails.
/// </summary>
///
/// <use visibility=export/>
class LelParseError : public std::runtime_error {
  public:
    LelParseError(const std::string& msg, std::size_t pos)
        : std::runtime_error("LEL parse error at position " +
                             std::to_string(pos) + ": " + msg),
          position_(pos) {}

    [[nodiscard]] std::size_t position() const { return position_; }

  private:
    std::size_t position_;
};

/// <summary>
/// Named lattice variable reference table for the LEL parser.
/// </summary>
///
/// <use visibility=export/>
///
/// <synopsis>
/// `LelSymbolTable` maps lattice variable names to type-erased
/// `LatticeExprNode` values.  Populate it before calling `lel_parse()` so
/// that lattice identifiers in the expression string resolve to the correct
/// typed expression nodes.
/// </synopsis>
class LelSymbolTable {
  public:
    template <typename T>
    void define(const std::string& name, std::shared_ptr<LelNode<T>> node) {
        symbols_[name] = LatticeExprNode(std::move(node));
    }

    [[nodiscard]] bool has(const std::string& name) const {
        return symbols_.count(name) > 0;
    }

    [[nodiscard]] const LatticeExprNode& get(const std::string& name) const {
        auto it = symbols_.find(name);
        if (it == symbols_.end()) {
            throw std::runtime_error(
                "LEL: undefined lattice reference '" + name + "'");
        }
        return it->second;
    }

  private:
    std::unordered_map<std::string, LatticeExprNode> symbols_;
};

/// Parse a LEL expression string into a type-erased expression node.
///
/// <synopsis>
/// Supported syntax:
/// - Numeric literals: <src>1.5</src>, <src>-2.0</src>, <src>3e10</src>
/// - Boolean literals: <src>T</src>, <src>F</src>
/// - Lattice references: identifier names looked up in the symbol table
/// - Arithmetic: <src>+</src>, <src>-</src>, <src>*</src>, <src>/</src>
/// - Comparison: <src>==</src>, <src>!=</src>, <src>></src>, <src>>=</src>,
///   <src><</src>, <src><=</src>
/// - Logical: <src>&&</src>, <src>||</src>, <src>!</src>
/// - Parentheses: <src>(expr)</src>
/// - Functions: <src>sin(x)</src>, <src>sqrt(x)</src>, <src>pow(x,y)</src>,
///   <src>min(x)</src>, <src>median(x)</src>, etc.
/// - Extractors: <src>value(expr)</src>, <src>mask(expr)</src>
/// - Conditional: <src>iif(cond, true, false)</src>
/// </synopsis>
///
/// @throws LelParseError on malformed input.
[[nodiscard]] LatticeExprNode
lel_parse(const std::string& expr, const LelSymbolTable& symbols);

} // namespace casacore_mini
