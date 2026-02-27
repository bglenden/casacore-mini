#include "casacore_mini/table.hpp"

#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

/// Find the SM index for a column by matching its sequence number.
struct SmLookup {
    std::size_t sm_index = SIZE_MAX;
    std::string_view sm_type;
};

[[nodiscard]] SmLookup find_sm_for_column(const TableDatFull& td, std::string_view col_name) {
    for (const auto& cs : td.column_setups) {
        if (cs.column_name == col_name) {
            for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
                if (td.storage_managers[i].sequence_number == cs.sequence_number) {
                    return {i, td.storage_managers[i].type_name};
                }
            }
        }
    }
    return {};
}

/// Map DataType to the type string used in ColumnDesc.
[[nodiscard]] std::string make_type_string(DataType dt, ColumnKind kind) {
    const char* base = nullptr;
    switch (dt) {
    case DataType::tp_bool:
        base = "Bool    ";
        break;
    case DataType::tp_char:
        base = "Char    ";
        break;
    case DataType::tp_uchar:
        base = "uChar   ";
        break;
    case DataType::tp_short:
        base = "Short   ";
        break;
    case DataType::tp_ushort:
        base = "uShort  ";
        break;
    case DataType::tp_int:
        base = "Int     ";
        break;
    case DataType::tp_uint:
        base = "uInt    ";
        break;
    case DataType::tp_int64:
        base = "Int64   ";
        break;
    case DataType::tp_float:
        base = "float   ";
        break;
    case DataType::tp_double:
        base = "double  ";
        break;
    case DataType::tp_complex:
        base = "Complex ";
        break;
    case DataType::tp_dcomplex:
        base = "DComplex";
        break;
    case DataType::tp_string:
        base = "String  ";
        break;
    default:
        base = "unknown ";
        break;
    }

    std::string prefix = (kind == ColumnKind::array) ? "ArrayColumnDesc<" : "ScalarColumnDesc<";
    // casacore stores this token as prefix + padded type-id (no trailing '>').
    return prefix + base;
}

} // namespace

// ---------------------------------------------------------------------------
// Table::Impl -- internal state
// ---------------------------------------------------------------------------

struct Table::Impl {
    std::filesystem::path path;

    // Lazy SM readers (one per SM instance, like TSM already does).
    mutable std::vector<SsmReader> ssm_readers;
    mutable std::vector<IsmReader> ism_readers;
    mutable std::vector<TiledStManReader> tsm_readers;
    std::vector<ColumnDesc> writer_columns; // columns bound to active writer

    // For writable tables
    std::optional<IsmWriter> ism_writer;
    std::optional<TiledStManWriter> tsm_writer;
    std::optional<SsmWriter> ssm_writer;

    TableDirectory dir;
    std::uint32_t tsm_writer_seq = 0; // SM sequence number for TSM writer
    bool writable = false;
    mutable bool ssm_readers_initialized = false;
    mutable bool ism_readers_initialized = false;
    mutable bool tsm_readers_initialized = false;

    void ensure_ssm_readers() const {
        if (ssm_readers_initialized)
            return;
        ssm_readers_initialized = true;
        for (std::size_t i = 0; i < dir.table_dat.storage_managers.size(); ++i) {
            if (dir.table_dat.storage_managers[i].type_name.find("StandardStMan") !=
                std::string::npos) {
                try {
                    ssm_readers.emplace_back();
                    ssm_readers.back().open(path.string(), i, dir.table_dat);
                } catch (const std::exception& ex) {
                    std::cerr << "warning: SSM reader open failed for SM[" << i
                              << "]: " << ex.what() << "\n";
                    ssm_readers.pop_back();
                } catch (...) {
                    std::cerr << "warning: SSM reader open failed for SM[" << i
                              << "] (unknown error)\n";
                    ssm_readers.pop_back();
                }
            }
        }
    }

    void ensure_ism_readers() const {
        if (ism_readers_initialized)
            return;
        ism_readers_initialized = true;
        for (std::size_t i = 0; i < dir.table_dat.storage_managers.size(); ++i) {
            if (dir.table_dat.storage_managers[i].type_name.find("IncrementalStMan") !=
                std::string::npos) {
                try {
                    ism_readers.emplace_back();
                    ism_readers.back().open(path.string(), i, dir.table_dat);
                } catch (...) {
                    ism_readers.pop_back();
                }
            }
        }
    }

    void ensure_tsm_readers() const {
        if (tsm_readers_initialized)
            return;
        tsm_readers_initialized = true;
        for (std::size_t i = 0; i < dir.table_dat.storage_managers.size(); ++i) {
            if (dir.table_dat.storage_managers[i].type_name.find("Tiled") != std::string::npos) {
                try {
                    tsm_readers.emplace_back();
                    tsm_readers.back().open(path.string(), i, dir.table_dat);
                } catch (const std::exception& ex) {
                    std::cerr << "warning: TSM reader open failed for SM[" << i
                              << "]: " << ex.what() << "\n";
                    tsm_readers.pop_back();
                } catch (...) {
                    std::cerr << "warning: TSM reader open failed for SM[" << i
                              << "] (unknown error)\n";
                    tsm_readers.pop_back();
                }
            }
        }
    }

    [[nodiscard]] SsmReader* find_ssm_for_column(std::string_view col_name) const {
        ensure_ssm_readers();
        for (auto& reader : ssm_readers) {
            if (reader.has_column(col_name)) {
                return &reader;
            }
        }
        return nullptr;
    }

    [[nodiscard]] IsmReader* find_ism_for_column(std::string_view col_name) const {
        ensure_ism_readers();
        for (auto& reader : ism_readers) {
            if (reader.has_column(col_name)) {
                return &reader;
            }
        }
        return nullptr;
    }

    [[nodiscard]] TiledStManReader* find_tsm_for_column(std::string_view col_name) const {
        ensure_tsm_readers();
        for (auto& reader : tsm_readers) {
            if (reader.has_column(col_name)) {
                return &reader;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool is_ssm_column(std::string_view col_name) const {
        auto lookup = find_sm_for_column(dir.table_dat, col_name);
        return lookup.sm_type.find("StandardStMan") != std::string_view::npos;
    }

    [[nodiscard]] bool is_ism_column(std::string_view col_name) const {
        auto lookup = find_sm_for_column(dir.table_dat, col_name);
        return lookup.sm_type.find("IncrementalStMan") != std::string_view::npos;
    }

    [[nodiscard]] bool is_tsm_column(std::string_view col_name) const {
        auto lookup = find_sm_for_column(dir.table_dat, col_name);
        return lookup.sm_type.find("Tiled") != std::string_view::npos;
    }
};

// ---------------------------------------------------------------------------
// Table static factories
// ---------------------------------------------------------------------------

Table Table::open(const std::filesystem::path& path) {
    Table t;
    t.impl_ = std::make_shared<Impl>();
    t.impl_->path = path;
    t.impl_->dir = read_table_directory(path.string());
    t.impl_->writable = false;
    if (t.impl_->dir.table_dat.row_count > 0) {
        for (auto& col : t.impl_->dir.table_dat.table_desc.columns) {
            if (col.kind != ColumnKind::array || !col.shape.empty() ||
                !t.impl_->is_tsm_column(col.name)) {
                continue;
            }
            if (auto* tsm = t.impl_->find_tsm_for_column(col.name)) {
                const auto inferred_shape = tsm->cell_shape(col.name, 0);
                if (!inferred_shape.empty()) {
                    col.shape = inferred_shape;
                    col.ndim = static_cast<std::int32_t>(inferred_shape.size());
                    for (auto& setup : t.impl_->dir.table_dat.column_setups) {
                        if (setup.column_name == col.name) {
                            setup.has_shape = true;
                            setup.shape = inferred_shape;
                            break;
                        }
                    }
                }
            }
        }
    }
    return t;
}

Table Table::open_rw(const std::filesystem::path& path) {
    Table t;
    t.impl_ = std::make_shared<Impl>();
    t.impl_->path = path;
    t.impl_->dir = read_table_directory(path.string());
    t.impl_->writable = true;
    if (t.impl_->dir.table_dat.row_count > 0) {
        for (auto& col : t.impl_->dir.table_dat.table_desc.columns) {
            if (col.kind != ColumnKind::array || !col.shape.empty() ||
                !t.impl_->is_tsm_column(col.name)) {
                continue;
            }
            if (auto* tsm = t.impl_->find_tsm_for_column(col.name)) {
                const auto inferred_shape = tsm->cell_shape(col.name, 0);
                if (!inferred_shape.empty()) {
                    col.shape = inferred_shape;
                    col.ndim = static_cast<std::int32_t>(inferred_shape.size());
                    for (auto& setup : t.impl_->dir.table_dat.column_setups) {
                        if (setup.column_name == col.name) {
                            setup.has_shape = true;
                            setup.shape = inferred_shape;
                            break;
                        }
                    }
                }
            }
        }
    }

    auto& td = t.impl_->dir.table_dat;
    auto nrow = td.row_count;

    // Find SSM columns.
    std::vector<ColumnDesc> ssm_cols;
    for (const auto& col : td.table_desc.columns) {
        auto lookup = find_sm_for_column(td, col.name);
        if (lookup.sm_type.find("StandardStMan") != std::string_view::npos) {
            ssm_cols.push_back(col);
        }
    }

    if (ssm_cols.empty()) {
        return t;
    }

    // Set up readers and writer.
    t.impl_->ensure_ssm_readers();

    SsmWriter writer;
    writer.setup(ssm_cols, nrow, td.big_endian, "StandardStMan");

    // Copy all cell data from readers to writer.
    for (std::size_t ci = 0; ci < ssm_cols.size(); ++ci) {
        const auto& col = ssm_cols[ci];
        auto* reader = t.impl_->find_ssm_for_column(col.name);
        if (reader == nullptr)
            continue;
        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (col.kind == ColumnKind::scalar) {
                writer.write_cell(ci, reader->read_cell(col.name, r), r);
            } else if (col.kind == ColumnKind::array) {
                if (reader->is_indirect(col.name)) {
                    auto offset = reader->read_indirect_offset(col.name, r);
                    writer.write_indirect_offset(ci, r, offset);
                } else {
                    auto raw = reader->read_array_raw(col.name, r);
                    writer.write_array_raw(ci, raw, r);
                }
            }
        }
    }

    t.impl_->ssm_writer = std::move(writer);
    t.impl_->writer_columns = ssm_cols;
    return t;
}

Table Table::create(const std::filesystem::path& path, const std::vector<TableColumnSpec>& columns,
                    std::uint64_t nrows) {
    // Build TableDatFull.
    TableDatFull td;
    td.table_version = 2;
    td.row_count = nrows;
    td.big_endian = true;
    td.table_type = "PlainTable";
    td.table_desc.version = 2;
    // casacore writes an empty TableDesc name for on-disk tables.
    td.table_desc.name = "";
    td.post_td_row_count = nrows;

    // Build column descriptors.
    for (const auto& cs : columns) {
        ColumnDesc cd;
        cd.kind = cs.kind;
        cd.name = cs.name;
        cd.data_type = cs.data_type;
        cd.comment = cs.comment;
        cd.type_string = make_type_string(cs.data_type, cs.kind);
        cd.dm_type = "StandardStMan";
        cd.dm_group = "StandardStMan";
        if (cs.kind == ColumnKind::array && !cs.shape.empty()) {
            cd.ndim = static_cast<std::int32_t>(cs.shape.size());
            cd.shape = cs.shape;
            // Fixed-shape arrays are direct in SSM (Direct|FixedShape).
            cd.options = 5;
        } else if (cs.kind == ColumnKind::array) {
            cd.ndim = -1; // variable shape
        }
        cd.version = 1;
        td.table_desc.columns.push_back(std::move(cd));
    }

    // Single SSM storage manager for all columns.
    StorageManagerSetup sm;
    sm.type_name = "StandardStMan";
    sm.sequence_number = 0;
    td.storage_managers.push_back(sm);

    for (const auto& cd : td.table_desc.columns) {
        ColumnManagerSetup cms;
        cms.column_name = cd.name;
        cms.sequence_number = 0;
        if (cd.kind == ColumnKind::array && !cd.shape.empty()) {
            cms.has_shape = true;
            cms.shape = cd.shape;
        }
        td.column_setups.push_back(std::move(cms));
    }

    // Set up SSM writer.
    SsmWriter ssm_wr;
    ssm_wr.setup(td.table_desc.columns, nrows, td.big_endian, "StandardStMan");

    // Write table directory.
    td.storage_managers[0].data_blob = ssm_wr.make_blob();
    write_table_directory(path.string(), td);

    // Write .f0 file.
    ssm_wr.write_file(path.string(), 0);
    ssm_wr.write_indirect_file(path.string(), 0);

    // Write table.info.
    {
        auto info_path = path / "table.info";
        std::ofstream info(info_path);
        if (info) {
            info << "Type = PlainTable\n";
        }
    }

    Table t;
    t.impl_ = std::make_shared<Impl>();
    t.impl_->path = path;
    t.impl_->dir = read_table_directory(path.string());
    t.impl_->writable = true;
    t.impl_->ssm_writer = std::move(ssm_wr);
    t.impl_->writer_columns = td.table_desc.columns;
    return t;
}

Table Table::create(const std::filesystem::path& path, const std::vector<TableColumnSpec>& columns,
                    std::uint64_t nrows, const TableCreateOptions& options) {
    // Build TableDatFull.
    TableDatFull td;
    td.table_version = 2;
    td.row_count = nrows;
    td.big_endian = options.big_endian;
    td.table_type = "PlainTable";
    td.table_desc.version = 2;
    // casacore writes an empty TableDesc name for on-disk tables.
    td.table_desc.name = "";
    td.post_td_row_count = nrows;

    // Copy keywords.
    td.table_desc.keywords = options.table_keywords;
    td.table_desc.private_keywords = options.private_keywords;

    // Build column descriptors.
    for (std::size_t ci = 0; ci < columns.size(); ++ci) {
        const auto& cs = columns[ci];
        ColumnDesc cd;
        cd.kind = cs.kind;
        cd.name = cs.name;
        cd.data_type = cs.data_type;
        cd.comment = cs.comment;
        cd.type_string = make_type_string(cs.data_type, cs.kind);
        cd.dm_type = options.sm_type;
        cd.dm_group = options.sm_group;
        if (cs.kind == ColumnKind::array && !cs.shape.empty()) {
            cd.ndim = static_cast<std::int32_t>(cs.shape.size());
            cd.shape = cs.shape;
            // Fixed-shape arrays are direct in SSM (Direct|FixedShape).
            cd.options = 5;
        } else if (cs.kind == ColumnKind::array) {
            cd.ndim = -1; // variable shape
        }
        cd.version = 1;
        // Column keywords.
        if (ci < options.column_keywords.size()) {
            cd.keywords = options.column_keywords[ci];
        }
        td.table_desc.columns.push_back(std::move(cd));
    }

    // Single storage manager for all columns.
    StorageManagerSetup sm;
    sm.type_name = options.sm_type;
    sm.sequence_number = 0;
    td.storage_managers.push_back(sm);

    for (const auto& cd : td.table_desc.columns) {
        ColumnManagerSetup cms;
        cms.column_name = cd.name;
        cms.sequence_number = 0;
        if (cd.kind == ColumnKind::array && !cd.shape.empty()) {
            cms.has_shape = true;
            cms.shape = cd.shape;
        }
        td.column_setups.push_back(std::move(cms));
    }

    const bool is_ism = (options.sm_type == "IncrementalStMan");
    const bool is_tsm = (options.sm_type.find("Tiled") != std::string::npos);

    if (is_ism) {
        // ISM path.
        IsmWriter ism_wr;
        ism_wr.setup(td.table_desc.columns, nrows, td.big_endian, options.sm_group);

        td.storage_managers[0].data_blob = ism_wr.make_blob();
        write_table_directory(path.string(), td);
        ism_wr.write_file(path.string(), 0);

        // Write table.info.
        {
            auto info_path = path / "table.info";
            std::ofstream info(info_path);
            if (info) {
                info << "Type = PlainTable\n";
            }
        }

        Table t;
        t.impl_ = std::make_shared<Impl>();
        t.impl_->path = path;
        t.impl_->dir = read_table_directory(path.string());
        t.impl_->writable = true;
        t.impl_->ism_writer = std::move(ism_wr);
        t.impl_->writer_columns = td.table_desc.columns;
        return t;
    }

    if (is_tsm) {
        // TSM path.
        TiledStManWriter tsm_wr;
        tsm_wr.setup(options.sm_type, options.sm_group, td.table_desc.columns, nrows);

        td.storage_managers[0].data_blob = tsm_wr.make_blob();
        write_table_directory(path.string(), td);
        tsm_wr.write_files(path.string(), 0);

        // Write table.info.
        {
            auto info_path = path / "table.info";
            std::ofstream info(info_path);
            if (info) {
                info << "Type = PlainTable\n";
            }
        }

        Table t;
        t.impl_ = std::make_shared<Impl>();
        t.impl_->path = path;
        t.impl_->dir = read_table_directory(path.string());
        t.impl_->writable = true;
        t.impl_->tsm_writer = std::move(tsm_wr);
        t.impl_->tsm_writer_seq = 0;
        t.impl_->writer_columns = td.table_desc.columns;
        return t;
    }

    // Default SSM path.
    SsmWriter ssm_wr;
    ssm_wr.setup(td.table_desc.columns, nrows, td.big_endian, options.sm_group);

    td.storage_managers[0].data_blob = ssm_wr.make_blob();
    write_table_directory(path.string(), td);
    ssm_wr.write_file(path.string(), 0);
    ssm_wr.write_indirect_file(path.string(), 0);

    // Write table.info.
    {
        auto info_path = path / "table.info";
        std::ofstream info(info_path);
        if (info) {
            info << "Type = PlainTable\n";
        }
    }

    Table t;
    t.impl_ = std::make_shared<Impl>();
    t.impl_->path = path;
    t.impl_->dir = read_table_directory(path.string());
    t.impl_->writable = true;
    t.impl_->ssm_writer = std::move(ssm_wr);
    t.impl_->writer_columns = td.table_desc.columns;
    return t;
}

// ---------------------------------------------------------------------------
// Table accessors
// ---------------------------------------------------------------------------

std::uint64_t Table::nrow() const {
    return impl_->dir.table_dat.row_count;
}

std::size_t Table::ncolumn() const {
    return impl_->dir.table_dat.table_desc.columns.size();
}

const std::vector<ColumnDesc>& Table::columns() const {
    return impl_->dir.table_dat.table_desc.columns;
}

const Record& Table::keywords() const {
    return impl_->dir.table_dat.table_desc.keywords;
}

Record& Table::rw_keywords() {
    return impl_->dir.table_dat.table_desc.keywords;
}

std::string Table::table_name() const {
    return impl_->path.filename().string();
}

const std::filesystem::path& Table::path() const {
    return impl_->path;
}

TableDirectory& Table::directory() {
    return impl_->dir;
}

const TableDirectory& Table::directory() const {
    return impl_->dir;
}

const TableDatFull& Table::table_dat() const {
    return impl_->dir.table_dat;
}

TableDatFull& Table::rw_table_dat() {
    return impl_->dir.table_dat;
}

bool Table::is_writable() const {
    return impl_->writable;
}

void Table::flush() {
    if (!impl_->writable)
        return;

    if (impl_->ssm_writer.has_value()) {
        impl_->ssm_writer->write_file(impl_->path.string(), 0);
        impl_->ssm_writer->write_indirect_file(impl_->path.string(), 0);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = impl_->ssm_writer->make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset reader cache.
        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        impl_->ism_writer->write_file(impl_->path.string(), 0);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = impl_->ism_writer->make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset reader cache.
        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        impl_->tsm_writer->write_files(impl_->path.string(), impl_->tsm_writer_seq);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = impl_->tsm_writer->make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset TSM reader cache.
        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    }
}

// ---------------------------------------------------------------------------
// Column descriptor lookup
// ---------------------------------------------------------------------------

const ColumnDesc* Table::find_column_desc(std::string_view name) const {
    for (const auto& cd : impl_->dir.table_dat.table_desc.columns) {
        if (cd.name == name)
            return &cd;
    }
    return nullptr;
}

std::size_t Table::find_column_index(std::string_view name) const {
    const auto& cols = impl_->dir.table_dat.table_desc.columns;
    for (std::size_t i = 0; i < cols.size(); ++i) {
        if (cols[i].name == name)
            return i;
    }
    throw std::runtime_error("Table: column '" + std::string(name) + "' not found");
}

// ---------------------------------------------------------------------------
// SM-routing: read
// ---------------------------------------------------------------------------

CellValue Table::read_scalar_cell(std::string_view col_name, std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        return ssm->read_cell(col_name, row);
    }
    if (auto* ism = impl_->find_ism_for_column(col_name)) {
        return ism->read_cell(col_name, row);
    }
    throw std::runtime_error("Table: no suitable SM reader for scalar column '" +
                             std::string(col_name) + "'");
}

std::vector<double> Table::read_array_double_cell(std::string_view col_name,
                                                  std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_double(col_name, row);
        }
        return ssm->read_array_double(col_name, row);
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        auto raw = tsm->read_raw_cell(col_name, row);
        std::vector<double> result(raw.size() / sizeof(double));
        std::memcpy(result.data(), raw.data(), raw.size());
        return result;
    }
    throw std::runtime_error("Table: no suitable SM reader for array column '" +
                             std::string(col_name) + "'");
}

std::vector<float> Table::read_array_float_cell(std::string_view col_name,
                                                std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_float(col_name, row);
        }
        return ssm->read_array_float(col_name, row);
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        return tsm->read_float_cell(col_name, row);
    }
    throw std::runtime_error("Table: no suitable SM reader for array column '" +
                             std::string(col_name) + "'");
}

std::vector<bool> Table::read_array_bool_cell(std::string_view col_name, std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_bool(col_name, row);
        }
        auto raw = ssm->read_array_raw(col_name, row);
        std::vector<bool> result(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i) {
            result[i] = raw[i] != 0;
        }
        return result;
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        auto raw = tsm->read_raw_cell(col_name, row);
        std::vector<bool> result(raw.size());
        for (std::size_t i = 0; i < raw.size(); ++i) {
            result[i] = raw[i] != 0;
        }
        return result;
    }
    throw std::runtime_error("Table: no suitable SM reader for bool array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::int32_t> Table::read_array_int_cell(std::string_view col_name,
                                                     std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_int(col_name, row);
        }
        auto raw = ssm->read_array_raw(col_name, row);
        const bool big_endian = impl_->dir.table_dat.big_endian;
        std::vector<std::int32_t> result(raw.size() / sizeof(std::int32_t));
        for (std::size_t i = 0; i < result.size(); ++i) {
            std::int32_t val{};
            std::memcpy(&val, raw.data() + i * sizeof(std::int32_t), sizeof(std::int32_t));
            if (big_endian) {
                auto* bytes = reinterpret_cast<std::uint8_t*>(&val);
                std::swap(bytes[0], bytes[3]);
                std::swap(bytes[1], bytes[2]);
            }
            result[i] = val;
        }
        return result;
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        return tsm->read_int_cell(col_name, row);
    }
    throw std::runtime_error("Table: no suitable SM reader for int array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::uint8_t> Table::read_array_raw_cell(std::string_view col_name,
                                                     std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        return ssm->read_array_raw(col_name, row);
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        return tsm->read_raw_cell(col_name, row);
    }
    throw std::runtime_error("Table: no suitable SM reader for raw array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::complex<float>> Table::read_array_complex_cell(std::string_view col_name,
                                                                std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_complex(col_name, row);
        }
        auto raw = ssm->read_array_raw(col_name, row);
        const bool be = impl_->dir.table_dat.big_endian;
        std::vector<std::complex<float>> result(raw.size() / 8);
        for (std::size_t i = 0; i < result.size(); ++i) {
            float re{};
            float im{};
            std::memcpy(&re, raw.data() + i * 8, 4);
            std::memcpy(&im, raw.data() + i * 8 + 4, 4);
            if (be) {
                auto swap32 = [](float& v) {
                    auto* b = reinterpret_cast<std::uint8_t*>(&v);
                    std::swap(b[0], b[3]);
                    std::swap(b[1], b[2]);
                };
                swap32(re);
                swap32(im);
            }
            result[i] = {re, im};
        }
        return result;
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        auto raw = tsm->read_raw_cell(col_name, row);
        std::vector<std::complex<float>> result(raw.size() / 8);
        for (std::size_t i = 0; i < result.size(); ++i) {
            float re{};
            float im{};
            std::memcpy(&re, raw.data() + i * 8, 4);
            std::memcpy(&im, raw.data() + i * 8 + 4, 4);
            result[i] = {re, im};
        }
        return result;
    }
    throw std::runtime_error("Table: no suitable SM reader for complex array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::complex<double>> Table::read_array_dcomplex_cell(std::string_view col_name,
                                                                  std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_dcomplex(col_name, row);
        }
        auto raw = ssm->read_array_raw(col_name, row);
        const bool be = impl_->dir.table_dat.big_endian;
        std::vector<std::complex<double>> result(raw.size() / 16);
        for (std::size_t i = 0; i < result.size(); ++i) {
            double re{};
            double im{};
            std::memcpy(&re, raw.data() + i * 16, 8);
            std::memcpy(&im, raw.data() + i * 16 + 8, 8);
            if (be) {
                auto swap64 = [](double& v) {
                    auto* b = reinterpret_cast<std::uint8_t*>(&v);
                    std::swap(b[0], b[7]);
                    std::swap(b[1], b[6]);
                    std::swap(b[2], b[5]);
                    std::swap(b[3], b[4]);
                };
                swap64(re);
                swap64(im);
            }
            result[i] = {re, im};
        }
        return result;
    }
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        auto raw = tsm->read_raw_cell(col_name, row);
        std::vector<std::complex<double>> result(raw.size() / 16);
        for (std::size_t i = 0; i < result.size(); ++i) {
            double re{};
            double im{};
            std::memcpy(&re, raw.data() + i * 16, 8);
            std::memcpy(&im, raw.data() + i * 16 + 8, 8);
            result[i] = {re, im};
        }
        return result;
    }
    throw std::runtime_error("Table: no suitable SM reader for dcomplex array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::string> Table::read_array_string_cell(std::string_view col_name,
                                                       std::uint64_t row) const {
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_string(col_name, row);
        }
    }
    throw std::runtime_error("Table: no suitable SM reader for string array column '" +
                             std::string(col_name) + "'");
}

std::vector<std::int64_t> Table::cell_shape(std::string_view col_name, std::uint64_t row) const {
    // TSM columns may be variable-shape even if ColumnDesc carries a nominal
    // shape; consult TSM first.
    if (auto* tsm = impl_->find_tsm_for_column(col_name)) {
        auto shape = tsm->cell_shape(col_name, row);
        if (!shape.empty()) {
            return shape;
        }
    }
    // Check fixed shape first.
    const auto* cd = find_column_desc(col_name);
    if (cd != nullptr && !cd->shape.empty()) {
        return cd->shape;
    }
    // Indirect array: read shape from SSM .f0i header.
    if (auto* ssm = impl_->find_ssm_for_column(col_name)) {
        if (ssm->is_indirect(col_name)) {
            return ssm->read_indirect_shape(col_name, row);
        }
    }
    return {};
}

bool Table::is_big_endian() const {
    return impl_->dir.table_dat.big_endian;
}

// ---------------------------------------------------------------------------
// SM-routing: write
// ---------------------------------------------------------------------------

void Table::write_scalar_cell(std::string_view col_name, std::uint64_t row,
                              const CellValue& value) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    // Find column index in writer's column list.
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->ssm_writer.has_value()) {
                impl_->ssm_writer->write_cell(i, value, row);
            } else if (impl_->ism_writer.has_value()) {
                impl_->ism_writer->write_cell(i, value, row);
            } else {
                throw std::runtime_error("Table: no scalar writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_scalar_cell: column '" + std::string(col_name) +
                             "' not found in writer");
}

void Table::write_array_float_cell(std::string_view col_name, std::uint64_t row,
                                   const std::vector<float>& values) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->ssm_writer.has_value()) {
                impl_->ssm_writer->write_array_float(i, values, row);
            } else if (impl_->tsm_writer.has_value()) {
                impl_->tsm_writer->write_float_cell(i, values, row);
            } else {
                throw std::runtime_error("Table: no float array writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_array_float_cell: column not found");
}

void Table::write_array_double_cell(std::string_view col_name, std::uint64_t row,
                                    const std::vector<double>& values) {
    if (!impl_->writable || !impl_->ssm_writer.has_value()) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            impl_->ssm_writer->write_array_double(i, values, row);
            return;
        }
    }
    throw std::runtime_error("Table::write_array_double_cell: column not found");
}

void Table::write_array_bool_cell(std::string_view col_name, std::uint64_t row,
                                  const std::vector<bool>& values,
                                  const std::vector<std::int32_t>& shape) {
    if (!impl_->writable || !impl_->ssm_writer.has_value()) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            // Bool arrays stored as 1 byte per element in indirect file.
            std::vector<std::uint8_t> raw(values.size());
            for (std::size_t j = 0; j < values.size(); ++j) {
                raw[j] = values[j] ? 1 : 0;
            }
            impl_->ssm_writer->write_indirect_array(i, shape, raw, row);
            return;
        }
    }
    throw std::runtime_error("Table::write_array_bool_cell: column not found");
}

void Table::write_array_complex_cell(std::string_view col_name, std::uint64_t row,
                                     const std::vector<std::complex<float>>& values,
                                     const std::vector<std::int32_t>& shape) {
    if (!impl_->writable || !impl_->ssm_writer.has_value()) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            // Complex = 2 floats = 8 bytes per element.
            std::vector<std::uint8_t> raw(values.size() * 8);
            auto* p = raw.data();
            for (const auto& c : values) {
                float re = c.real();
                float im = c.imag();
                std::memcpy(p, &re, 4);
                std::memcpy(p + 4, &im, 4);
                p += 8;
            }
            impl_->ssm_writer->write_indirect_array(i, shape, raw, row);
            return;
        }
    }
    throw std::runtime_error("Table::write_array_complex_cell: column not found");
}

void Table::write_array_int_cell(std::string_view col_name, std::uint64_t row,
                                 const std::vector<std::int32_t>& values) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->tsm_writer.has_value()) {
                impl_->tsm_writer->write_int_cell(i, values, row);
            } else {
                throw std::runtime_error("Table: no int array writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_array_int_cell: column not found");
}

void Table::write_array_raw_cell(std::string_view col_name, std::uint64_t row,
                                 const std::vector<std::uint8_t>& data) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->tsm_writer.has_value()) {
                impl_->tsm_writer->write_raw_cell(i, data, row);
            } else {
                throw std::runtime_error("Table: no raw array writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_array_raw_cell: column not found");
}

void Table::write_indirect_array(std::string_view col_name, std::uint64_t row,
                                 const std::vector<std::int32_t>& shape,
                                 const std::vector<std::uint8_t>& data) {
    if (!impl_->writable || !impl_->ssm_writer.has_value()) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            impl_->ssm_writer->write_indirect_array(i, shape, data, row);
            return;
        }
    }
    throw std::runtime_error("Table::write_indirect_array: column not found");
}

// ---------------------------------------------------------------------------
// CellValue <-> RecordValue conversion helpers
// ---------------------------------------------------------------------------

namespace {

/// Convert a CellValue to a RecordValue.
RecordValue cell_to_record_value(const CellValue& cell) {
    struct Converter {
        RecordValue operator()(bool v) const {
            return RecordValue(v);
        }
        RecordValue operator()(std::int32_t v) const {
            return RecordValue(v);
        }
        RecordValue operator()(std::uint32_t v) const {
            return RecordValue(v);
        }
        RecordValue operator()(std::int64_t v) const {
            return RecordValue(v);
        }
        RecordValue operator()(float v) const {
            return RecordValue(v);
        }
        RecordValue operator()(double v) const {
            return RecordValue(v);
        }
        RecordValue operator()(std::complex<float> v) const {
            return RecordValue(v);
        }
        RecordValue operator()(std::complex<double> v) const {
            return RecordValue(v);
        }
        RecordValue operator()(const std::string& v) const {
            return RecordValue(v);
        }
    };
    return std::visit(Converter{}, cell);
}

/// Try to convert a RecordValue to a CellValue. Returns true on success.
bool record_to_cell_value(const RecordValue::storage_type& storage, CellValue& out) {
    if (const auto* v = std::get_if<bool>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::int32_t>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::uint32_t>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::int64_t>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<float>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<double>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::complex<float>>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::complex<double>>(&storage)) {
        out = *v;
        return true;
    }
    if (const auto* v = std::get_if<std::string>(&storage)) {
        out = *v;
        return true;
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// TableRow
// ---------------------------------------------------------------------------

TableRow::TableRow(Table& table) : table_(&table) {
    for (const auto& cd : table.columns()) {
        if (cd.kind == ColumnKind::scalar) {
            column_names_.push_back(cd.name);
        }
    }
}

TableRow::TableRow(Table& table, const std::vector<std::string>& column_names)
    : table_(&table), column_names_(column_names) {}

Record TableRow::get(std::uint64_t row) const {
    Record rec;
    for (const auto& col_name : column_names_) {
        const auto* cd = table_->find_column_desc(col_name);
        if (cd == nullptr || cd->kind != ColumnKind::scalar)
            continue;

        auto cell = table_->read_scalar_cell(col_name, row);
        rec.set(col_name, cell_to_record_value(cell));
    }
    return rec;
}

void TableRow::put(std::uint64_t row, const Record& values) {
    for (const auto& [key, val] : values.entries()) {
        const auto* cd = table_->find_column_desc(key);
        if (cd == nullptr || cd->kind != ColumnKind::scalar)
            continue;

        CellValue cv;
        if (record_to_cell_value(val.storage(), cv)) {
            table_->write_scalar_cell(key, row, cv);
        }
    }
}

} // namespace casacore_mini
