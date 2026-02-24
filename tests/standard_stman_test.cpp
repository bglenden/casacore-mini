#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_desc.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

namespace {

const std::filesystem::path kSourceDir = CASACORE_MINI_SOURCE_DIR;
const std::filesystem::path kFixtureDir = kSourceDir / "tests" / "fixtures" / "ssm_table";

int test_parse_table_dat_blob() {
    const auto table_dat_path = kFixtureDir / "table.dat";
    const auto td = casacore_mini::read_table_dat_full(table_dat_path.string());

    // The SSM test table has 1 SM (StandardStMan) with 4 columns.
    if (td.storage_managers.empty()) {
        std::cerr << "FAIL: no storage managers found\n";
        return 1;
    }

    // Find the StandardStMan entry.
    std::size_t ssm_idx = td.storage_managers.size();
    for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
        if (td.storage_managers[i].type_name == "StandardStMan") {
            ssm_idx = i;
            break;
        }
    }
    if (ssm_idx == td.storage_managers.size()) {
        std::cerr << "FAIL: no StandardStMan found in storage managers\n";
        return 1;
    }

    const auto& sm = td.storage_managers[ssm_idx];
    if (sm.data_blob.empty()) {
        std::cerr << "FAIL: StandardStMan data_blob is empty\n";
        return 1;
    }

    // Parse the SSM blob.
    const auto blob = casacore_mini::parse_ssm_blob(sm.data_blob);
    if (blob.column_offset.empty()) {
        std::cerr << "FAIL: column_offset is empty\n";
        return 1;
    }
    if (blob.col_index_map.empty()) {
        std::cerr << "FAIL: col_index_map is empty\n";
        return 1;
    }

    std::cout << "  SSM blob: name=" << blob.data_man_name
              << " offsets=" << blob.column_offset.size()
              << " index_map=" << blob.col_index_map.size() << "\n";

    return 0;
}

int test_parse_file_header() {
    const auto td = casacore_mini::read_table_dat_full((kFixtureDir / "table.dat").string());

    // Read the .f0 file.
    std::ifstream f0_in((kFixtureDir / "table.f0").string(), std::ios::binary);
    if (!f0_in) {
        std::cerr << "FAIL: cannot open table.f0\n";
        return 1;
    }
    std::vector<std::uint8_t> f0_data((std::istreambuf_iterator<char>(f0_in)),
                                      std::istreambuf_iterator<char>());

    const auto header = casacore_mini::parse_ssm_file_header(
        std::span<const std::uint8_t>(f0_data.data(), 512), td.big_endian);

    std::cout << "  SSM file header: bucket_size=" << header.bucket_size
              << " nr_buckets=" << header.nr_buckets << " nr_indices=" << header.nr_indices << "\n";

    if (header.bucket_size == 0) {
        std::cerr << "FAIL: bucket_size is 0\n";
        return 1;
    }
    if (header.nr_indices == 0) {
        std::cerr << "FAIL: nr_indices is 0\n";
        return 1;
    }

    return 0;
}

int test_read_cells() {
    const auto td = casacore_mini::read_table_dat_full((kFixtureDir / "table.dat").string());

    // Find SSM index.
    std::size_t ssm_idx = 0;
    for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
        if (td.storage_managers[i].type_name == "StandardStMan") {
            ssm_idx = i;
            break;
        }
    }

    casacore_mini::SsmReader reader;
    reader.open(kFixtureDir.string(), ssm_idx, td);

    if (!reader.is_open()) {
        std::cerr << "FAIL: reader not open after open()\n";
        return 1;
    }
    if (reader.column_count() != 4) {
        std::cerr << "FAIL: expected 4 columns, got " << reader.column_count() << "\n";
        return 1;
    }

    // Expected values from casacore_interop_tool: 5 rows, columns:
    //   id:Int    = 0, 10, 20, 30, 40
    //   value:Float = 0.0, 1.5, 3.0, 4.5, 6.0
    //   label:String = "row_0", "row_1", "row_2", "row_3", "row_4"
    //   dval:Double = 0.0, 3.14, 6.28, 9.42, 12.56

    int failures = 0;

    // Test Int column.
    for (std::uint64_t r = 0; r < 5; ++r) {
        const auto v = reader.read_cell("id", r);
        const auto* ip = std::get_if<std::int32_t>(&v);
        if (ip == nullptr || *ip != static_cast<std::int32_t>(r * 10)) {
            std::cerr << "FAIL: id[" << r << "] expected " << (r * 10) << " got "
                      << (ip != nullptr ? std::to_string(*ip) : "wrong type") << "\n";
            ++failures;
        }
    }

    // Test Float column.
    for (std::uint64_t r = 0; r < 5; ++r) {
        const auto v = reader.read_cell("value", r);
        const auto* fp = std::get_if<float>(&v);
        const float expected = static_cast<float>(r) * 1.5F;
        if (fp == nullptr || std::fabs(*fp - expected) > 1e-6F) {
            std::cerr << "FAIL: value[" << r << "] expected " << expected << " got "
                      << (fp != nullptr ? std::to_string(*fp) : "wrong type") << "\n";
            ++failures;
        }
    }

    // Test String column.
    for (std::uint64_t r = 0; r < 5; ++r) {
        const auto v = reader.read_cell("label", r);
        const auto* sp = std::get_if<std::string>(&v);
        const auto expected = "row_" + std::to_string(r);
        if (sp == nullptr || *sp != expected) {
            std::cerr << "FAIL: label[" << r << "] expected '" << expected << "' got '"
                      << (sp != nullptr ? *sp : "wrong type") << "'\n";
            ++failures;
        }
    }

    // Test Double column.
    for (std::uint64_t r = 0; r < 5; ++r) {
        const auto v = reader.read_cell("dval", r);
        const auto* dp = std::get_if<double>(&v);
        const double expected = static_cast<double>(r) * 3.14;
        if (dp == nullptr || std::fabs(*dp - expected) > 1e-10) {
            std::cerr << "FAIL: dval[" << r << "] expected " << expected << " got "
                      << (dp != nullptr ? std::to_string(*dp) : "wrong type") << "\n";
            ++failures;
        }
    }

    if (failures > 0) {
        std::cerr << "FAIL: " << failures << " cell value mismatches\n";
        return 1;
    }

    std::cout << "  All 20 cell values verified (5 rows x 4 columns)\n";
    return 0;
}

int test_out_of_range_row() {
    const auto td = casacore_mini::read_table_dat_full((kFixtureDir / "table.dat").string());

    std::size_t ssm_idx = 0;
    for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
        if (td.storage_managers[i].type_name == "StandardStMan") {
            ssm_idx = i;
            break;
        }
    }

    casacore_mini::SsmReader reader;
    reader.open(kFixtureDir.string(), ssm_idx, td);

    bool caught = false;
    try {
        static_cast<void>(reader.read_cell("id", 999));
    } catch (const std::runtime_error&) {
        caught = true;
    }
    if (!caught) {
        std::cerr << "FAIL: expected exception for out-of-range row\n";
        return 1;
    }

    return 0;
}

int test_unknown_column() {
    const auto td = casacore_mini::read_table_dat_full((kFixtureDir / "table.dat").string());

    std::size_t ssm_idx = 0;
    for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
        if (td.storage_managers[i].type_name == "StandardStMan") {
            ssm_idx = i;
            break;
        }
    }

    casacore_mini::SsmReader reader;
    reader.open(kFixtureDir.string(), ssm_idx, td);

    bool caught = false;
    try {
        static_cast<void>(reader.read_cell("nonexistent", 0));
    } catch (const std::runtime_error&) {
        caught = true;
    }
    if (!caught) {
        std::cerr << "FAIL: expected exception for unknown column\n";
        return 1;
    }

    return 0;
}

} // namespace

int main() noexcept {
    try {
        int failures = 0;

        std::cout << "test_parse_table_dat_blob:\n";
        failures += test_parse_table_dat_blob();

        std::cout << "test_parse_file_header:\n";
        failures += test_parse_file_header();

        std::cout << "test_read_cells:\n";
        failures += test_read_cells();

        std::cout << "test_out_of_range_row:\n";
        failures += test_out_of_range_row();

        std::cout << "test_unknown_column:\n";
        failures += test_unknown_column();

        if (failures > 0) {
            std::cerr << "\n" << failures << " test(s) FAILED\n";
            return 1;
        }

        std::cout << "\nAll SSM reader tests passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
}
