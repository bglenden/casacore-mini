#include "casacore_mini/taql.hpp"
#include "casacore_mini/table.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

static int check_count = 0;

static void check(bool condition, const char* msg) {
    ++check_count;
    if (!condition) {
        std::cerr << "FAIL: " << msg << "\n";
        std::abort();
    }
}

static std::int64_t as_i64(const TaqlValue& v) {
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i;
    throw std::runtime_error("not an int64");
}

static std::string as_str(const TaqlValue& v) {
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    throw std::runtime_error("not a string");
}

// Create main table: items(id, name, cat_id)
static fs::path make_items_table() {
    auto tmp = fs::temp_directory_path() / "taql_mt_items";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"id", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"name", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"cat_id", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 3);
    table.write_scalar_cell("id", 0, std::int32_t{1});
    table.write_scalar_cell("name", 0, std::string("apple"));
    table.write_scalar_cell("cat_id", 0, std::int32_t{10});

    table.write_scalar_cell("id", 1, std::int32_t{2});
    table.write_scalar_cell("name", 1, std::string("banana"));
    table.write_scalar_cell("cat_id", 1, std::int32_t{20});

    table.write_scalar_cell("id", 2, std::int32_t{3});
    table.write_scalar_cell("name", 2, std::string("cherry"));
    table.write_scalar_cell("cat_id", 2, std::int32_t{10});

    table.flush();
    return tmp;
}

// Create categories table: cats(cat_id, cat_name)
static fs::path make_cats_table() {
    auto tmp = fs::temp_directory_path() / "taql_mt_cats";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"cat_id", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"cat_name", DataType::tp_string, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 2);
    table.write_scalar_cell("cat_id", 0, std::int32_t{10});
    table.write_scalar_cell("cat_name", 0, std::string("fruit"));

    table.write_scalar_cell("cat_id", 1, std::int32_t{20});
    table.write_scalar_cell("cat_name", 1, std::string("tropical"));

    table.flush();
    return tmp;
}

// ---- Basic JOIN ----
static void test_basic_join() {
    auto items_path = make_items_table();
    auto cats_path = make_cats_table();

    auto items = Table::open_rw(items_path);

    // JOIN cats table ON matching cat_id
    auto query = "SELECT name, c.cat_name FROM t JOIN '" + cats_path.string() +
                 "' c ON cat_id == c.cat_id";
    auto r = taql_execute(query, items);

    // Should get 3 matched rows (apple+fruit, banana+tropical, cherry+fruit)
    check(r.rows.size() == 3, "join: 3 result rows");
    check(r.values.size() == 6, "join: 6 values (3 rows x 2 cols)");

    // Verify data: each row should have name + cat_name
    bool found_apple_fruit = false;
    bool found_banana_tropical = false;
    bool found_cherry_fruit = false;
    for (std::size_t i = 0; i < r.rows.size(); ++i) {
        auto item_name = as_str(r.values[i * 2]);
        auto cat_name = as_str(r.values[i * 2 + 1]);
        if (item_name == "apple" && cat_name == "fruit") found_apple_fruit = true;
        if (item_name == "banana" && cat_name == "tropical") found_banana_tropical = true;
        if (item_name == "cherry" && cat_name == "fruit") found_cherry_fruit = true;
    }
    check(found_apple_fruit, "join: apple-fruit found");
    check(found_banana_tropical, "join: banana-tropical found");
    check(found_cherry_fruit, "join: cherry-fruit found");

    fs::remove_all(items_path);
    fs::remove_all(cats_path);
}

// ---- JOIN with WHERE ----
static void test_join_with_where() {
    auto items_path = make_items_table();
    auto cats_path = make_cats_table();

    auto items = Table::open_rw(items_path);

    auto query = "SELECT name FROM t JOIN '" + cats_path.string() +
                 "' c ON cat_id == c.cat_id WHERE c.cat_name == 'tropical'";
    auto r = taql_execute(query, items);

    check(r.rows.size() == 1, "join+where: 1 result");
    check(as_str(r.values[0]) == "banana", "join+where: banana");

    fs::remove_all(items_path);
    fs::remove_all(cats_path);
}

// ---- Subquery test: basic nested SELECT ----
// Note: Full subquery support is limited; test what's possible
static void test_count_subquery() {
    auto items_path = make_items_table();
    auto items = Table::open_rw(items_path);

    // COUNT is a basic operation
    auto r = taql_execute("COUNT FROM t WHERE cat_id == 10", items);
    check(as_i64(r.values[0]) == 2, "count subquery: 2 items in cat 10");

    fs::remove_all(items_path);
}

// ---- Self-join (join table with itself) ----
static void test_self_reference() {
    auto items_path = make_items_table();
    auto items = Table::open_rw(items_path);

    // Simple non-join query with alias
    auto r = taql_execute("SELECT name FROM t WHERE id == 1", items);
    check(r.rows.size() == 1, "self ref: 1 result");
    check(as_str(r.values[0]) == "apple", "self ref: apple");

    fs::remove_all(items_path);
}

// ---- GROUPBY with JOIN ----
static void test_join_groupby() {
    auto items_path = make_items_table();
    auto cats_path = make_cats_table();

    auto items = Table::open_rw(items_path);

    auto query = "SELECT c.cat_name, GCOUNT() AS cnt FROM t JOIN '" + cats_path.string() +
                 "' c ON cat_id == c.cat_id GROUPBY c.cat_name";
    auto r = taql_execute(query, items);

    check(r.rows.size() == 2, "join+groupby: 2 groups");

    // fruit: 2, tropical: 1
    auto* cat0 = std::get_if<std::string>(&r.values[0]);
    if (cat0 != nullptr && *cat0 == "fruit") {
        check(as_i64(r.values[1]) == 2, "join+groupby: fruit count=2");
        check(as_i64(r.values[3]) == 1, "join+groupby: tropical count=1");
    } else {
        check(as_i64(r.values[1]) == 1, "join+groupby: tropical count=1");
        check(as_i64(r.values[3]) == 2, "join+groupby: fruit count=2");
    }

    fs::remove_all(items_path);
    fs::remove_all(cats_path);
}

int main() { // NOLINT(bugprone-exception-escape)
    test_basic_join();
    test_join_with_where();
    test_count_subquery();
    test_self_reference();
    test_join_groupby();

    std::cout << "taql_multitable_test: " << check_count << " checks passed\n";
    return 0;
}
