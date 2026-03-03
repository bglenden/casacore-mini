// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

/// @file
/// @brief TaQL (Table Query Language) parser, AST, and evaluator.
///
///
///
///
/// Implements the full TaQL command and expression surface compatible with
/// upstream casacore's tables/TaQL module.  Supported top-level commands are:
///
///   SELECT, COUNT, UPDATE, INSERT, DELETE, CALC,
///   CREATE TABLE, ALTER TABLE, DROP TABLE, SHOW
///
/// The parser converts a TaQL query string into a TaqlAst value.  The
/// evaluator (`taql_execute`) applies that AST against one or more
/// Table objects and returns a TaqlResult.  A standalone scalar evaluator
/// (`taql_calc`) handles CALC expressions with no table context.
///
/// Expression grammar covers 220+ built-in functions, all arithmetic,
/// comparison, logical, bitwise, pattern-match, range, and set operators,
/// aggregate functions, array subscripting, unit suffixes, cone-search
/// primitives, IIF, record literals, and nested subqueries.
///
///
/// @par Example
/// @code{.cpp}
///   // Parse and inspect
///   auto ast = taql_parse("SELECT TIME, DATA FROM my.ms WHERE ANTENNA1=0");
///   assert(ast.command == TaqlCommand::select_cmd);
///   assert(ast.projections.size() == 2);
///
///   // Execute against a table
///   Table tbl("my.ms");
///   auto result = taql_execute("SELECT TIME FROM my.ms WHERE ANTENNA1=0", tbl);
///   for (auto row : result.rows) { /* process */ }
///
///   // Pure CALC (no table)
///   auto calc = taql_calc("CALC 2*pi()*1.5GHz");
///   double freq_rad_per_s = std::get<double>(calc.values[0]);
/// @endcode
///
/// @ingroup tables

#include <complex>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace casacore_mini {
/// @addtogroup tables
/// @{

class Table;

// ---------------------------------------------------------------------------
// TaQL value types
// ---------------------------------------------------------------------------

/// Typed value used in TaQL expression evaluation.
///
///
/// TaqlValue is a variant covering all scalar and array types that TaQL
/// expressions can produce.  The `std::monostate` alternative
/// represents a SQL-style NULL.  Scalar alternatives are `bool`,
/// `std::int64_t`, `double`, `std::complex<double>`,
/// and `std::string`.  Each scalar type has a corresponding
/// `std::vector` alternative for array-valued results.
///
using TaqlValue = std::variant<std::monostate, // null
                               bool, std::int64_t, double, std::complex<double>, std::string,
                               std::vector<bool>, std::vector<std::int64_t>, std::vector<double>,
                               std::vector<std::complex<double>>, std::vector<std::string>>;

// ---------------------------------------------------------------------------
// AST node types
// ---------------------------------------------------------------------------

/// TaQL command type.
///
///
/// Discriminates the top-level TaQL statement stored in a TaqlAst.  The
/// active fields of TaqlAst depend on which command variant is present.
///
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
///
///
/// Every node in a TaqlExprNode tree carries one of these tags to identify
/// the kind of expression it represents.  The tag controls which fields of
/// TaqlExprNode are meaningful:
/// <dl>
///   <dt>literal</dt>       <dd>TaqlExprNode::value holds the constant.</dd>
///   <dt>column_ref</dt>    <dd>TaqlExprNode::name holds the column name.</dd>
///   <dt>keyword_ref</dt>   <dd>TaqlExprNode::name holds "col::kw" or just "kw".</dd>
///   <dt>unary_op</dt>      <dd>TaqlExprNode::op + one child.</dd>
///   <dt>binary_op</dt>     <dd>TaqlExprNode::op + two children.</dd>
///   <dt>comparison</dt>    <dd>TaqlExprNode::op + two children.</dd>
///   <dt>logical_op</dt>    <dd>TaqlExprNode::op + two children.</dd>
///   <dt>func_call</dt>     <dd>TaqlExprNode::name + children as arguments.</dd>
///   <dt>aggregate_call</dt><dd>TaqlExprNode::name + children + distinct flag.</dd>
///   <dt>in_expr</dt>       <dd>children[0]=test value, children[1..]=set elements.</dd>
///   <dt>between_expr</dt>  <dd>children: [value, lower, upper].</dd>
///   <dt>subscript</dt>     <dd>children[0]=array expr, rest=index expressions.</dd>
///   <dt>unit_expr</dt>     <dd>TaqlExprNode::unit holds the suffix string.</dd>
///   <dt>subquery</dt>      <dd>children[0] wraps a nested TaqlAst.</dd>
/// </dl>
///
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
///
///
/// Used in TaqlExprNode::op to identify arithmetic, bitwise, comparison,
/// logical, pattern-match, set, range, and special operators.  The tag is
/// only meaningful when ExprType is one of `unary_op`,
/// `binary_op`, `comparison`, `logical_op`,
/// `like_expr`, `regex_expr`, `in_expr`,
/// `between_expr`, `around_expr`, or
/// `exists_expr`.
///
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

/// Sort order for ORDER BY clauses.
enum class SortOrder : std::uint8_t { ascending, descending };

/// Range inclusion (open/closed) for range expressions.
///
///
/// Identifies which endpoints of a `start:end` range are included
/// in the interval.  Maps directly to the bracket notation:
///   `closed` = [a,b], `left_open` = (a,b],
///   `right_open` = [a,b), `open` = (a,b).
///
enum class RangeIncl : std::uint8_t { closed, left_open, right_open, open };

// ---------------------------------------------------------------------------
// AST node
// ---------------------------------------------------------------------------

/// A single node in the TaQL expression tree.
///
///
/// TaqlExprNode is the recursive building block of all TaQL expressions.
/// Each node carries a type tag (ExprType), an operator tag (TaqlOp, for
/// operator nodes), a value (TaqlValue, for literals), a name string (for
/// column references and function calls), and zero or more child nodes for
/// sub-expressions and function arguments.
///
/// The `unit` field carries a physical unit suffix string when
/// ExprType is `unit_expr`.  The `sort_order` and
/// `range_incl` fields are used by ORDER BY sort keys and range
/// literals respectively.  The `distinct` flag applies to
/// aggregate function calls.
///
struct TaqlExprNode {
    ExprType type;
    TaqlOp op = TaqlOp::plus;           ///< operator for unary/binary/comparison
    TaqlValue value;                    ///< for literals
    std::string name;                   ///< for column_ref, func_call, keyword_ref
    std::vector<TaqlExprNode> children; ///< sub-expressions / arguments
    std::string unit;                   ///< unit suffix (if any)
    SortOrder sort_order = SortOrder::ascending;
    RangeIncl range_incl = RangeIncl::closed;
    bool distinct = false; ///< for aggregate DISTINCT

    TaqlExprNode() : type(ExprType::literal) {}
    explicit TaqlExprNode(ExprType t) : type(t) {}
};

/// Sort key (expression + order) for ORDER BY clauses.
struct TaqlSortKey {
    TaqlExprNode expr;
    SortOrder order = SortOrder::ascending;
};

/// Column specification for CREATE TABLE statements.
///
///
/// Describes a single column to be created.  `data_type` is the
/// TaQL type keyword (e.g. "double", "complex", "string").  `ndim`
/// is the number of array dimensions (absent for scalars).  `shape`
/// fixes the array shape when known at creation time.  `dm_type`
/// and `dm_group` control the storage manager assignment.
///
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

/// Table creation options from the AS clause.
///
///
/// Controls the storage backend and byte order when creating a new table
/// via CREATE TABLE ... AS.  `type` selects the table format
/// (PLAIN, MEMORY, SCRATCH).  `endian` picks byte order (BIG,
/// LITTLE, LOCAL, AIPSRC).  `storage` selects the storage
/// manager layout (SEPFILE, MULTIFILE, MULTIHDF5).
///
struct TaqlTableOptions {
    std::string type;    // PLAIN, MEMORY, SCRATCH, etc.
    std::string endian;  // BIG, LITTLE, LOCAL, AIPSRC
    std::string storage; // SEPFILE, MULTIFILE, MULTIHDF5
    bool overwrite = false;
};

/// Column assignment for UPDATE SET clauses.
///
///
/// Represents one "col = expr" clause from an UPDATE statement.
/// `indices` carries array slice expressions when the target
/// is a sub-range of an array column (e.g. `DATA[0:3]`).
/// `mask_column` names an optional boolean mask column that
/// gates which elements are written.
///
struct TaqlAssignment {
    std::string column;
    TaqlExprNode value;
    std::vector<TaqlExprNode> indices; // array slicing (optional)
    std::optional<std::string> mask_column;
};

/// ALTER TABLE sub-command discriminator.
///
///
/// Identifies the operation carried by a TaqlAlterStep.  Column operations
/// (`add_column`, `copy_column`, `rename_column`,
/// `drop_column`) and keyword operations (`set_keyword`,
/// `copy_keyword`, `rename_keyword`, `drop_keyword`)
/// are both represented, as is the `add_row` bulk row insertion.
///
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

/// One step in an ALTER TABLE command.
///
///
/// A single ALTER TABLE statement may carry multiple steps.  The
/// `action` tag selects which of the payload fields are valid:
/// <dl>
///   <dt>add_column</dt>    <dd>`columns` describes columns to add.</dd>
///   <dt>copy_column</dt>   <dd>`renames` maps old->new names.</dd>
///   <dt>rename_column</dt> <dd>`renames` maps old->new names.</dd>
///   <dt>drop_column</dt>   <dd>`names` lists columns to remove.</dd>
///   <dt>set_keyword</dt>   <dd>`keywords` maps name->value expression.</dd>
///   <dt>copy_keyword</dt>  <dd>`renames` maps source->dest keyword.</dd>
///   <dt>rename_keyword</dt><dd>`renames` maps old->new keyword.</dd>
///   <dt>drop_keyword</dt>  <dd>`names` lists keywords to remove.</dd>
///   <dt>add_row</dt>       <dd>`row_count` is the count expression.</dd>
/// </dl>
///
struct TaqlAlterStep {
    AlterAction action;
    std::vector<TaqlColumnDef> columns;                         // for add_column
    std::vector<std::pair<std::string, std::string>> renames;   // from->to
    std::vector<std::string> names;                             // for drop
    std::vector<std::pair<std::string, TaqlExprNode>> keywords; // for set_keyword
    TaqlExprNode row_count;                                     // for add_row
};

/// Table reference (name or subquery) used in FROM / JOIN clauses.
///
///
/// A named table reference identifies a table by its path or registered
/// name; `shorthand` holds the alias (e.g. `t1`) used
/// to qualify column names in the query.  When the FROM source is a
/// parenthesised subquery, `subquery_ast` holds the inner AST
/// as a TaqlExprNode wrapper instead.
///
struct TaqlTableRef {
    std::string name;                         // table path or name
    std::string shorthand;                    // alias (t0, t1, etc.)
    std::optional<TaqlExprNode> subquery_ast; // if FROM (subquery)
};

/// Projection column (expression AS alias) in a SELECT list.
///
///
/// Represents one entry in the SELECT projection list.  `expr`
/// is the value expression; `alias` is the output column name
/// (may be empty if none was specified).  `data_type` is the
/// optional output type cast keyword (e.g. "float", "complex").
///
struct TaqlProjection {
    TaqlExprNode expr;
    std::string alias;
    std::string data_type; // optional output type
};

/// JOIN specification pairing a table reference with an ON predicate.
struct TaqlJoin {
    TaqlTableRef table;
    TaqlExprNode on_expr;
};

// ---------------------------------------------------------------------------
// Top-level AST
// ---------------------------------------------------------------------------

///
/// Complete parsed TaQL command represented as an abstract syntax tree.
///
///
///
///
///
/// TaqlAst holds the full result of parsing a TaQL query string.  The
/// active fields depend on the command type stored in `command`:
/// <dl>
///   <dt>SELECT / COUNT</dt>
///   <dd>projections, tables, where_expr, group_by, order_by, limit_expr, offset_expr</dd>
///   <dt>UPDATE</dt>
///   <dd>tables, assignments, where_expr</dd>
///   <dt>INSERT</dt>
///   <dd>tables, insert_columns, insert_values</dd>
///   <dt>DELETE</dt>
///   <dd>tables, where_expr</dd>
///   <dt>CALC</dt>
///   <dd>calc_expr</dd>
///   <dt>CREATE TABLE</dt>
///   <dd>tables, create_columns, table_options</dd>
///   <dt>ALTER TABLE</dt>
///   <dd>tables, alter_steps</dd>
///   <dt>DROP TABLE</dt>
///   <dd>drop_tables</dd>
/// </dl>
///
/// Fields not relevant to the active command are left default-initialised
/// and should not be accessed.  Use `taql_parse` to create a
/// TaqlAst; direct construction is for testing only.
///
///
/// @par Example
/// @code{.cpp}
///   auto ast = taql_parse("SELECT TIME, DATA FROM my.ms WHERE ANTENNA1=0");
///   // Inspect command type
///   assert(ast.command == TaqlCommand::select_cmd);
///   // Walk projections
///   for (const auto& proj : ast.projections) {
///       if (proj.expr.type == ExprType::column_ref)
///           std::cout << proj.expr.name << '\n';
///   }
///   // Check WHERE clause
///   if (ast.where_expr.has_value())
///       process_predicate(*ast.where_expr);
/// @endcode
///
///
/// @par Motivation
/// A single value type for all TaQL commands simplifies the evaluator
/// interface and avoids a polymorphic hierarchy.  Command-specific
/// processing is handled via the command tag rather than virtual dispatch.
///
struct TaqlAst {
    TaqlCommand command = TaqlCommand::select_cmd;

    // SELECT / COUNT
    std::vector<TaqlProjection> projections;
    std::vector<TaqlTableRef> tables;      // FROM
    std::vector<TaqlTableRef> with_tables; // WITH
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

/// Source location for error reporting within a TaQL query string.
///
///
/// Carries byte `offset`, one-based `line` number, and
/// one-based `column` number pointing to the offending token so
/// that callers can produce annotated diagnostic messages.
///
struct TaqlSourceLoc {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

/// Parse error with location context returned by the parser.
///
///
/// A TaqlParseError captures the human-readable `message`, the
/// `location` in the source string, the offending `token`
/// text, and a `category` tag (e.g. "syntax", "unknown_function")
/// for programmatic error handling.  Use `format_parse_error`
/// to render it with a source excerpt.
///
struct TaqlParseError {
    std::string message;
    TaqlSourceLoc location;
    std::string token;
    std::string category;
};

// ---------------------------------------------------------------------------
// Parser API
// ---------------------------------------------------------------------------

/// Parse a TaQL query string into an abstract syntax tree.
///
///
/// Accepts the full TaQL grammar including SELECT, UPDATE, INSERT,
/// DELETE, COUNT, CALC, CREATE TABLE, ALTER TABLE, DROP TABLE, and
/// SHOW commands.  Expression nodes are stored in TaqlExprNode trees
/// and the complete result is returned as a TaqlAst.
///
/// The parser is non-destructive: the original query string is not
/// modified, and the returned AST owns all allocated nodes.  Case is
/// folded for TaQL keywords and function names; table and column names
/// preserve their original case.
///
///
/// @par Example
/// @code{.cpp}
///   auto ast = taql_parse("SELECT TIME, DATA FROM my.ms WHERE ANTENNA1=0");
///   assert(ast.command == TaqlCommand::select_cmd);
///   assert(ast.projections.size() == 2);
/// @endcode
///
///
/// @param query  The TaQL query string.
/// @return Parsed AST.
/// @throws std::runtime_error on parse failure with diagnostic details.
[[nodiscard]] TaqlAst taql_parse(std::string_view query);

/// Format a TaQL parse error as a human-readable string.
///
///
/// Produces a multi-line message containing the error description, a
/// copy of the source line, and a caret pointing at the offending
/// token.  Suitable for user-facing diagnostics or test output.
///
///
/// @param error   The parse error value.
/// @param source  The original query string (used to extract the source line).
/// @return Formatted diagnostic string.
[[nodiscard]] std::string format_parse_error(const TaqlParseError& error, std::string_view source);

// ---------------------------------------------------------------------------
// SHOW command
// ---------------------------------------------------------------------------

/// Get TaQL help text for a topic.
///
///
/// Returns a human-readable help string for the given TaQL topic.  When
/// `topic` is empty or "overview", a summary of all available
/// commands and functions is returned.  Recognised topic strings include
/// command names ("SELECT", "CALC"), function categories ("MATH", "ASTRO"),
/// and individual function names.
///
///
/// @par Example
/// @code{.cpp}
///   std::cout << taql_show("SELECT");
///   std::cout << taql_show("SIN");
///   std::cout << taql_show();   // general overview
/// @endcode
///
///
/// @param topic Help topic (empty for overview).
/// @return Help text string.
[[nodiscard]] std::string taql_show(std::string_view topic = "");

// ---------------------------------------------------------------------------
// Evaluator API (populated in W4)
// ---------------------------------------------------------------------------

/// Result of executing a TaQL command against one or more tables.
///
///
/// TaqlResult bundles all output produced by the evaluator:
/// <dl>
///   <dt>rows</dt>          <dd>Row indices selected or modified.</dd>
///   <dt>values</dt>        <dd>Column values for CALC or scalar SELECT results.</dd>
///   <dt>column_names</dt>  <dd>Output column names in projection order.</dd>
///   <dt>affected_rows</dt> <dd>Row count for UPDATE/INSERT/DELETE.</dd>
///   <dt>output_table</dt>  <dd>Path to the result table for SELECT INTO or CREATE TABLE.</dd>
///   <dt>show_text</dt>     <dd>Help or description text for SHOW commands.</dd>
/// </dl>
///
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
///
///
/// Parses `query` and evaluates it against the supplied Table.
/// The table is identified internally using the name embedded in the
/// FROM clause, or the first table reference if no explicit name is given.
/// All DML operations (UPDATE, INSERT, DELETE) are applied in place; the
/// table is modified immediately without an additional commit step.
///
///
/// @par Example
/// @code{.cpp}
///   Table tbl("my.ms");
///   auto res = taql_execute(
///       "SELECT TIME, ANTENNA1 FROM my.ms WHERE ANTENNA1 < 3", tbl);
///   for (auto row : res.rows)
///       process_row(tbl, row);
/// @endcode
///
///
/// @param query TaQL query string.
/// @param table The table to operate on.
/// @return Query result.
/// @throws std::runtime_error on execution failure.
[[nodiscard]] TaqlResult taql_execute(std::string_view query, Table& table);

/// Execute a TaQL command against multiple named tables.
///
///
/// Overload for multi-table queries.  `tables` maps shorthand
/// aliases (e.g. "t1", "cal") to Table pointers.  The first entry in
/// iteration order is treated as the primary table when no explicit
/// shorthand is specified in the FROM clause.  JOIN and sub-table
/// references use the same map.
///
///
/// @par Example
/// @code{.cpp}
///   Table ms("science.ms"), cal("cal.ms");
///   auto res = taql_execute(
///       "SELECT t1.DATA / t2.DATA FROM $ms t1, $cal t2",
///       {{"t1", &ms}, {"t2", &cal}});
/// @endcode
///
///
/// @param query TaQL query string.
/// @param tables Map of shorthand/alias to Table reference. The first entry
///              is the primary table.
/// @return Query result.
/// @throws std::runtime_error on execution failure.
[[nodiscard]] TaqlResult taql_execute(std::string_view query,
                                      std::unordered_map<std::string, Table*> tables);

/// Execute a TaQL CALC expression with no table context.
///
///
/// Evaluates a standalone CALC expression such as
/// `CALC sin(45deg)` or `CALC 2*pi()*1.5GHz`.  No
/// table is required; all inputs must be literal constants or built-in
/// function calls.  The result appears in TaqlResult::values.
///
///
/// @par Example
/// @code{.cpp}
///   auto r = taql_calc("CALC sin(45deg)");
///   double val = std::get<double>(r.values[0]);  // 0.7071...
/// @endcode
///
///
/// @param query TaQL CALC expression.
/// @return Calculated values.
[[nodiscard]] TaqlResult taql_calc(std::string_view query);

/// @}
} // namespace casacore_mini
