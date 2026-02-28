#pragma once

/// @file
/// @brief TaQL (Table Query Language) parser, AST, and evaluator.
///
/// Implements the full TaQL command and expression surface compatible with
/// upstream casacore's tables/TaQL module. Supports SELECT, UPDATE, INSERT,
/// DELETE, COUNT, CALC, CREATE TABLE, ALTER TABLE, DROP TABLE, and SHOW
/// commands with complete expression grammar including 220+ built-in functions.

#include <complex>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace casacore_mini {

class Table;

// ---------------------------------------------------------------------------
// TaQL value types
// ---------------------------------------------------------------------------

/// Typed value used in TaQL expression evaluation.
using TaqlValue = std::variant<std::monostate, // null
                               bool,
                               std::int64_t,
                               double,
                               std::complex<double>,
                               std::string,
                               std::vector<bool>,
                               std::vector<std::int64_t>,
                               std::vector<double>,
                               std::vector<std::complex<double>>,
                               std::vector<std::string>>;

// ---------------------------------------------------------------------------
// AST node types
// ---------------------------------------------------------------------------

/// TaQL command type.
enum class TaqlCommand : std::uint8_t {
    select_cmd,
    update_cmd,
    insert_cmd,
    delete_cmd,
    count_cmd,
    calc_cmd,
    create_table_cmd,
    alter_table_cmd,
    drop_table_cmd,
    show_cmd
};

/// Expression node type tag.
enum class ExprType : std::uint8_t {
    literal,        // constant value
    column_ref,     // column name reference
    keyword_ref,    // column::keyword or table keyword
    unary_op,       // unary operator (NOT, -, +, ~)
    binary_op,      // binary operator (+, -, *, /, etc.)
    comparison,     // comparison operator (==, !=, <, >, etc.)
    logical_op,     // AND, OR
    func_call,      // function call
    aggregate_call, // aggregate function (gmin, gmax, etc.)
    in_expr,        // IN set/range test
    between_expr,   // BETWEEN a AND b
    like_expr,      // LIKE pattern
    regex_expr,     // ~ regex match
    exists_expr,    // EXISTS subquery
    subscript,      // array indexing
    set_expr,       // set/range literal
    range_expr,     // range element (start:end or start:end:step)
    unit_expr,      // expression with unit suffix
    iif_expr,       // IIF(cond, true, false)
    cone_expr,      // INCONE / ANYCONE / FINDCONE
    around_expr,    // AROUND a IN b
    wildcard,       // * column projection
    subquery,       // nested SELECT
    record_literal  // [key=val, ...] record literal
};

/// Operator tag for unary/binary operators.
enum class TaqlOp : std::uint8_t {
    // Arithmetic
    plus,
    minus,
    multiply,
    divide,
    int_divide,
    modulo,
    power,
    // Bitwise
    bit_and,
    bit_or,
    bit_xor,
    bit_not,
    // Unary
    negate,
    unary_plus,
    logical_not,
    // Comparison
    eq,
    ne,
    lt,
    le,
    gt,
    ge,
    near,
    not_near,
    // Logical
    logical_and,
    logical_or,
    // Pattern
    like,
    ilike,
    not_like,
    not_ilike,
    regex_match,
    not_regex,
    // Set
    in_set,
    not_in_set,
    // Range
    between,
    not_between,
    around,
    // Special
    incone,
    exists
};

/// Sort order.
enum class SortOrder : std::uint8_t { ascending, descending };

/// Range inclusion (open/closed).
enum class RangeIncl : std::uint8_t { closed, left_open, right_open, open };

// ---------------------------------------------------------------------------
// AST node
// ---------------------------------------------------------------------------

/// A single node in the TaQL expression tree.
struct TaqlExprNode {
    ExprType type;
    TaqlOp op = TaqlOp::plus;          ///< operator for unary/binary/comparison
    TaqlValue value;                     ///< for literals
    std::string name;                    ///< for column_ref, func_call, keyword_ref
    std::vector<TaqlExprNode> children;  ///< sub-expressions / arguments
    std::string unit;                    ///< unit suffix (if any)
    SortOrder sort_order = SortOrder::ascending;
    RangeIncl range_incl = RangeIncl::closed;
    bool distinct = false;               ///< for aggregate DISTINCT

    TaqlExprNode() : type(ExprType::literal) {}
    explicit TaqlExprNode(ExprType t) : type(t) {}
};

/// Sort key (expression + order).
struct TaqlSortKey {
    TaqlExprNode expr;
    SortOrder order = SortOrder::ascending;
};

/// Column specification for CREATE TABLE.
struct TaqlColumnDef {
    std::string name;
    std::string data_type;
    std::optional<int> ndim;
    std::vector<std::int64_t> shape;
    std::string unit;
    std::string comment;
    std::string dm_type;
    std::string dm_group;
};

/// Table creation options from AS clause.
struct TaqlTableOptions {
    std::string type;       // PLAIN, MEMORY, SCRATCH, etc.
    std::string endian;     // BIG, LITTLE, LOCAL, AIPSRC
    std::string storage;    // SEPFILE, MULTIFILE, MULTIHDF5
    bool overwrite = false;
};

/// Column assignment for UPDATE SET.
struct TaqlAssignment {
    std::string column;
    TaqlExprNode value;
    std::vector<TaqlExprNode> indices; // array slicing (optional)
    std::optional<std::string> mask_column;
};

/// ALTER TABLE sub-command.
enum class AlterAction : std::uint8_t {
    add_column,
    copy_column,
    rename_column,
    drop_column,
    set_keyword,
    copy_keyword,
    rename_keyword,
    drop_keyword,
    add_row
};

struct TaqlAlterStep {
    AlterAction action;
    std::vector<TaqlColumnDef> columns;       // for add_column
    std::vector<std::pair<std::string, std::string>> renames; // from->to
    std::vector<std::string> names;           // for drop
    std::vector<std::pair<std::string, TaqlExprNode>> keywords; // for set_keyword
    TaqlExprNode row_count;                   // for add_row
};

/// Table reference (name or subquery).
struct TaqlTableRef {
    std::string name;                        // table path or name
    std::string shorthand;                   // alias (t0, t1, etc.)
    std::optional<TaqlExprNode> subquery_ast; // if FROM (subquery)
};

/// Projection column (expression AS alias).
struct TaqlProjection {
    TaqlExprNode expr;
    std::string alias;
    std::string data_type; // optional output type
};

/// Join specification.
struct TaqlJoin {
    TaqlTableRef table;
    TaqlExprNode on_expr;
};

// ---------------------------------------------------------------------------
// Top-level AST
// ---------------------------------------------------------------------------

/// Complete parsed TaQL command.
struct TaqlAst {
    TaqlCommand command = TaqlCommand::select_cmd;

    // SELECT / COUNT
    std::vector<TaqlProjection> projections;
    std::vector<TaqlTableRef> tables;       // FROM
    std::vector<TaqlTableRef> with_tables;  // WITH
    std::vector<TaqlJoin> joins;
    std::optional<TaqlExprNode> where_expr;
    std::vector<TaqlExprNode> group_by;
    std::optional<TaqlExprNode> having_expr;
    std::vector<TaqlSortKey> order_by;
    std::optional<TaqlExprNode> limit_expr;
    std::optional<TaqlExprNode> offset_expr;
    std::optional<TaqlTableRef> into_table;
    std::optional<TaqlTableRef> giving_table;
    TaqlTableOptions table_options;
    bool select_distinct = false;
    bool order_distinct = false;
    bool has_rollup = false;

    // UPDATE
    std::vector<TaqlAssignment> assignments;

    // INSERT
    std::vector<std::vector<TaqlExprNode>> insert_values; // VALUES rows
    std::vector<std::string> insert_columns;

    // CALC
    std::optional<TaqlExprNode> calc_expr;

    // CREATE TABLE
    std::vector<TaqlColumnDef> create_columns;
    std::string like_table;
    std::vector<std::string> like_drop_columns;

    // ALTER TABLE
    std::vector<TaqlAlterStep> alter_steps;

    // DROP TABLE
    std::vector<std::string> drop_tables;

    // SHOW
    std::string show_topic;

    // DMINFO
    std::vector<TaqlExprNode> dminfo;

    // Style/timing
    std::string style;
    bool timing = false;
};

// ---------------------------------------------------------------------------
// Parser diagnostics
// ---------------------------------------------------------------------------

/// Source location for error reporting.
struct TaqlSourceLoc {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

/// Parse error with location context.
struct TaqlParseError {
    std::string message;
    TaqlSourceLoc location;
    std::string token;
    std::string category;
};

// ---------------------------------------------------------------------------
// Parser API
// ---------------------------------------------------------------------------

/// Parse a TaQL query string into an AST.
/// @param query The TaQL query string.
/// @return The parsed AST.
/// @throws std::runtime_error on parse failure with diagnostic details.
[[nodiscard]] TaqlAst taql_parse(std::string_view query);

/// Format a TaQL parse error as a human-readable string.
[[nodiscard]] std::string format_parse_error(const TaqlParseError& error,
                                              std::string_view source);

// ---------------------------------------------------------------------------
// SHOW command
// ---------------------------------------------------------------------------

/// Get TaQL help text for a topic.
/// @param topic Help topic (empty for overview).
/// @return Help text string.
[[nodiscard]] std::string taql_show(std::string_view topic = "");

// ---------------------------------------------------------------------------
// Evaluator API (populated in W4)
// ---------------------------------------------------------------------------

/// Result of executing a TaQL command.
struct TaqlResult {
    /// Selected/affected row indices.
    std::vector<std::uint64_t> rows;
    /// Column values for CALC or scalar results.
    std::vector<TaqlValue> values;
    /// Column names for SELECT results.
    std::vector<std::string> column_names;
    /// Number of affected rows (for UPDATE/INSERT/DELETE).
    std::uint64_t affected_rows = 0;
    /// Output table path (for SELECT INTO / CREATE TABLE).
    std::string output_table;
    /// SHOW text output.
    std::string show_text;
};

/// Execute a TaQL command against a table.
/// @param query TaQL query string.
/// @param table The table to operate on.
/// @return Query result.
/// @throws std::runtime_error on execution failure.
[[nodiscard]] TaqlResult taql_execute(std::string_view query, Table& table);

/// Execute a TaQL CALC expression (no table context).
/// @param query TaQL CALC expression.
/// @return Calculated values.
[[nodiscard]] TaqlResult taql_calc(std::string_view query);

} // namespace casacore_mini
