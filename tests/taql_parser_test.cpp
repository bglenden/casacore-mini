/// @file taql_parser_test.cpp
/// @brief Tests for the TaQL parser and AST construction.

// NOLINTBEGIN(bugprone-exception-escape,bugprone-unchecked-optional-access)

#include "casacore_mini/taql.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

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

// ---------------------------------------------------------------------------
// Command type recognition
// ---------------------------------------------------------------------------

static bool test_select_basic() {
    auto ast = taql_parse("SELECT col1, col2 FROM mytable");
    check(ast.command == TaqlCommand::select_cmd, "select command type");
    check(ast.projections.size() == 2, "select 2 projections");
    check(ast.projections[0].expr.type == ExprType::column_ref, "proj0 is column_ref");
    check(ast.projections[0].expr.name == "col1", "proj0 name");
    check(ast.projections[1].expr.name == "col2", "proj1 name");
    check(ast.tables.size() == 1, "1 table");
    check(ast.tables[0].name == "mytable", "table name");
    return g_fail == 0;
}

static bool test_select_where() {
    auto ast = taql_parse("SELECT col1 FROM t WHERE col1 > 5");
    check(ast.where_expr.has_value(), "has where");
    check(ast.where_expr->type == ExprType::comparison, "where is comparison");
    check(ast.where_expr->op == TaqlOp::gt, "where op is gt");
    check(ast.where_expr->children.size() == 2, "where has 2 children");
    check(ast.where_expr->children[0].type == ExprType::column_ref, "where lhs is column_ref");
    check(ast.where_expr->children[1].type == ExprType::literal, "where rhs is literal");
    auto& rhs = ast.where_expr->children[1].value;
    check(std::holds_alternative<std::int64_t>(rhs), "rhs is int");
    check(std::get<std::int64_t>(rhs) == 5, "rhs value is 5");
    return g_fail == 0;
}

static bool test_select_distinct() {
    auto ast = taql_parse("SELECT DISTINCT col1 FROM t");
    check(ast.select_distinct, "select distinct");
    check(ast.projections.size() == 1, "1 projection");
    return g_fail == 0;
}

static bool test_select_orderby() {
    auto ast = taql_parse("SELECT col1 FROM t ORDERBY col1 DESC");
    check(ast.order_by.size() == 1, "1 sort key");
    check(ast.order_by[0].order == SortOrder::descending, "desc order");
    return g_fail == 0;
}

static bool test_select_groupby_having() {
    auto ast = taql_parse("SELECT col1, gcount() FROM t GROUPBY col1 HAVING gcount() > 1");
    check(ast.group_by.size() == 1, "1 group-by expr");
    check(ast.having_expr.has_value(), "has having");
    return g_fail == 0;
}

static bool test_select_limit_offset() {
    auto ast = taql_parse("SELECT col1 FROM t LIMIT 10 OFFSET 5");
    check(ast.limit_expr.has_value(), "has limit");
    check(ast.offset_expr.has_value(), "has offset");
    return g_fail == 0;
}

static bool test_select_with() {
    auto ast = taql_parse("WITH t1 SELECT col1 FROM t1");
    check(ast.with_tables.size() == 1, "1 with table");
    check(ast.with_tables[0].name == "t1", "with table name");
    return g_fail == 0;
}

static bool test_select_join() {
    auto ast = taql_parse("SELECT t1.col1 FROM t1 JOIN t2 ON t1.key = t2.key");
    check(ast.joins.size() == 1, "1 join");
    check(ast.joins[0].table.name == "t2", "join table name");
    return g_fail == 0;
}

static bool test_select_wildcard() {
    auto ast = taql_parse("SELECT * FROM t");
    check(ast.projections.size() == 1, "1 projection");
    check(ast.projections[0].expr.type == ExprType::wildcard, "wildcard");
    return g_fail == 0;
}

static bool test_select_alias() {
    auto ast = taql_parse("SELECT col1 AS c FROM t");
    check(ast.projections[0].alias == "c", "alias");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// UPDATE
// ---------------------------------------------------------------------------

static bool test_update() {
    auto ast = taql_parse("UPDATE t SET col1 = 42 WHERE col2 > 0");
    check(ast.command == TaqlCommand::update_cmd, "update command type");
    check(ast.assignments.size() == 1, "1 assignment");
    check(ast.assignments[0].column == "col1", "assignment column");
    check(ast.where_expr.has_value(), "has where");
    return g_fail == 0;
}

static bool test_update_multi() {
    auto ast = taql_parse("UPDATE t SET col1 = 1, col2 = 2");
    check(ast.assignments.size() == 2, "2 assignments");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// INSERT
// ---------------------------------------------------------------------------

static bool test_insert_values() {
    auto ast = taql_parse("INSERT INTO t VALUES (1, 'hello')");
    check(ast.command == TaqlCommand::insert_cmd, "insert command type");
    check(ast.insert_values.size() == 1, "1 row");
    check(ast.insert_values[0].size() == 2, "2 values in row");
    return g_fail == 0;
}

static bool test_insert_set() {
    auto ast = taql_parse("INSERT INTO t SET col1 = 42, col2 = 'test'");
    check(ast.command == TaqlCommand::insert_cmd, "insert set command");
    check(ast.insert_columns.size() == 2, "2 insert columns");
    check(ast.insert_values.size() == 1, "1 row");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// DELETE
// ---------------------------------------------------------------------------

static bool test_delete() {
    auto ast = taql_parse("DELETE FROM t WHERE col1 < 0");
    check(ast.command == TaqlCommand::delete_cmd, "delete command type");
    check(ast.where_expr.has_value(), "has where");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// COUNT
// ---------------------------------------------------------------------------

static bool test_count() {
    auto ast = taql_parse("COUNT FROM t WHERE col1 > 0");
    check(ast.command == TaqlCommand::count_cmd, "count command type");
    check(ast.where_expr.has_value(), "has where");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// CALC
// ---------------------------------------------------------------------------

static bool test_calc() {
    auto ast = taql_parse("CALC 2 + 3");
    check(ast.command == TaqlCommand::calc_cmd, "calc command type");
    check(ast.calc_expr.has_value(), "has calc expr");
    check(ast.calc_expr->type == ExprType::binary_op, "calc is binary_op");
    check(ast.calc_expr->op == TaqlOp::plus, "calc op is plus");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// CREATE TABLE
// ---------------------------------------------------------------------------

static bool test_create_table() {
    auto ast = taql_parse("CREATE TABLE newtable (col1 INT, col2 DOUBLE)");
    check(ast.command == TaqlCommand::create_table_cmd, "create table command");
    check(ast.create_columns.size() == 2, "2 columns");
    check(ast.create_columns[0].name == "col1", "col1 name");
    check(ast.create_columns[0].data_type == "INT", "col1 type");
    check(ast.create_columns[1].name == "col2", "col2 name");
    check(ast.create_columns[1].data_type == "DOUBLE", "col2 type");
    return g_fail == 0;
}

static bool test_create_table_like() {
    auto ast = taql_parse("CREATE TABLE newtable LIKE existing");
    check(ast.like_table == "existing", "like table");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// ALTER TABLE
// ---------------------------------------------------------------------------

static bool test_alter_table_add() {
    auto ast = taql_parse("ALTER TABLE t ADD COLUMN (col3 STRING)");
    check(ast.command == TaqlCommand::alter_table_cmd, "alter table command");
    check(ast.alter_steps.size() == 1, "1 alter step");
    check(ast.alter_steps[0].action == AlterAction::add_column, "add_column");
    check(ast.alter_steps[0].columns.size() == 1, "1 column added");
    return g_fail == 0;
}

static bool test_alter_table_drop() {
    auto ast = taql_parse("ALTER TABLE t DROP COLUMN col3");
    check(ast.alter_steps.size() == 1, "1 alter step");
    check(ast.alter_steps[0].action == AlterAction::drop_column, "drop_column");
    check(ast.alter_steps[0].names.size() == 1, "1 name dropped");
    return g_fail == 0;
}

static bool test_alter_table_rename() {
    auto ast = taql_parse("ALTER TABLE t RENAME COLUMN old TO new");
    check(ast.alter_steps[0].action == AlterAction::rename_column, "rename_column");
    check(ast.alter_steps[0].renames[0].first == "old", "rename from");
    check(ast.alter_steps[0].renames[0].second == "new", "rename to");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// DROP TABLE
// ---------------------------------------------------------------------------

static bool test_drop_table() {
    auto ast = taql_parse("DROP TABLE t1, t2");
    check(ast.command == TaqlCommand::drop_table_cmd, "drop table command");
    check(ast.drop_tables.size() == 2, "2 tables dropped");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// SHOW
// ---------------------------------------------------------------------------

static bool test_show() {
    auto ast = taql_parse("SHOW functions");
    check(ast.command == TaqlCommand::show_cmd, "show command type");
    check(ast.show_topic == "functions", "show topic");
    return g_fail == 0;
}

static bool test_show_help() {
    auto text = taql_show();
    check(!text.empty(), "show help not empty");
    check(text.find("SELECT") != std::string::npos, "show mentions SELECT");
    return g_fail == 0;
}

static bool test_show_functions() {
    auto text = taql_show("functions");
    check(!text.empty(), "show functions not empty");
    check(text.find("sin") != std::string::npos, "show mentions sin");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Expression tests
// ---------------------------------------------------------------------------

static bool test_arithmetic_precedence() {
    // 2 + 3 * 4 should parse as 2 + (3 * 4)
    auto ast = taql_parse("CALC 2 + 3 * 4");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::binary_op, "top is binary_op");
    check(expr.op == TaqlOp::plus, "top op is plus");
    check(expr.children[1].type == ExprType::binary_op, "rhs is binary_op");
    check(expr.children[1].op == TaqlOp::multiply, "rhs op is multiply");
    return g_fail == 0;
}

static bool test_power_right_assoc() {
    // 2 ** 3 ** 4 should parse as 2 ** (3 ** 4) (right-associative)
    auto ast = taql_parse("CALC 2 ** 3 ** 4");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::binary_op, "top is binary_op");
    check(expr.op == TaqlOp::power, "top op is power");
    check(expr.children[1].type == ExprType::binary_op, "rhs is binary_op");
    check(expr.children[1].op == TaqlOp::power, "rhs op is power");
    return g_fail == 0;
}

static bool test_unary_negate() {
    auto ast = taql_parse("CALC -5");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::unary_op, "unary_op");
    check(expr.op == TaqlOp::negate, "negate");
    return g_fail == 0;
}

static bool test_comparison_operators() {
    auto test_op = [](const char* query, TaqlOp expected) {
        auto ast = taql_parse(query);
        return ast.where_expr.has_value() && ast.where_expr->op == expected;
    };
    check(test_op("SELECT x FROM t WHERE x = 1", TaqlOp::eq), "eq");
    check(test_op("SELECT x FROM t WHERE x == 1", TaqlOp::eq), "== as eq");
    check(test_op("SELECT x FROM t WHERE x != 1", TaqlOp::ne), "ne");
    check(test_op("SELECT x FROM t WHERE x <> 1", TaqlOp::ne), "<> as ne");
    check(test_op("SELECT x FROM t WHERE x < 1", TaqlOp::lt), "lt");
    check(test_op("SELECT x FROM t WHERE x <= 1", TaqlOp::le), "le");
    check(test_op("SELECT x FROM t WHERE x > 1", TaqlOp::gt), "gt");
    check(test_op("SELECT x FROM t WHERE x >= 1", TaqlOp::ge), "ge");
    return g_fail == 0;
}

static bool test_logical_and_or() {
    auto ast = taql_parse("SELECT x FROM t WHERE a > 1 AND b < 2");
    check(ast.where_expr->type == ExprType::logical_op, "logical_op");
    check(ast.where_expr->op == TaqlOp::logical_and, "logical_and");

    auto ast2 = taql_parse("SELECT x FROM t WHERE a > 1 OR b < 2");
    check(ast2.where_expr->type == ExprType::logical_op, "logical_op OR");
    check(ast2.where_expr->op == TaqlOp::logical_or, "logical_or");
    return g_fail == 0;
}

static bool test_not_expr() {
    auto ast = taql_parse("SELECT x FROM t WHERE NOT a > 1");
    check(ast.where_expr->type == ExprType::unary_op, "unary_op NOT");
    check(ast.where_expr->op == TaqlOp::logical_not, "logical_not");
    return g_fail == 0;
}

static bool test_in_expr() {
    auto ast = taql_parse("SELECT x FROM t WHERE x IN [1,2,3]");
    check(ast.where_expr->type == ExprType::in_expr, "in_expr");
    check(ast.where_expr->op == TaqlOp::in_set, "in_set");
    return g_fail == 0;
}

static bool test_between_expr() {
    auto ast = taql_parse("SELECT x FROM t WHERE x BETWEEN 1 AND 10");
    check(ast.where_expr->type == ExprType::between_expr, "between_expr");
    check(ast.where_expr->children.size() == 3, "between has 3 children");
    return g_fail == 0;
}

static bool test_like_expr() {
    auto ast = taql_parse("SELECT x FROM t WHERE name LIKE 'foo*'");
    check(ast.where_expr->type == ExprType::like_expr, "like_expr");
    check(ast.where_expr->op == TaqlOp::like, "like op");
    return g_fail == 0;
}

static bool test_function_call() {
    auto ast = taql_parse("CALC sin(3.14)");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::func_call, "func_call");
    check(expr.name == "sin", "function name");
    check(expr.children.size() == 1, "1 argument");
    return g_fail == 0;
}

static bool test_aggregate_call() {
    auto ast = taql_parse("SELECT gmin(col1) FROM t");
    check(ast.projections[0].expr.type == ExprType::aggregate_call, "aggregate_call");
    check(ast.projections[0].expr.name == "gmin", "aggregate name");
    return g_fail == 0;
}

static bool test_iif_expr() {
    auto ast = taql_parse("CALC IIF(TRUE, 1, 0)");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::iif_expr, "iif_expr");
    check(expr.children.size() == 3, "iif has 3 children");
    return g_fail == 0;
}

static bool test_boolean_literals() {
    auto ast = taql_parse("CALC TRUE");
    auto& val = ast.calc_expr->value;
    check(std::holds_alternative<bool>(val), "TRUE is bool");
    check(std::get<bool>(val) == true, "TRUE value");

    auto ast2 = taql_parse("CALC FALSE");
    check(std::get<bool>(ast2.calc_expr->value) == false, "FALSE value");
    return g_fail == 0;
}

static bool test_string_literal() {
    auto ast = taql_parse("CALC 'hello world'");
    auto& val = ast.calc_expr->value;
    check(std::holds_alternative<std::string>(val), "string literal");
    check(std::get<std::string>(val) == "hello world", "string value");
    return g_fail == 0;
}

static bool test_float_literal() {
    auto ast = taql_parse("CALC 3.14");
    auto& val = ast.calc_expr->value;
    check(std::holds_alternative<double>(val), "float literal");
    return g_fail == 0;
}

static bool test_complex_literal() {
    auto ast = taql_parse("CALC 2.5i");
    auto& val = ast.calc_expr->value;
    check(std::holds_alternative<std::complex<double>>(val), "complex literal");
    auto c = std::get<std::complex<double>>(val);
    check(c.real() == 0.0 && c.imag() == 2.5, "complex value");
    return g_fail == 0;
}

static bool test_hex_literal() {
    auto ast = taql_parse("CALC 0xFF");
    auto& val = ast.calc_expr->value;
    check(std::holds_alternative<std::int64_t>(val), "hex is int64");
    check(std::get<std::int64_t>(val) == 255, "hex value");
    return g_fail == 0;
}

static bool test_keyword_ref() {
    auto ast = taql_parse("SELECT col::mykey FROM t");
    check(ast.projections[0].expr.type == ExprType::keyword_ref, "keyword_ref");
    check(ast.projections[0].expr.name.find("::") != std::string::npos, "has ::");
    return g_fail == 0;
}

static bool test_nested_parentheses() {
    auto ast = taql_parse("CALC (2 + 3) * 4");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::binary_op, "binary_op");
    check(expr.op == TaqlOp::multiply, "multiply");
    check(expr.children[0].type == ExprType::binary_op, "lhs is binary_op");
    check(expr.children[0].op == TaqlOp::plus, "lhs op is plus");
    return g_fail == 0;
}

static bool test_array_literal() {
    auto ast = taql_parse("CALC [1, 2, 3]");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::set_expr, "set_expr");
    check(expr.children.size() == 3, "3 elements");
    return g_fail == 0;
}

static bool test_record_literal() {
    auto ast = taql_parse("CALC [name=1, value=2]");
    auto& expr = *ast.calc_expr;
    check(expr.type == ExprType::record_literal, "record_literal");
    check(expr.children.size() == 2, "2 fields");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// TIMING and STYLE
// ---------------------------------------------------------------------------

static bool test_timing() {
    auto ast = taql_parse("TIMING SELECT col1 FROM t");
    check(ast.timing, "timing flag");
    check(ast.command == TaqlCommand::select_cmd, "still select");
    return g_fail == 0;
}

static bool test_style() {
    auto ast = taql_parse("STYLE PYTHON SELECT col1 FROM t");
    check(ast.style == "PYTHON", "style");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// format_parse_error
// ---------------------------------------------------------------------------

static bool test_format_parse_error() {
    TaqlParseError err;
    err.message = "test error";
    err.location = {5, 1, 6};
    err.token = "bad";
    auto msg = format_parse_error(err, "SELECT bad FROM t");
    check(!msg.empty(), "format_parse_error not empty");
    check(msg.find("test error") != std::string::npos, "contains message");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// taql_calc evaluator
// ---------------------------------------------------------------------------

static bool test_calc_eval() {
    auto result = taql_calc("CALC 1 + 2");
    check(result.values.size() == 1, "calc returns 1 value");
    check(std::holds_alternative<std::int64_t>(result.values[0]), "calc result is int");
    check(std::get<std::int64_t>(result.values[0]) == 3, "1+2=3");

    auto r2 = taql_calc("CALC 3.14 * 2");
    check(r2.values.size() == 1, "calc float returns 1 value");
    check(std::holds_alternative<double>(r2.values[0]), "calc float result is double");

    auto r3 = taql_calc("CALC sin(0)");
    check(r3.values.size() == 1, "sin(0) returns 1 value");
    auto sin_val = std::get<double>(r3.values[0]);
    check(sin_val < 1e-10 && sin_val > -1e-10, "sin(0)=0");

    auto r4 = taql_calc("CALC IIF(TRUE, 10, 20)");
    check(std::get<std::int64_t>(r4.values[0]) == 10, "IIF true branch");

    auto r5 = taql_calc("CALC 2 ** 10");
    check(std::get<std::int64_t>(r5.values[0]) == 1024, "2**10=1024");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_select_basic();
    test_select_where();
    test_select_distinct();
    test_select_orderby();
    test_select_groupby_having();
    test_select_limit_offset();
    test_select_with();
    test_select_join();
    test_select_wildcard();
    test_select_alias();
    test_update();
    test_update_multi();
    test_insert_values();
    test_insert_set();
    test_delete();
    test_count();
    test_calc();
    test_create_table();
    test_create_table_like();
    test_alter_table_add();
    test_alter_table_drop();
    test_alter_table_rename();
    test_drop_table();
    test_show();
    test_show_help();
    test_show_functions();
    test_arithmetic_precedence();
    test_power_right_assoc();
    test_unary_negate();
    test_comparison_operators();
    test_logical_and_or();
    test_not_expr();
    test_in_expr();
    test_between_expr();
    test_like_expr();
    test_function_call();
    test_aggregate_call();
    test_iif_expr();
    test_boolean_literals();
    test_string_literal();
    test_float_literal();
    test_complex_literal();
    test_hex_literal();
    test_keyword_ref();
    test_nested_parentheses();
    test_array_literal();
    test_record_literal();
    test_timing();
    test_style();
    test_format_parse_error();
    test_calc_eval();

    std::cout << "taql_parser_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// NOLINTEND(bugprone-exception-escape,bugprone-unchecked-optional-access)
