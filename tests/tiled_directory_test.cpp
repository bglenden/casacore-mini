#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_directory.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

/// Build a TiledColumnStMan table directory and read it back.
bool test_tiled_col_roundtrip() {
    namespace fs = std::filesystem;
    const auto tmp = fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_tiled_col_rt";
    fs::remove_all(tmp);

    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 10;
    full.big_endian = true;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tiledcol";

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::array;
    col.name = "data";
    col.data_type = casacore_mini::DataType::tp_float;
    col.dm_type = "TiledColumnStMan";
    col.dm_group = "TiledCol";
    col.type_string = "ArrayColumnDesc<Float    >";
    col.version = 1;
    col.ndim = 2;
    col.shape = {4, 8};
    col.options = 4;
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledColumnStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "data";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape = {4, 8};
    full.column_setups.push_back(cms);
    full.post_td_row_count = 10;

    casacore_mini::write_table_directory(tmp.string(), full);
    auto result = casacore_mini::read_table_directory(tmp.string());

    assert(result.table_dat.row_count == 10);
    assert(result.table_dat.table_desc.columns.size() == 1);
    assert(result.table_dat.table_desc.columns[0].name == "data");
    assert(result.table_dat.table_desc.columns[0].dm_type == "TiledColumnStMan");

    fs::remove_all(tmp);
    std::cerr << "  PASS: test_tiled_col_roundtrip\n";
    return true;
}

/// Build a TiledCellStMan table directory and read it back.
bool test_tiled_cell_roundtrip() {
    namespace fs = std::filesystem;
    const auto tmp = fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_tiled_cell_rt";
    fs::remove_all(tmp);

    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 5;
    full.big_endian = true;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tiledcell";

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::array;
    col.name = "map";
    col.data_type = casacore_mini::DataType::tp_float;
    col.dm_type = "TiledCellStMan";
    col.dm_group = "TiledCell";
    col.type_string = "ArrayColumnDesc<Float    >";
    col.version = 1;
    col.ndim = 2;
    col.shape = {32, 8};
    col.options = 4;
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledCellStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "map";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape = {32, 8};
    full.column_setups.push_back(cms);
    full.post_td_row_count = 5;

    casacore_mini::write_table_directory(tmp.string(), full);
    auto result = casacore_mini::read_table_directory(tmp.string());

    assert(result.table_dat.row_count == 5);
    assert(result.table_dat.table_desc.columns.size() == 1);
    assert(result.table_dat.table_desc.columns[0].name == "map");
    assert(result.table_dat.table_desc.columns[0].dm_type == "TiledCellStMan");

    fs::remove_all(tmp);
    std::cerr << "  PASS: test_tiled_cell_roundtrip\n";
    return true;
}

/// Build TiledShapeStMan and TiledDataStMan directories, read back.
bool test_tiled_shape_and_data_roundtrip() {
    namespace fs = std::filesystem;

    // TiledShapeStMan
    {
        const auto tmp =
            fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_tiled_shape_rt";
        fs::remove_all(tmp);

        casacore_mini::TableDatFull full;
        full.table_version = 2;
        full.row_count = 5;
        full.big_endian = true;
        full.table_type = "PlainTable";
        full.table_desc.version = 2;
        full.table_desc.name = "test_tiledshape";

        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::array;
        col.name = "vis";
        col.data_type = casacore_mini::DataType::tp_complex;
        col.dm_type = "TiledShapeStMan";
        col.dm_group = "TiledShape";
        col.type_string = "ArrayColumnDesc<Complex  >";
        col.version = 1;
        col.ndim = 2;
        full.table_desc.columns.push_back(col);

        casacore_mini::StorageManagerSetup sm;
        sm.type_name = "TiledShapeStMan";
        sm.sequence_number = 0;
        full.storage_managers.push_back(sm);

        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = "vis";
        cms.sequence_number = 0;
        full.column_setups.push_back(cms);
        full.post_td_row_count = 5;

        casacore_mini::write_table_directory(tmp.string(), full);
        auto result = casacore_mini::read_table_directory(tmp.string());

        assert(result.table_dat.table_desc.columns[0].dm_type == "TiledShapeStMan");
        fs::remove_all(tmp);
    }

    // TiledDataStMan
    {
        const auto tmp =
            fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_tiled_data_rt";
        fs::remove_all(tmp);

        casacore_mini::TableDatFull full;
        full.table_version = 2;
        full.row_count = 5;
        full.big_endian = true;
        full.table_type = "PlainTable";
        full.table_desc.version = 2;
        full.table_desc.name = "test_tileddata";

        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::array;
        col.name = "spectrum";
        col.data_type = casacore_mini::DataType::tp_float;
        col.dm_type = "TiledDataStMan";
        col.dm_group = "TiledData";
        col.type_string = "ArrayColumnDesc<Float    >";
        col.version = 1;
        col.ndim = 1;
        col.shape = {256};
        col.options = 4;
        full.table_desc.columns.push_back(col);

        casacore_mini::StorageManagerSetup sm;
        sm.type_name = "TiledDataStMan";
        sm.sequence_number = 0;
        full.storage_managers.push_back(sm);

        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = "spectrum";
        cms.sequence_number = 0;
        cms.has_shape = true;
        cms.shape = {256};
        full.column_setups.push_back(cms);
        full.post_td_row_count = 5;

        casacore_mini::write_table_directory(tmp.string(), full);
        auto result = casacore_mini::read_table_directory(tmp.string());

        assert(result.table_dat.table_desc.columns[0].dm_type == "TiledDataStMan");
        fs::remove_all(tmp);
    }

    std::cerr << "  PASS: test_tiled_shape_and_data_roundtrip\n";
    return true;
}

/// Verify that casacore-produced tiled tables (via pagedimage fixture) can be read.
bool test_read_pagedimage_fixture() {
    const auto fixture =
        std::string(CASACORE_MINI_SOURCE_DIR) + "/data/corpus/fixtures/pagedimage_coords";
    if (!std::filesystem::exists(fixture + "/table.dat")) {
        std::cerr << "  SKIP: test_read_pagedimage_fixture (fixture not present)\n";
        return true;
    }

    auto td = casacore_mini::read_table_directory(fixture);
    // pagedimage fixture has 1 row, 1 column ("map"), TiledCellStMan
    assert(td.table_dat.row_count == 1);
    assert(td.table_dat.table_desc.columns.size() == 1);
    assert(td.table_dat.table_desc.columns[0].name == "map");

    std::cerr << "  PASS: test_read_pagedimage_fixture\n";
    return true;
}

} // namespace

int main() noexcept {
    try {
        std::cerr << "tiled_directory_test:\n";
        if (!test_tiled_col_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_tiled_cell_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_tiled_shape_and_data_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_read_pagedimage_fixture()) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << "tiled_directory_test threw: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
