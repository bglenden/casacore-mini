#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/table_desc.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::filesystem::path& test_root_dir() {
    static const auto k_root = []() {
        const auto* runner_temp = std::getenv("RUNNER_TEMP");
        if (runner_temp != nullptr && runner_temp[0] != '\0') {
            return std::filesystem::path(runner_temp) / "casacore_mini_ism_test";
        }
        return std::filesystem::temp_directory_path() / "casacore_mini_ism_test";
    }();
    return k_root;
}

/// Helper: build a 3-column ISM table (time:Double, antenna:Int, flag:Bool).
casacore_mini::TableDatFull make_test_table_dat(std::uint64_t row_count) {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = row_count;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "ism_test";

    auto make_col = [](const std::string& name, casacore_mini::DataType dtype) {
        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::scalar;
        col.name = name;
        col.data_type = dtype;
        col.dm_type = "IncrementalStMan";
        col.dm_group = "ISMData";
        col.version = 1;
        return col;
    };

    full.table_desc.columns.push_back(make_col("time", casacore_mini::DataType::tp_double));
    full.table_desc.columns.push_back(make_col("antenna", casacore_mini::DataType::tp_int));
    full.table_desc.columns.push_back(make_col("flag", casacore_mini::DataType::tp_bool));

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "IncrementalStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : full.table_desc.columns) {
        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        full.column_setups.push_back(cms);
    }

    full.post_td_row_count = row_count;
    return full;
}

/// Write ISM file and return its path.
std::string write_ism_test(const std::string& subdir, casacore_mini::IsmWriter& writer,
                           std::uint32_t seq_nr) {
    auto dir = test_root_dir() / subdir;
    std::filesystem::create_directories(dir);
    writer.write_file(dir.string(), seq_nr);
    return dir.string();
}

void test_single_bucket_roundtrip() {
    constexpr std::uint64_t kRows = 10;
    auto full = make_test_table_dat(kRows);

    casacore_mini::IsmWriter writer;
    writer.setup(full.table_desc.columns, kRows, false, "ISMData");

    for (std::uint64_t r = 0; r < kRows; ++r) {
        writer.write_cell(0, casacore_mini::CellValue{1000.0 + static_cast<double>(r)}, r);
        writer.write_cell(1, casacore_mini::CellValue{static_cast<std::int32_t>(r * 10)}, r);
        writer.write_cell(2, casacore_mini::CellValue{(r % 2) == 0}, r);
    }

    auto dir = write_ism_test("single_bucket", writer, 0);

    // Read back.
    casacore_mini::IsmReader reader;
    reader.open(dir, 0, full);

    for (std::uint64_t r = 0; r < kRows; ++r) {
        const auto vt = reader.read_cell("time", r);
        const auto* dp = std::get_if<double>(&vt);
        if (dp == nullptr || std::fabs(*dp - (1000.0 + static_cast<double>(r))) > 1e-10) {
            throw std::runtime_error("time[" + std::to_string(r) + "] mismatch");
        }

        const auto va = reader.read_cell("antenna", r);
        const auto* ip = std::get_if<std::int32_t>(&va);
        if (ip == nullptr || *ip != static_cast<std::int32_t>(r * 10)) {
            throw std::runtime_error("antenna[" + std::to_string(r) + "] mismatch");
        }

        const auto vf = reader.read_cell("flag", r);
        const auto* bp = std::get_if<bool>(&vf);
        if (bp == nullptr || *bp != ((r % 2) == 0)) {
            throw std::runtime_error("flag[" + std::to_string(r) + "] mismatch");
        }
    }
    std::cout << "  test_single_bucket_roundtrip: PASS (30 cells)\n";
}

void test_multi_bucket_roundtrip() {
    constexpr std::uint64_t kRows = 1000;
    auto full = make_test_table_dat(kRows);

    casacore_mini::IsmWriter writer;
    writer.setup(full.table_desc.columns, kRows, false, "ISMData");

    // Unique values per row to force many entries.
    for (std::uint64_t r = 0; r < kRows; ++r) {
        writer.write_cell(0, casacore_mini::CellValue{static_cast<double>(r) * 1.1}, r);
        writer.write_cell(1, casacore_mini::CellValue{static_cast<std::int32_t>(r)}, r);
        writer.write_cell(2, casacore_mini::CellValue{(r % 3) == 0}, r);
    }

    auto dir = write_ism_test("multi_bucket", writer, 0);

    // Read back.
    casacore_mini::IsmReader reader;
    reader.open(dir, 0, full);

    std::size_t cells_verified = 0;
    for (std::uint64_t r = 0; r < kRows; ++r) {
        const auto vt = reader.read_cell("time", r);
        const auto* dp = std::get_if<double>(&vt);
        if (dp == nullptr || std::fabs(*dp - static_cast<double>(r) * 1.1) > 1e-10) {
            throw std::runtime_error("time[" + std::to_string(r) + "] mismatch");
        }

        const auto va = reader.read_cell("antenna", r);
        const auto* ip = std::get_if<std::int32_t>(&va);
        if (ip == nullptr || *ip != static_cast<std::int32_t>(r)) {
            throw std::runtime_error("antenna[" + std::to_string(r) + "] mismatch");
        }

        const auto vf = reader.read_cell("flag", r);
        const auto* bp = std::get_if<bool>(&vf);
        if (bp == nullptr || *bp != ((r % 3) == 0)) {
            throw std::runtime_error("flag[" + std::to_string(r) + "] mismatch");
        }
        cells_verified += 3;
    }
    std::cout << "  test_multi_bucket_roundtrip: PASS (" << cells_verified << " cells)\n";
}

void test_multi_bucket_value_continuity() {
    // Test ISM "value applies forward" semantic across bucket boundaries.
    // Bool column changes at rows 0, 500, 900. All other rows inherit.
    constexpr std::uint64_t kRows = 1000;
    auto full = make_test_table_dat(kRows);

    casacore_mini::IsmWriter writer;
    writer.setup(full.table_desc.columns, kRows, false, "ISMData");

    for (std::uint64_t r = 0; r < kRows; ++r) {
        // time: unique per row to force multi-bucket.
        writer.write_cell(0, casacore_mini::CellValue{static_cast<double>(r)}, r);
        // antenna: unique per row.
        writer.write_cell(1, casacore_mini::CellValue{static_cast<std::int32_t>(r)}, r);
        // flag: changes only at specific rows.
        const bool flag_val = (r < 500) || (r >= 900);
        writer.write_cell(2, casacore_mini::CellValue{flag_val}, r);
    }

    auto dir = write_ism_test("value_continuity", writer, 0);

    casacore_mini::IsmReader reader;
    reader.open(dir, 0, full);

    for (std::uint64_t r = 0; r < kRows; ++r) {
        const bool expected_flag = (r < 500) || (r >= 900);

        const auto vf = reader.read_cell("flag", r);
        const auto* bp = std::get_if<bool>(&vf);
        if (bp == nullptr || *bp != expected_flag) {
            throw std::runtime_error("flag[" + std::to_string(r) +
                                     "] continuity mismatch: expected " +
                                     (expected_flag ? "true" : "false"));
        }
    }
    std::cout << "  test_multi_bucket_value_continuity: PASS (1000 flag cells)\n";
}

} // namespace

int main() {
    try {
        // Clean up test directory.
        std::filesystem::remove_all(test_root_dir());

        test_single_bucket_roundtrip();
        test_multi_bucket_roundtrip();
        test_multi_bucket_value_continuity();

        std::cout << "All ISM tests passed.\n";

        // Cleanup.
        std::filesystem::remove_all(test_root_dir());
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ISM test failed: " << e.what() << '\n';
        return 1;
    }
}
