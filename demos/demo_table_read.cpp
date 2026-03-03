// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// demo_table_read.cpp -- Phase 7: table read/introspection transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/tables/Tables/test/tTable.cc
//     Table tab("tTable_tmp.data");
//     ScalarColumn<Int> ab2(tab, "ab");
//     ArrayColumn<float> arr1(tab, "arr1");
//     ab2.get(i, abval);
//
//   Source: casacore-original/tables/Tables/test/tTableRow.cc
//     ROTableRow rowx(tab, stringToVector("ab,arr1"));
//     rowx.get(i);
//     RORecordFieldPtr<Int> ab(rowx.record(), 0);
//
// This casacore-mini demo transliterates the same access modes: schema
// introspection, typed column reads, and row-oriented record reads.

#include <casacore_mini/table.hpp>
#include <casacore_mini/table_column.hpp>

#include "demo_check.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

using namespace casacore_mini;
namespace fs = std::filesystem;

int main() {
    try {
        std::cout << "=== casacore-mini Demo: Table Read (Phase 7) ===\n";

        // Open the SSM test fixture.
        const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
        const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

        std::cout << "\n--- Opening table: " << fixture << " ---\n";
        auto table = Table::open(fixture);

        // 1. Table structure.
        std::cout << "\n--- Table Structure ---\n";
        std::cout << "  Name:    " << table.table_name() << "\n";
        std::cout << "  Rows:    " << table.nrow() << "\n";
        std::cout << "  Columns: " << table.ncolumn() << "\n";
        DEMO_CHECK(table.nrow() == 5);
        DEMO_CHECK(table.ncolumn() == 4);

        std::cout << "\n  Column details:\n";
        for (const auto& col : table.columns()) {
            std::cout << "    " << col.name << " type=" << static_cast<int>(col.data_type)
                      << " kind=" << (col.kind == ColumnKind::scalar ? "scalar" : "array") << "\n";
        }

        // 2. Typed column access via ScalarColumn<T>.
        std::cout << "\n--- ScalarColumn<T> Access ---\n";
        ScalarColumn<std::int32_t> id_col(table, "id");
        ScalarColumn<float> val_col(table, "value");
        ScalarColumn<std::string> label_col(table, "label");
        ScalarColumn<double> dval_col(table, "dval");

        for (std::uint64_t r = 0; r < table.nrow(); ++r) {
            std::cout << "  row " << r << ": id=" << id_col.get(r) << " value=" << val_col.get(r)
                      << " label=\"" << label_col.get(r) << "\""
                      << " dval=" << dval_col.get(r) << "\n";
        }

        // Verify expected values.
        DEMO_CHECK(id_col.get(0) == 0);
        DEMO_CHECK(id_col.get(2) == 20);
        DEMO_CHECK(id_col.get(4) == 40);
        DEMO_CHECK(std::fabs(val_col.get(1) - 1.5F) < 1e-6F);
        DEMO_CHECK(label_col.get(3) == "row_3");
        DEMO_CHECK(std::fabs(dval_col.get(2) - 6.28) < 1e-10);

        std::cout << "  [OK] ScalarColumn values verified.\n";

        // 3. Row-oriented access via TableRow.
        std::cout << "\n--- TableRow Access ---\n";
        TableRow row(table);

        for (std::uint64_t r = 0; r < table.nrow(); ++r) {
            Record rec = row.get(r);
            std::cout << "  row " << r << ": {";
            bool first = true;
            for (const auto& [key, value] : rec.entries()) {
                if (!first)
                    std::cout << ", ";
                first = false;
                std::cout << key << "=";
                const auto& s = value.storage();
                if (const auto* pi = std::get_if<std::int32_t>(&s)) {
                    std::cout << *pi;
                } else if (const auto* pf = std::get_if<float>(&s)) {
                    std::cout << *pf;
                } else if (const auto* pd = std::get_if<double>(&s)) {
                    std::cout << *pd;
                } else if (const auto* ps = std::get_if<std::string>(&s)) {
                    std::cout << "\"" << *ps << "\"";
                }
            }
            std::cout << "}\n";
        }

        // Verify TableRow row 0.
        Record r0 = row.get(0);
        DEMO_CHECK(r0.size() == 4);
        DEMO_CHECK(std::get<std::int32_t>(r0.find("id")->storage()) == 0);
        DEMO_CHECK(std::get<std::string>(r0.find("label")->storage()) == "row_0");

        std::cout << "  [OK] TableRow access verified.\n";

        // 4. Table-level keywords (escape hatch).
        std::cout << "\n--- Table Keywords ---\n";
        const auto& kw = table.keywords();
        std::cout << "  " << kw.size() << " table-level keywords\n";

        // 5. Column info summary.
        std::cout << "\n--- Column Info ---\n";
        std::cout << "  " << table.ncolumn() << " columns, " << table.nrow() << " rows\n";

        std::cout << "\n=== Table Read demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
