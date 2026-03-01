/// @file table_info_test.cpp
/// @brief P12-W10 tests for Table info/metadata, private keywords, has_column,
///        parse_data_type_name, data_type_to_string utilities.

#include "casacore_mini/table.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
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

static fs::path make_temp_dir(const std::string& suffix) {
    return fs::temp_directory_path() /
           ("tbl_info_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) fs::remove_all(p);
}

// ============================================================================
// Test: Table info read/write
// ============================================================================
static void test_table_info() {
    auto path = make_temp_dir("info");
    cleanup(path);

    std::vector<TableColumnSpec> cols = {
        {.name = "X", .data_type = DataType::tp_int, .shape = {}},
    };
    auto table = Table::create(path, cols, 0);

    // Default info type should be "PlainTable" (set by Table::create)
    auto info = table.table_info();
    check(info.type == "PlainTable", "default type is PlainTable");

    check(table.table_info_type() == "PlainTable", "table_info_type() convenience");

    // Set custom type
    table.set_table_info("MyCustomType", "SubVariant");
    auto info2 = table.table_info();
    check(info2.type == "MyCustomType", "set_table_info type");
    check(info2.sub_type == "SubVariant", "set_table_info sub_type");

    // Verify persistence: re-open from disk
    auto table2 = Table::open(path);
    auto info3 = table2.table_info();
    check(info3.type == "MyCustomType", "type persisted on disk");
    check(info3.sub_type == "SubVariant", "sub_type persisted on disk");

    std::cout << "  table info read/write... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: Private keywords
// ============================================================================
static void test_private_keywords() {
    auto path = make_temp_dir("privkw");
    cleanup(path);

    Record priv_kw;
    priv_kw.set("secret_key", RecordValue(42));

    std::vector<TableColumnSpec> cols = {
        {.name = "A", .data_type = DataType::tp_float, .shape = {}},
    };
    TableCreateOptions opts;
    opts.private_keywords = priv_kw;
    auto table = Table::create(path, cols, 0, opts);

    const auto& pk = table.private_keywords();
    check(pk.find("secret_key") != nullptr, "private keyword exists");
    const auto* val = pk.find("secret_key");
    check(val != nullptr && std::get<std::int32_t>(val->storage()) == 42,
          "private keyword value correct");

    // Mutable access
    table.rw_private_keywords().set("another", RecordValue(std::string("test")));
    check(table.private_keywords().find("another") != nullptr, "mutable private keyword added");

    std::cout << "  private keywords... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: has_column
// ============================================================================
static void test_has_column() {
    auto path = make_temp_dir("hascol");
    cleanup(path);

    std::vector<TableColumnSpec> cols = {
        {.name = "FOO", .data_type = DataType::tp_int, .shape = {}},
        {.name = "BAR", .data_type = DataType::tp_string, .shape = {}},
    };
    auto table = Table::create(path, cols, 0);

    check(table.has_column("FOO"), "has_column FOO");
    check(table.has_column("BAR"), "has_column BAR");
    check(!table.has_column("NONEXISTENT"), "!has_column NONEXISTENT");

    std::cout << "  has_column... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: parse_data_type_name
// ============================================================================
static void test_parse_data_type_name() {
    check(parse_data_type_name("BOOL") == DataType::tp_bool, "BOOL");
    check(parse_data_type_name("Bool") == DataType::tp_bool, "Bool");
    check(parse_data_type_name("B") == DataType::tp_bool, "B");
    check(parse_data_type_name("INT") == DataType::tp_int, "INT");
    check(parse_data_type_name("Int") == DataType::tp_int, "Int");
    check(parse_data_type_name("I4") == DataType::tp_int, "I4");
    check(parse_data_type_name("INT32") == DataType::tp_int, "INT32");
    check(parse_data_type_name("I") == DataType::tp_int, "I");
    check(parse_data_type_name("INT64") == DataType::tp_int64, "INT64");
    check(parse_data_type_name("I8") == DataType::tp_int64, "I8");
    check(parse_data_type_name("SHORT") == DataType::tp_short, "SHORT");
    check(parse_data_type_name("Short") == DataType::tp_short, "Short");
    check(parse_data_type_name("I2") == DataType::tp_short, "I2");
    check(parse_data_type_name("FLOAT") == DataType::tp_float, "FLOAT");
    check(parse_data_type_name("Float") == DataType::tp_float, "Float");
    check(parse_data_type_name("R4") == DataType::tp_float, "R4");
    check(parse_data_type_name("DOUBLE") == DataType::tp_double, "DOUBLE");
    check(parse_data_type_name("Double") == DataType::tp_double, "Double");
    check(parse_data_type_name("R8") == DataType::tp_double, "R8");
    check(parse_data_type_name("D") == DataType::tp_double, "D");
    check(parse_data_type_name("COMPLEX") == DataType::tp_complex, "COMPLEX");
    check(parse_data_type_name("Complex") == DataType::tp_complex, "Complex");
    check(parse_data_type_name("C4") == DataType::tp_complex, "C4");
    check(parse_data_type_name("DCOMPLEX") == DataType::tp_dcomplex, "DCOMPLEX");
    check(parse_data_type_name("DComplex") == DataType::tp_dcomplex, "DComplex");
    check(parse_data_type_name("C8") == DataType::tp_dcomplex, "C8");
    check(parse_data_type_name("STRING") == DataType::tp_string, "STRING");
    check(parse_data_type_name("String") == DataType::tp_string, "String");
    check(parse_data_type_name("S") == DataType::tp_string, "S");

    // Unknown type throws
    bool threw = false;
    try { (void)parse_data_type_name("UNKNOWN"); } catch (const std::runtime_error&) { threw = true; }
    check(threw, "unknown type throws");

    std::cout << "  parse_data_type_name... PASS\n";
}

// ============================================================================
// Test: data_type_to_string
// ============================================================================
static void test_data_type_to_string() {
    check(data_type_to_string(DataType::tp_bool) == "Bool", "Bool");
    check(data_type_to_string(DataType::tp_int) == "Int", "Int");
    check(data_type_to_string(DataType::tp_int64) == "Int64", "Int64");
    check(data_type_to_string(DataType::tp_short) == "Short", "Short");
    check(data_type_to_string(DataType::tp_float) == "Float", "Float");
    check(data_type_to_string(DataType::tp_double) == "Double", "Double");
    check(data_type_to_string(DataType::tp_complex) == "Complex", "Complex");
    check(data_type_to_string(DataType::tp_dcomplex) == "DComplex", "DComplex");
    check(data_type_to_string(DataType::tp_string) == "String", "String");

    std::cout << "  data_type_to_string... PASS\n";
}

int main() {
    try { test_table_info(); } catch (const std::exception& e) {
        ++g_fail; std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try { test_private_keywords(); } catch (const std::exception& e) {
        ++g_fail; std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try { test_has_column(); } catch (const std::exception& e) {
        ++g_fail; std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try { test_parse_data_type_name(); } catch (const std::exception& e) {
        ++g_fail; std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try { test_data_type_to_string(); } catch (const std::exception& e) {
        ++g_fail; std::cerr << "EXCEPTION: " << e.what() << "\n";
    }

    std::cout << "table_info_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
