/// @file taql_malformed_test.cpp
/// @brief Tests that malformed TaQL queries produce parse errors.

#include "casacore_mini/taql.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
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

/// Verify that parsing the given query throws a runtime_error.
static bool expect_parse_error(const char* query, const char* label) {
    try {
        (void)taql_parse(query);
        // If we get here, parsing unexpectedly succeeded
        check(false, label);
        return false;
    } catch (const std::runtime_error&) {
        check(true, label);
        return true;
    } catch (...) {
        check(false, label);
        return false;
    }
}

// ---------------------------------------------------------------------------
// Malformed query tests
// ---------------------------------------------------------------------------

static bool test_empty_query() {
    // Empty queries should parse as bare SELECT (not throw).
    // This is actually valid in TaQL: empty produces a select with no projections.
    try {
        auto ast = taql_parse("");
        check(ast.command == TaqlCommand::select_cmd, "empty parses as select");
        return true;
    } catch (...) {
        check(false, "empty should not throw");
        return false;
    }
}

static bool test_update_missing_set() {
    return expect_parse_error("UPDATE t",
                              "UPDATE without SET should fail");
}

static bool test_unclosed_paren() {
    return expect_parse_error("SELECT (col1 + col2 FROM t",
                              "unclosed paren should fail");
}

static bool test_invalid_after_where() {
    return expect_parse_error("SELECT col1 FROM t WHERE",
                              "WHERE with no expression should fail");
}

static bool test_between_missing_and() {
    return expect_parse_error("SELECT col1 FROM t WHERE col1 BETWEEN 1 OR 10",
                              "BETWEEN missing AND should fail");
}

static bool test_not_without_operator() {
    return expect_parse_error("SELECT col1 FROM t WHERE col1 NOT AROUND 5",
                              "NOT without LIKE/IN/BETWEEN should fail");
}

static bool test_insert_values_unclosed() {
    return expect_parse_error("INSERT INTO t VALUES (1, 2",
                              "unclosed VALUES paren should fail");
}

static bool test_unknown_character() {
    // The backtick is not in TaQL grammar
    return expect_parse_error("SELECT `col1` FROM t",
                              "backtick should fail");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_empty_query();
    test_update_missing_set();
    test_unclosed_paren();
    test_invalid_after_where();
    test_between_missing_and();
    test_not_without_operator();
    test_insert_values_unclosed();
    test_unknown_character();

    std::cout << "taql_malformed_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
