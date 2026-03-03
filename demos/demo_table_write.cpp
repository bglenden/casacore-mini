// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// demo_table_write.cpp -- Phase 7: table creation/write transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/tables/Tables/test/tTable.cc
//     TableDesc td("", "1", TableDesc::Scratch);
//     td.addColumn(ScalarColumnDesc<Int>("ab"));
//     td.addColumn(ScalarColumnDesc<uInt>("ad"));
//     SetupNewTable newtab("tTable_tmp.data", td, Table::New, stopt);
//     Table tab(newtab, 10);
//     ScalarColumn<Int> ab1(tab, "ab");
//     ab1.put(i, i);
//
//   Source: casacore-original/tables/Tables/test/tTableRow.cc
//     TableRow row(tab, stringToVector("ab,ad,ag,arr1,arr2,rec"));
//     TableRecord rec(row.record().description(), RecordInterface::Variable);
//     row.put(i, rec);
//
// This casacore-mini demo follows the same pattern: define schema, create
// table, write via typed scalar columns, and write via row records.

#include <casacore_mini/table.hpp>
#include <casacore_mini/table_column.hpp>

#include "demo_check.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

using namespace casacore_mini;
namespace fs = std::filesystem;

static fs::path make_temp_path() {
    return fs::temp_directory_path() / "casacore_mini_demo_table_write";
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: Table Write (Phase 7) ===\n";

        auto table_path = make_temp_path();
        if (fs::exists(table_path)) {
            fs::remove_all(table_path);
        }

        // 1. Create a table with 4 scalar columns and 10 rows.
        std::cout << "\n--- Creating table with 4 columns, 10 rows ---\n";
        std::vector<TableColumnSpec> columns = {
            {.name = "ID",
             .data_type = DataType::tp_int,
             .kind = ColumnKind::scalar,
             .shape = {},
             .comment = {}},
            {.name = "VALUE",
             .data_type = DataType::tp_float,
             .kind = ColumnKind::scalar,
             .shape = {},
             .comment = {}},
            {.name = "DVAL",
             .data_type = DataType::tp_double,
             .kind = ColumnKind::scalar,
             .shape = {},
             .comment = {}},
            {.name = "LABEL",
             .data_type = DataType::tp_string,
             .kind = ColumnKind::scalar,
             .shape = {},
             .comment = {}},
        };

        auto table = Table::create(table_path, columns, 10);
        std::cout << "  Created: " << table.table_name() << " at " << table.path() << "\n";
        std::cout << "  Rows:    " << table.nrow() << "\n";
        std::cout << "  Columns: " << table.ncolumn() << "\n";
        DEMO_CHECK(table.nrow() == 10);
        DEMO_CHECK(table.ncolumn() == 4);

        // 2. Write first 5 rows via ScalarColumn<T>::put().
        std::cout << "\n--- Writing rows 0-4 via ScalarColumn ---\n";
        ScalarColumn<std::int32_t> id_col(table, "ID");
        ScalarColumn<float> val_col(table, "VALUE");
        ScalarColumn<double> dval_col(table, "DVAL");
        ScalarColumn<std::string> label_col(table, "LABEL");

        for (std::uint64_t r = 0; r < 5; ++r) {
            id_col.put(r, static_cast<std::int32_t>(r * 10));
            val_col.put(r, static_cast<float>(r) * 1.5F);
            dval_col.put(r, static_cast<double>(r) * 3.14);
            label_col.put(r, "row_" + std::to_string(r));
        }
        std::cout << "  Wrote 5 rows via typed columns.\n";

        // 3. Write rows 5-9 via TableRow::put() using Records.
        std::cout << "\n--- Writing rows 5-9 via TableRow ---\n";
        TableRow row(table);
        for (std::uint64_t r = 5; r < 10; ++r) {
            Record rec;
            rec.set("ID", RecordValue(static_cast<std::int32_t>(r * 10)));
            rec.set("VALUE", RecordValue(static_cast<float>(r) * 1.5F));
            rec.set("DVAL", RecordValue(static_cast<double>(r) * 3.14));
            rec.set("LABEL", RecordValue(std::string("row_" + std::to_string(r))));
            row.put(r, rec);
        }
        std::cout << "  Wrote 5 rows via TableRow.\n";

        // 4. Flush to disk.
        std::cout << "\n--- Flushing to disk ---\n";
        table.flush();
        std::cout << "  Flushed.\n";

        // 5. Read back and verify.
        std::cout << "\n--- Reading back via Table::open ---\n";
        auto table2 = Table::open(table_path);
        DEMO_CHECK(table2.nrow() == 10);
        DEMO_CHECK(table2.ncolumn() == 4);

        ScalarColumn<std::int32_t> id2(table2, "ID");
        ScalarColumn<float> val2(table2, "VALUE");
        ScalarColumn<double> dval2(table2, "DVAL");
        ScalarColumn<std::string> label2(table2, "LABEL");

        for (std::uint64_t r = 0; r < 10; ++r) {
            auto expected_id = static_cast<std::int32_t>(r * 10);
            auto expected_val = static_cast<float>(r) * 1.5F;
            auto expected_dval = static_cast<double>(r) * 3.14;
            auto expected_label = "row_" + std::to_string(r);

            DEMO_CHECK(id2.get(r) == expected_id);
            DEMO_CHECK(std::fabs(val2.get(r) - expected_val) < 1e-6F);
            DEMO_CHECK(std::fabs(dval2.get(r) - expected_dval) < 1e-10);
            DEMO_CHECK(label2.get(r) == expected_label);

            std::cout << "  row " << r << ": ID=" << id2.get(r) << " VALUE=" << val2.get(r)
                      << " DVAL=" << dval2.get(r) << " LABEL=\"" << label2.get(r) << "\"\n";
        }
        std::cout << "  [OK] All 10 rows verified (column-oriented read).\n";

        // 6. Read back via TableRow.
        std::cout << "\n--- Reading back via TableRow ---\n";
        TableRow row2(table2);
        for (std::uint64_t r = 0; r < 10; ++r) {
            Record rec = row2.get(r);
            DEMO_CHECK(rec.size() == 4);
            DEMO_CHECK(std::get<std::int32_t>(rec.find("ID")->storage()) ==
                   static_cast<std::int32_t>(r * 10));
        }
        std::cout << "  [OK] All 10 rows verified (row-oriented read).\n";

        // 7. Clean up.
        fs::remove_all(table_path);
        std::cout << "\n=== Table Write demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
