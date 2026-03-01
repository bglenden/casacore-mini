#include "casacore_mini/table.hpp"

#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <algorithm>
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

struct SmClassInfo {
    std::size_t sm_index = SIZE_MAX;
    std::uint32_t sequence_number = 0;
    std::string sm_type;
    std::string dm_group;
    std::vector<ColumnDesc> columns;
};

[[nodiscard]] SmClassInfo collect_sm_class(const TableDatFull& td, std::string_view type_token) {
    SmClassInfo info;
    for (const auto& col : td.table_desc.columns) {
        auto lookup = find_sm_for_column(td, col.name);
        if (lookup.sm_type.find(type_token) == std::string_view::npos) {
            continue;
        }
        if (lookup.sm_index == SIZE_MAX || lookup.sm_index >= td.storage_managers.size()) {
            continue;
        }
        if (info.sm_index == SIZE_MAX) {
            info.sm_index = lookup.sm_index;
            info.sequence_number = td.storage_managers[lookup.sm_index].sequence_number;
            info.sm_type = std::string(lookup.sm_type);
        }
        if (lookup.sm_index != info.sm_index) {
            continue;
        }
        if (info.dm_group.empty() && !col.dm_group.empty()) {
            info.dm_group = col.dm_group;
        }
        info.columns.push_back(col);
    }
    if (info.sm_index == SIZE_MAX) {
        for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
            if (td.storage_managers[i].type_name.find(type_token) != std::string::npos) {
                info.sm_index = i;
                info.sequence_number = td.storage_managers[i].sequence_number;
                info.sm_type = td.storage_managers[i].type_name;
                break;
            }
        }
    }
    if (info.dm_group.empty() && !info.sm_type.empty()) {
        info.dm_group = info.sm_type;
    }
    return info;
}

void set_sm_blob(TableDatFull& td, const std::size_t sm_index, std::vector<std::uint8_t> blob) {
    if (sm_index >= td.storage_managers.size()) {
        throw std::runtime_error("storage manager index out of range");
    }
    td.storage_managers[sm_index].data_blob = std::move(blob);
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
    const auto nrow = td.row_count;

    const auto ssm = collect_sm_class(td, "StandardStMan");
    const auto ism = collect_sm_class(td, "IncrementalStMan");
    const auto tsm = collect_sm_class(td, "Tiled");

    if (ssm.columns.empty() && ism.columns.empty() && tsm.columns.empty()) {
        return t;
    }

    // Set up SSM writer if needed.
    if (!ssm.columns.empty()) {
        t.impl_->ensure_ssm_readers();

        SsmWriter writer;
        writer.setup(ssm.columns, nrow, td.big_endian,
                     ssm.dm_group.empty() ? std::string_view{"StandardStMan"}
                                          : std::string_view{ssm.dm_group});

        // Copy all cell data from readers to writer.
        for (std::size_t ci = 0; ci < ssm.columns.size(); ++ci) {
            const auto& col = ssm.columns[ci];
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
        t.impl_->writer_columns = ssm.columns;
    }

    // Set up ISM writer for pure-ISM writable tables.
    if (!ism.columns.empty() && ssm.columns.empty()) {
        t.impl_->ensure_ism_readers();

        IsmWriter writer;
        writer.setup(ism.columns, nrow, td.big_endian,
                     ism.dm_group.empty() ? std::string_view{"ISMData"}
                                          : std::string_view{ism.dm_group});

        for (std::size_t ci = 0; ci < ism.columns.size(); ++ci) {
            const auto& col = ism.columns[ci];
            auto* reader = t.impl_->find_ism_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                writer.write_cell(ci, reader->read_cell(col.name, r), r);
            }
        }

        t.impl_->ism_writer = std::move(writer);
        t.impl_->writer_columns = ism.columns;
    }

    // Set up TSM writer for pure-TSM writable tables.
    if (!tsm.columns.empty() && ssm.columns.empty() && ism.columns.empty()) {
        t.impl_->ensure_tsm_readers();

        TiledStManWriter tsm_wr;
        tsm_wr.setup(tsm.sm_type, tsm.dm_group, tsm.columns, nrow, td.big_endian);

        // Copy existing data from TSM reader into writer.
        for (std::size_t ci = 0; ci < tsm.columns.size(); ++ci) {
            const auto& col = tsm.columns[ci];
            auto* reader = t.impl_->find_tsm_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto raw = reader->read_raw_cell(col.name, r);
                tsm_wr.write_raw_cell(ci, raw, r);
            }
        }

        t.impl_->tsm_writer = std::move(tsm_wr);
        t.impl_->tsm_writer_seq = tsm.sequence_number;
        t.impl_->writer_columns = tsm.columns;
    }

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
        tsm_wr.setup(options.sm_type, options.sm_group, td.table_desc.columns, nrows,
                     td.big_endian);

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

    // Use local references from .value() after has_value() checks so clang-tidy
    // can prove optional accesses are checked.
    if (impl_->ssm_writer.has_value()) {
        auto& ssm_writer = impl_->ssm_writer.value();
        ssm_writer.write_file(impl_->path.string(), 0);
        ssm_writer.write_indirect_file(impl_->path.string(), 0);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = ssm_writer.make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset reader cache.
        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        auto& ism_writer = impl_->ism_writer.value();
        ism_writer.write_file(impl_->path.string(), 0);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = ism_writer.make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset reader cache.
        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        auto& tsm_writer = impl_->tsm_writer.value();
        tsm_writer.write_files(impl_->path.string(), impl_->tsm_writer_seq);

        // Re-write table.dat with updated blob.
        impl_->dir.table_dat.storage_managers[0].data_blob = tsm_writer.make_blob();
        write_table_directory(impl_->path.string(), impl_->dir.table_dat);

        // Reset TSM reader cache.
        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    }
}

// ---------------------------------------------------------------------------
// Table mutation methods
// ---------------------------------------------------------------------------

void Table::add_rows(std::uint64_t n) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::add_rows: table is not writable");
    }
    if (n == 0) {
        return;
    }

    auto& td = impl_->dir.table_dat;
    const auto old_nrow = td.row_count;
    const auto new_nrow = old_nrow + n;
    td.row_count = new_nrow;
    td.post_td_row_count = new_nrow;

    // Rebuild the active SM writer with the expanded row count, preserving existing data.
    if (impl_->ssm_writer.has_value()) {
        auto& old_writer = *impl_->ssm_writer;
        const auto ssm = collect_sm_class(td, "StandardStMan");
        if (ssm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_rows: unable to locate StandardStMan columns");
        }

        // Flush old writer data to disk with old_nrow so the reader can parse it.
        old_writer.write_file(impl_->path.string(), 0);
        old_writer.write_indirect_file(impl_->path.string(), 0);
        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, ssm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        // Re-read using a fresh reader against old_nrow metadata.
        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
        impl_->ensure_ssm_readers();

        // Now restore new_nrow and build the new writer.
        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        SsmWriter new_writer;
        new_writer.setup(ssm.columns, new_nrow, td.big_endian,
                         ssm.dm_group.empty() ? std::string_view{"StandardStMan"}
                                              : std::string_view{ssm.dm_group});

        for (std::size_t ci = 0; ci < ssm.columns.size(); ++ci) {
            const auto& col = ssm.columns[ci];
            auto* reader = impl_->find_ssm_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < old_nrow; ++r) {
                if (col.kind == ColumnKind::scalar) {
                    new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
                } else if (col.kind == ColumnKind::array) {
                    if (reader->is_indirect(col.name)) {
                        auto offset = reader->read_indirect_offset(col.name, r);
                        new_writer.write_indirect_offset(ci, r, offset);
                    } else {
                        auto raw = reader->read_array_raw(col.name, r);
                        new_writer.write_array_raw(ci, raw, r);
                    }
                }
            }
        }

        impl_->ssm_writer = std::move(new_writer);
        impl_->writer_columns = ssm.columns;

        // Flush new writer to disk so readers see the expanded table.
        auto& ssm_writer = *impl_->ssm_writer;
        ssm_writer.write_file(impl_->path.string(), 0);
        ssm_writer.write_indirect_file(impl_->path.string(), 0);
        set_sm_blob(td, ssm.sm_index, ssm_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        // Reset reader cache since metadata changed.
        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        auto& old_writer = *impl_->ism_writer;
        const auto ism = collect_sm_class(td, "IncrementalStMan");
        if (ism.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_rows: unable to locate IncrementalStMan columns");
        }

        old_writer.write_file(impl_->path.string(), ism.sequence_number);
        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, ism.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
        impl_->ensure_ism_readers();

        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        IsmWriter new_writer;
        new_writer.setup(ism.columns, new_nrow, td.big_endian,
                         ism.dm_group.empty() ? std::string_view{"ISMData"}
                                              : std::string_view{ism.dm_group});

        for (std::size_t ci = 0; ci < ism.columns.size(); ++ci) {
            const auto& col = ism.columns[ci];
            auto* reader = impl_->find_ism_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < old_nrow; ++r) {
                new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
            }
        }

        impl_->ism_writer = std::move(new_writer);
        impl_->writer_columns = ism.columns;

        auto& ism_writer = *impl_->ism_writer;
        ism_writer.write_file(impl_->path.string(), ism.sequence_number);
        set_sm_blob(td, ism.sm_index, ism_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        auto& old_writer = *impl_->tsm_writer;
        const auto tsm = collect_sm_class(td, "Tiled");
        if (tsm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_rows: unable to locate tiled SM columns");
        }

        old_writer.write_files(impl_->path.string(), tsm.sequence_number);
        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, tsm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
        impl_->ensure_tsm_readers();

        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        TiledStManWriter new_writer;
        new_writer.setup(tsm.sm_type, tsm.dm_group, tsm.columns, new_nrow, td.big_endian);

        for (std::size_t ci = 0; ci < tsm.columns.size(); ++ci) {
            const auto& col = tsm.columns[ci];
            auto* reader = impl_->find_tsm_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < old_nrow; ++r) {
                new_writer.write_raw_cell(ci, reader->read_raw_cell(col.name, r), r);
            }
        }

        impl_->tsm_writer = std::move(new_writer);
        impl_->tsm_writer_seq = tsm.sequence_number;
        impl_->writer_columns = tsm.columns;

        auto& tsm_writer = *impl_->tsm_writer;
        tsm_writer.write_files(impl_->path.string(), tsm.sequence_number);
        set_sm_blob(td, tsm.sm_index, tsm_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    }
}

void Table::remove_rows(std::vector<std::uint64_t> rows_to_remove) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::remove_rows: table is not writable");
    }
    if (rows_to_remove.empty()) {
        return;
    }

    auto& td = impl_->dir.table_dat;
    const auto old_nrow = td.row_count;

    // Sort and deduplicate.
    std::sort(rows_to_remove.begin(), rows_to_remove.end());
    rows_to_remove.erase(std::unique(rows_to_remove.begin(), rows_to_remove.end()),
                         rows_to_remove.end());

    // Validate indices.
    for (auto r : rows_to_remove) {
        if (r >= old_nrow) {
            throw std::runtime_error("Table::remove_rows: row index " + std::to_string(r) +
                                     " out of range (nrow=" + std::to_string(old_nrow) + ")");
        }
    }

    const auto new_nrow = old_nrow - static_cast<std::uint64_t>(rows_to_remove.size());
    td.row_count = new_nrow;
    td.post_td_row_count = new_nrow;

    if (impl_->ssm_writer.has_value()) {
        auto& old_writer = *impl_->ssm_writer;
        const auto ssm = collect_sm_class(td, "StandardStMan");
        if (ssm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_rows: unable to locate StandardStMan columns");
        }

        // Flush current state to disk so we can re-read.
        old_writer.write_file(impl_->path.string(), 0);
        old_writer.write_indirect_file(impl_->path.string(), 0);

        // Temporarily set nrow back to old for reader.
        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, ssm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
        impl_->ensure_ssm_readers();

        // Build the new writer with reduced row count.
        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        SsmWriter new_writer;
        new_writer.setup(ssm.columns, new_nrow, td.big_endian,
                         ssm.dm_group.empty() ? std::string_view{"StandardStMan"}
                                              : std::string_view{ssm.dm_group});

        // Build a set for fast removal lookup.
        std::size_t remove_idx = 0;
        std::uint64_t dst_row = 0;
        for (std::uint64_t src_row = 0; src_row < old_nrow; ++src_row) {
            if (remove_idx < rows_to_remove.size() && rows_to_remove[remove_idx] == src_row) {
                ++remove_idx;
                continue; // skip removed row
            }
            for (std::size_t ci = 0; ci < ssm.columns.size(); ++ci) {
                const auto& col = ssm.columns[ci];
                auto* reader = impl_->find_ssm_for_column(col.name);
                if (reader == nullptr)
                    continue;
                if (col.kind == ColumnKind::scalar) {
                    new_writer.write_cell(ci, reader->read_cell(col.name, src_row), dst_row);
                } else if (col.kind == ColumnKind::array) {
                    if (reader->is_indirect(col.name)) {
                        auto offset = reader->read_indirect_offset(col.name, src_row);
                        new_writer.write_indirect_offset(ci, dst_row, offset);
                    } else {
                        auto raw = reader->read_array_raw(col.name, src_row);
                        new_writer.write_array_raw(ci, raw, dst_row);
                    }
                }
            }
            ++dst_row;
        }

        impl_->ssm_writer = std::move(new_writer);
        impl_->writer_columns = ssm.columns;

        // Flush new writer to disk so readers see the compacted table.
        {
            auto& w = *impl_->ssm_writer;
            w.write_file(impl_->path.string(), 0);
            w.write_indirect_file(impl_->path.string(), 0);
            set_sm_blob(td, ssm.sm_index, w.make_blob());
            write_table_directory(impl_->path.string(), td);
        }

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        auto& old_writer = *impl_->ism_writer;
        const auto ism = collect_sm_class(td, "IncrementalStMan");
        if (ism.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_rows: unable to locate IncrementalStMan columns");
        }

        old_writer.write_file(impl_->path.string(), ism.sequence_number);

        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, ism.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
        impl_->ensure_ism_readers();

        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        IsmWriter new_writer;
        new_writer.setup(ism.columns, new_nrow, td.big_endian,
                         ism.dm_group.empty() ? std::string_view{"ISMData"}
                                              : std::string_view{ism.dm_group});

        std::size_t remove_idx = 0;
        std::uint64_t dst_row = 0;
        for (std::uint64_t src_row = 0; src_row < old_nrow; ++src_row) {
            if (remove_idx < rows_to_remove.size() && rows_to_remove[remove_idx] == src_row) {
                ++remove_idx;
                continue;
            }
            for (std::size_t ci = 0; ci < ism.columns.size(); ++ci) {
                const auto& col = ism.columns[ci];
                auto* reader = impl_->find_ism_for_column(col.name);
                if (reader == nullptr) {
                    continue;
                }
                new_writer.write_cell(ci, reader->read_cell(col.name, src_row), dst_row);
            }
            ++dst_row;
        }

        impl_->ism_writer = std::move(new_writer);
        impl_->writer_columns = ism.columns;

        auto& w = *impl_->ism_writer;
        w.write_file(impl_->path.string(), ism.sequence_number);
        set_sm_blob(td, ism.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        auto& old_writer = *impl_->tsm_writer;
        const auto tsm = collect_sm_class(td, "Tiled");
        if (tsm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_rows: unable to locate tiled SM columns");
        }

        old_writer.write_files(impl_->path.string(), tsm.sequence_number);

        td.row_count = old_nrow;
        td.post_td_row_count = old_nrow;
        set_sm_blob(td, tsm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
        impl_->ensure_tsm_readers();

        td.row_count = new_nrow;
        td.post_td_row_count = new_nrow;

        TiledStManWriter new_writer;
        new_writer.setup(tsm.sm_type, tsm.dm_group, tsm.columns, new_nrow, td.big_endian);

        std::size_t remove_idx = 0;
        std::uint64_t dst_row = 0;
        for (std::uint64_t src_row = 0; src_row < old_nrow; ++src_row) {
            if (remove_idx < rows_to_remove.size() && rows_to_remove[remove_idx] == src_row) {
                ++remove_idx;
                continue;
            }
            for (std::size_t ci = 0; ci < tsm.columns.size(); ++ci) {
                const auto& col = tsm.columns[ci];
                auto* reader = impl_->find_tsm_for_column(col.name);
                if (reader == nullptr) {
                    continue;
                }
                new_writer.write_raw_cell(ci, reader->read_raw_cell(col.name, src_row), dst_row);
            }
            ++dst_row;
        }

        impl_->tsm_writer = std::move(new_writer);
        impl_->tsm_writer_seq = tsm.sequence_number;
        impl_->writer_columns = tsm.columns;

        auto& w = *impl_->tsm_writer;
        w.write_files(impl_->path.string(), tsm.sequence_number);
        set_sm_blob(td, tsm.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    }
}

void Table::add_column(const TableColumnSpec& spec) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::add_column: table is not writable");
    }

    auto& td = impl_->dir.table_dat;

    // Check for duplicate.
    for (const auto& existing : td.table_desc.columns) {
        if (existing.name == spec.name) {
            throw std::runtime_error("Table::add_column: column '" + spec.name + "' already exists");
        }
    }

    SmClassInfo active_sm;
    if (impl_->ssm_writer.has_value()) {
        active_sm = collect_sm_class(td, "StandardStMan");
    } else if (impl_->ism_writer.has_value()) {
        active_sm = collect_sm_class(td, "IncrementalStMan");
    } else if (impl_->tsm_writer.has_value()) {
        active_sm = collect_sm_class(td, "Tiled");
    } else {
        throw std::runtime_error("Table::add_column: no active writer");
    }
    if (active_sm.sm_index == SIZE_MAX) {
        throw std::runtime_error("Table::add_column: unable to resolve storage manager binding");
    }
    if (impl_->ism_writer.has_value() && spec.kind != ColumnKind::scalar) {
        throw std::runtime_error("Table::add_column: IncrementalStMan only supports scalar columns");
    }
    if (impl_->tsm_writer.has_value() &&
        (spec.kind != ColumnKind::array || spec.shape.empty())) {
        throw std::runtime_error(
            "Table::add_column: tiled storage managers require fixed-shape array columns");
    }

    // Build ColumnDesc.
    ColumnDesc cd;
    cd.kind = spec.kind;
    cd.name = spec.name;
    cd.data_type = spec.data_type;
    cd.comment = spec.comment;
    cd.type_string = make_type_string(spec.data_type, spec.kind);
    cd.dm_type = active_sm.sm_type;
    cd.dm_group = active_sm.dm_group;
    if (spec.kind == ColumnKind::array && !spec.shape.empty()) {
        cd.ndim = static_cast<std::int32_t>(spec.shape.size());
        cd.shape = spec.shape;
        cd.options = 5; // Direct|FixedShape
    } else if (spec.kind == ColumnKind::array) {
        cd.ndim = -1; // variable shape
    }
    cd.version = 1;
    td.table_desc.columns.push_back(cd);

    // Add ColumnManagerSetup.
    ColumnManagerSetup cms;
    cms.column_name = spec.name;
    cms.sequence_number = active_sm.sequence_number;
    if (spec.kind == ColumnKind::array && !spec.shape.empty()) {
        cms.has_shape = true;
        cms.shape = spec.shape;
    }
    td.column_setups.push_back(std::move(cms));

    // Rebuild the active writer with the new column set.
    if (impl_->ssm_writer.has_value()) {
        auto& old_writer = *impl_->ssm_writer;
        const auto nrow = td.row_count;
        const auto ssm = collect_sm_class(td, "StandardStMan");
        if (ssm.columns.empty() || ssm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_column: unable to locate StandardStMan columns");
        }

        // Flush old writer data to disk with old metadata (without new column)
        // so the reader can parse it.
        old_writer.write_file(impl_->path.string(), 0);
        old_writer.write_indirect_file(impl_->path.string(), 0);

        // Temporarily remove the new column from td for the reader.
        auto saved_cd = td.table_desc.columns.back();
        td.table_desc.columns.pop_back();
        auto saved_cms = td.column_setups.back();
        td.column_setups.pop_back();
        set_sm_blob(td, ssm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
        impl_->ensure_ssm_readers();

        // Restore the new column.
        td.table_desc.columns.push_back(std::move(saved_cd));
        td.column_setups.push_back(std::move(saved_cms));

        const auto ssm_after = collect_sm_class(td, "StandardStMan");

        SsmWriter new_writer;
        new_writer.setup(ssm_after.columns, nrow, td.big_endian,
                         ssm_after.dm_group.empty() ? std::string_view{"StandardStMan"}
                                                    : std::string_view{ssm_after.dm_group});

        // Copy existing column data (skip the new column).
        for (std::size_t ci = 0; ci < ssm_after.columns.size(); ++ci) {
            const auto& col = ssm_after.columns[ci];
            if (col.name == spec.name) {
                continue; // new column, no data to copy
            }
            auto* reader = impl_->find_ssm_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (col.kind == ColumnKind::scalar) {
                    new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
                } else if (col.kind == ColumnKind::array) {
                    if (reader->is_indirect(col.name)) {
                        auto offset = reader->read_indirect_offset(col.name, r);
                        new_writer.write_indirect_offset(ci, r, offset);
                    } else {
                        auto raw = reader->read_array_raw(col.name, r);
                        new_writer.write_array_raw(ci, raw, r);
                    }
                }
            }
        }

        impl_->ssm_writer = std::move(new_writer);
        impl_->writer_columns = ssm_after.columns;

        // Flush new writer to disk so readers see the updated table.
        {
            auto& w = *impl_->ssm_writer;
            w.write_file(impl_->path.string(), 0);
            w.write_indirect_file(impl_->path.string(), 0);
            set_sm_blob(td, ssm_after.sm_index, w.make_blob());
            write_table_directory(impl_->path.string(), td);
        }

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        auto& old_writer = *impl_->ism_writer;
        const auto nrow = td.row_count;
        const auto ism = collect_sm_class(td, "IncrementalStMan");
        if (ism.columns.empty() || ism.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_column: unable to locate IncrementalStMan columns");
        }

        old_writer.write_file(impl_->path.string(), ism.sequence_number);

        auto saved_cd = td.table_desc.columns.back();
        td.table_desc.columns.pop_back();
        auto saved_cms = td.column_setups.back();
        td.column_setups.pop_back();
        set_sm_blob(td, ism.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
        impl_->ensure_ism_readers();

        td.table_desc.columns.push_back(std::move(saved_cd));
        td.column_setups.push_back(std::move(saved_cms));

        const auto ism_after = collect_sm_class(td, "IncrementalStMan");

        IsmWriter new_writer;
        new_writer.setup(ism_after.columns, nrow, td.big_endian,
                         ism_after.dm_group.empty() ? std::string_view{"ISMData"}
                                                    : std::string_view{ism_after.dm_group});

        for (std::size_t ci = 0; ci < ism_after.columns.size(); ++ci) {
            const auto& col = ism_after.columns[ci];
            if (col.name == spec.name) {
                continue;
            }
            auto* reader = impl_->find_ism_for_column(col.name);
            if (reader == nullptr) {
                continue;
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
            }
        }

        impl_->ism_writer = std::move(new_writer);
        impl_->writer_columns = ism_after.columns;

        auto& w = *impl_->ism_writer;
        w.write_file(impl_->path.string(), ism_after.sequence_number);
        set_sm_blob(td, ism_after.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        auto& old_writer = *impl_->tsm_writer;
        const auto nrow = td.row_count;
        const auto tsm = collect_sm_class(td, "Tiled");
        if (tsm.columns.empty() || tsm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::add_column: unable to locate tiled SM columns");
        }

        old_writer.write_files(impl_->path.string(), tsm.sequence_number);

        auto saved_cd = td.table_desc.columns.back();
        td.table_desc.columns.pop_back();
        auto saved_cms = td.column_setups.back();
        td.column_setups.pop_back();
        set_sm_blob(td, tsm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
        impl_->ensure_tsm_readers();

        td.table_desc.columns.push_back(std::move(saved_cd));
        td.column_setups.push_back(std::move(saved_cms));

        const auto tsm_after = collect_sm_class(td, "Tiled");

        TiledStManWriter new_writer;
        new_writer.setup(tsm_after.sm_type, tsm_after.dm_group, tsm_after.columns, nrow,
                         td.big_endian);

        for (std::size_t ci = 0; ci < tsm_after.columns.size(); ++ci) {
            const auto& col = tsm_after.columns[ci];
            if (col.name == spec.name) {
                continue;
            }
            auto* reader = impl_->find_tsm_for_column(col.name);
            if (reader == nullptr) {
                continue;
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                new_writer.write_raw_cell(ci, reader->read_raw_cell(col.name, r), r);
            }
        }

        impl_->tsm_writer = std::move(new_writer);
        impl_->tsm_writer_seq = tsm_after.sequence_number;
        impl_->writer_columns = tsm_after.columns;

        auto& w = *impl_->tsm_writer;
        w.write_files(impl_->path.string(), tsm_after.sequence_number);
        set_sm_blob(td, tsm_after.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    }
}

void Table::remove_column(std::string_view name) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::remove_column: table is not writable");
    }

    auto& td = impl_->dir.table_dat;

    // Verify the column exists before removing.
    auto& cols = td.table_desc.columns;
    auto col_it =
        std::find_if(cols.begin(), cols.end(), [&](const ColumnDesc& c) { return c.name == name; });
    if (col_it == cols.end()) {
        throw std::runtime_error("Table::remove_column: column '" + std::string(name) +
                                 "' not found");
    }

    if (impl_->ssm_writer.has_value()) {
        auto& old_writer = *impl_->ssm_writer;
        const auto nrow = td.row_count;
        const auto ssm = collect_sm_class(td, "StandardStMan");
        if (ssm.columns.empty() || ssm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_column: unable to locate StandardStMan columns");
        }

        old_writer.write_file(impl_->path.string(), 0);
        old_writer.write_indirect_file(impl_->path.string(), 0);
        set_sm_blob(td, ssm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
        impl_->ensure_ssm_readers();

        cols.erase(std::find_if(cols.begin(), cols.end(),
                                [&](const ColumnDesc& c) { return c.name == name; }));
        std::erase_if(td.column_setups,
                      [&](const ColumnManagerSetup& s) { return s.column_name == name; });
        std::erase_if(impl_->writer_columns,
                      [&](const ColumnDesc& c) { return c.name == name; });

        const auto ssm_after = collect_sm_class(td, "StandardStMan");

        SsmWriter new_writer;
        new_writer.setup(ssm_after.columns, nrow, td.big_endian,
                         ssm_after.dm_group.empty() ? std::string_view{"StandardStMan"}
                                                    : std::string_view{ssm_after.dm_group});

        for (std::size_t ci = 0; ci < ssm_after.columns.size(); ++ci) {
            const auto& col = ssm_after.columns[ci];
            auto* reader = impl_->find_ssm_for_column(col.name);
            if (reader == nullptr)
                continue;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (col.kind == ColumnKind::scalar) {
                    new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
                } else if (col.kind == ColumnKind::array) {
                    if (reader->is_indirect(col.name)) {
                        auto offset = reader->read_indirect_offset(col.name, r);
                        new_writer.write_indirect_offset(ci, r, offset);
                    } else {
                        auto raw = reader->read_array_raw(col.name, r);
                        new_writer.write_array_raw(ci, raw, r);
                    }
                }
            }
        }

        impl_->ssm_writer = std::move(new_writer);
        impl_->writer_columns = ssm_after.columns;

        auto& w = *impl_->ssm_writer;
        w.write_file(impl_->path.string(), 0);
        w.write_indirect_file(impl_->path.string(), 0);
        set_sm_blob(td, ssm_after.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ssm_readers.clear();
        impl_->ssm_readers_initialized = false;
    } else if (impl_->ism_writer.has_value()) {
        auto& old_writer = *impl_->ism_writer;
        const auto nrow = td.row_count;
        const auto ism = collect_sm_class(td, "IncrementalStMan");
        if (ism.columns.empty() || ism.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_column: unable to locate IncrementalStMan columns");
        }

        old_writer.write_file(impl_->path.string(), ism.sequence_number);
        set_sm_blob(td, ism.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
        impl_->ensure_ism_readers();

        cols.erase(std::find_if(cols.begin(), cols.end(),
                                [&](const ColumnDesc& c) { return c.name == name; }));
        std::erase_if(td.column_setups,
                      [&](const ColumnManagerSetup& s) { return s.column_name == name; });
        std::erase_if(impl_->writer_columns,
                      [&](const ColumnDesc& c) { return c.name == name; });

        const auto ism_after = collect_sm_class(td, "IncrementalStMan");

        IsmWriter new_writer;
        new_writer.setup(ism_after.columns, nrow, td.big_endian,
                         ism_after.dm_group.empty() ? std::string_view{"ISMData"}
                                                    : std::string_view{ism_after.dm_group});

        for (std::size_t ci = 0; ci < ism_after.columns.size(); ++ci) {
            const auto& col = ism_after.columns[ci];
            auto* reader = impl_->find_ism_for_column(col.name);
            if (reader == nullptr) {
                continue;
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                new_writer.write_cell(ci, reader->read_cell(col.name, r), r);
            }
        }

        impl_->ism_writer = std::move(new_writer);
        impl_->writer_columns = ism_after.columns;

        auto& w = *impl_->ism_writer;
        w.write_file(impl_->path.string(), ism_after.sequence_number);
        set_sm_blob(td, ism_after.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->ism_readers.clear();
        impl_->ism_readers_initialized = false;
    } else if (impl_->tsm_writer.has_value()) {
        auto& old_writer = *impl_->tsm_writer;
        const auto nrow = td.row_count;
        const auto tsm = collect_sm_class(td, "Tiled");
        if (tsm.columns.empty() || tsm.sm_index == SIZE_MAX) {
            throw std::runtime_error("Table::remove_column: unable to locate tiled SM columns");
        }

        old_writer.write_files(impl_->path.string(), tsm.sequence_number);
        set_sm_blob(td, tsm.sm_index, old_writer.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
        impl_->ensure_tsm_readers();

        cols.erase(std::find_if(cols.begin(), cols.end(),
                                [&](const ColumnDesc& c) { return c.name == name; }));
        std::erase_if(td.column_setups,
                      [&](const ColumnManagerSetup& s) { return s.column_name == name; });
        std::erase_if(impl_->writer_columns,
                      [&](const ColumnDesc& c) { return c.name == name; });

        const auto tsm_after = collect_sm_class(td, "Tiled");

        TiledStManWriter new_writer;
        new_writer.setup(tsm_after.sm_type, tsm_after.dm_group, tsm_after.columns, nrow,
                         td.big_endian);

        for (std::size_t ci = 0; ci < tsm_after.columns.size(); ++ci) {
            const auto& col = tsm_after.columns[ci];
            auto* reader = impl_->find_tsm_for_column(col.name);
            if (reader == nullptr) {
                continue;
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                new_writer.write_raw_cell(ci, reader->read_raw_cell(col.name, r), r);
            }
        }

        impl_->tsm_writer = std::move(new_writer);
        impl_->tsm_writer_seq = tsm_after.sequence_number;
        impl_->writer_columns = tsm_after.columns;

        auto& w = *impl_->tsm_writer;
        w.write_files(impl_->path.string(), tsm_after.sequence_number);
        set_sm_blob(td, tsm_after.sm_index, w.make_blob());
        write_table_directory(impl_->path.string(), td);

        impl_->tsm_readers.clear();
        impl_->tsm_readers_initialized = false;
    } else {
        cols.erase(col_it);
        std::erase_if(td.column_setups,
                      [&](const ColumnManagerSetup& s) { return s.column_name == name; });
        std::erase_if(impl_->writer_columns,
                      [&](const ColumnDesc& c) { return c.name == name; });
    }
}

void Table::rename_column(std::string_view old_name, std::string_view new_name) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::rename_column: table is not writable");
    }

    auto& td = impl_->dir.table_dat;

    // Verify old_name exists.
    auto& cols = td.table_desc.columns;
    auto col_it = std::find_if(cols.begin(), cols.end(),
                               [&](const ColumnDesc& c) { return c.name == old_name; });
    if (col_it == cols.end()) {
        throw std::runtime_error("Table::rename_column: column '" + std::string(old_name) +
                                 "' not found");
    }

    // Verify new_name does not exist.
    auto dup_it = std::find_if(cols.begin(), cols.end(),
                               [&](const ColumnDesc& c) { return c.name == new_name; });
    if (dup_it != cols.end()) {
        throw std::runtime_error("Table::rename_column: column '" + std::string(new_name) +
                                 "' already exists");
    }

    // Rename in table_desc.columns.
    col_it->name = std::string(new_name);

    // Rename in column_setups.
    for (auto& setup : td.column_setups) {
        if (setup.column_name == old_name) {
            setup.column_name = std::string(new_name);
        }
    }

    // Rename in writer_columns.
    for (auto& wc : impl_->writer_columns) {
        if (wc.name == old_name) {
            wc.name = std::string(new_name);
        }
    }
}

void Table::set_keyword(std::string_view key, RecordValue value) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::set_keyword: table is not writable");
    }
    impl_->dir.table_dat.table_desc.keywords.set(key, std::move(value));
}

void Table::remove_keyword(std::string_view key) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::remove_keyword: table is not writable");
    }
    if (!impl_->dir.table_dat.table_desc.keywords.remove(key)) {
        throw std::runtime_error("Table::remove_keyword: keyword '" + std::string(key) +
                                 "' not found");
    }
}

void Table::copy_column(std::string_view src, std::string_view dst) {
    if (!impl_->writable) {
        throw std::runtime_error("Table::copy_column: table is not writable");
    }

    const auto* src_cd = find_column_desc(src);
    if (src_cd == nullptr) {
        throw std::runtime_error("Table::copy_column: source column '" + std::string(src) +
                                 "' not found");
    }

    // Check dst does not exist.
    if (find_column_desc(dst) != nullptr) {
        throw std::runtime_error("Table::copy_column: destination column '" + std::string(dst) +
                                 "' already exists");
    }

    // Build a spec from the source column.
    TableColumnSpec spec;
    spec.name = std::string(dst);
    spec.data_type = src_cd->data_type;
    spec.kind = src_cd->kind;
    spec.shape.assign(src_cd->shape.begin(), src_cd->shape.end());
    spec.comment = src_cd->comment;

    // Add the new column (this rebuilds the writer).
    add_column(spec);

    // Now copy the data cell by cell.
    const auto nrow = this->nrow();
    if (nrow == 0) {
        return;
    }

    if (spec.kind == ColumnKind::scalar) {
        for (std::uint64_t r = 0; r < nrow; ++r) {
            auto val = read_scalar_cell(src, r);
            write_scalar_cell(dst, r, val);
        }
    } else if (spec.kind == ColumnKind::array) {
        // Copy array data. We need to use the appropriate typed read/write
        // based on data type.
        switch (spec.data_type) {
        case DataType::tp_float:
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto vals = read_array_float_cell(src, r);
                write_array_float_cell(dst, r, vals);
            }
            break;
        case DataType::tp_double:
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto vals = read_array_double_cell(src, r);
                write_array_double_cell(dst, r, vals);
            }
            break;
        default:
            // For other array types, use raw bytes.
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto raw = read_array_raw_cell(src, r);
                // Write via the SSM writer directly.
                if (impl_->ssm_writer.has_value()) {
                    auto& ssm_writer = impl_->ssm_writer.value();
                    for (std::size_t ci = 0; ci < impl_->writer_columns.size(); ++ci) {
                        if (impl_->writer_columns[ci].name == dst) {
                            ssm_writer.write_array_raw(ci, raw, r);
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
}

void Table::drop(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Table::drop: path '" + path.string() + "' does not exist");
    }
    std::filesystem::remove_all(path);
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
    // Check TSM writer first for read-after-write consistency.
    if (impl_->writable && impl_->tsm_writer.has_value()) {
        const auto& tsm_writer = impl_->tsm_writer.value();
        auto ci = tsm_writer.find_column(col_name);
        if (ci != SIZE_MAX) {
            return tsm_writer.read_double_cell(ci, row);
        }
    }
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
    // Check TSM writer first for read-after-write consistency.
    if (impl_->writable && impl_->tsm_writer.has_value()) {
        const auto& tsm_writer = impl_->tsm_writer.value();
        auto ci = tsm_writer.find_column(col_name);
        if (ci != SIZE_MAX) {
            return tsm_writer.read_float_cell(ci, row);
        }
    }
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
    // Check TSM writer first for read-after-write consistency.
    if (impl_->writable && impl_->tsm_writer.has_value()) {
        const auto& tsm_writer = impl_->tsm_writer.value();
        auto ci = tsm_writer.find_column(col_name);
        if (ci != SIZE_MAX) {
            auto raw = tsm_writer.read_raw_cell(ci, row);
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
    }
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
                auto& ssm_writer = impl_->ssm_writer.value();
                ssm_writer.write_cell(i, value, row);
            } else if (impl_->ism_writer.has_value()) {
                auto& ism_writer = impl_->ism_writer.value();
                ism_writer.write_cell(i, value, row);
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
                auto& ssm_writer = impl_->ssm_writer.value();
                ssm_writer.write_array_float(i, values, row);
            } else if (impl_->tsm_writer.has_value()) {
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_float_cell(i, values, row);
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
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->ssm_writer.has_value()) {
                auto& ssm_writer = impl_->ssm_writer.value();
                ssm_writer.write_array_double(i, values, row);
            } else if (impl_->tsm_writer.has_value()) {
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_double_cell(i, values, row);
            } else {
                throw std::runtime_error("Table: no double array writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_array_double_cell: column not found");
}

void Table::write_array_bool_cell(std::string_view col_name, std::uint64_t row,
                                  const std::vector<bool>& values,
                                  const std::vector<std::int32_t>& shape) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->tsm_writer.has_value()) {
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_bool_cell(i, values, row);
            } else if (impl_->ssm_writer.has_value()) {
                auto& ssm_writer = impl_->ssm_writer.value();
                // Bool arrays stored as 1 byte per element in indirect file.
                std::vector<std::uint8_t> raw(values.size());
                for (std::size_t j = 0; j < values.size(); ++j) {
                    raw[j] = values[j] ? 1 : 0;
                }
                ssm_writer.write_indirect_array(i, shape, raw, row);
            } else {
                throw std::runtime_error("Table: no bool array writer available");
            }
            return;
        }
    }
    throw std::runtime_error("Table::write_array_bool_cell: column not found");
}

void Table::write_array_complex_cell(std::string_view col_name, std::uint64_t row,
                                     const std::vector<std::complex<float>>& values,
                                     const std::vector<std::int32_t>& shape) {
    if (!impl_->writable) {
        throw std::runtime_error("Table: not writable");
    }
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            if (impl_->tsm_writer.has_value()) {
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_complex_cell(i, values, row);
            } else if (impl_->ssm_writer.has_value()) {
                auto& ssm_writer = impl_->ssm_writer.value();
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
                ssm_writer.write_indirect_array(i, shape, raw, row);
            } else {
                throw std::runtime_error("Table: no complex array writer available");
            }
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
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_int_cell(i, values, row);
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
                auto& tsm_writer = impl_->tsm_writer.value();
                tsm_writer.write_raw_cell(i, data, row);
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
    auto& ssm_writer = impl_->ssm_writer.value();
    for (std::size_t i = 0; i < impl_->writer_columns.size(); ++i) {
        if (impl_->writer_columns[i].name == col_name) {
            ssm_writer.write_indirect_array(i, shape, data, row);
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
