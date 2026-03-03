// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/image.hpp"

#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_desc_writer.hpp"

#include <complex>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace casacore_mini {

// ── ImageInfo serialization ──────────────────────────────────────────

Record ImageInfo::to_record() const {
    Record rec;
    rec.set("bmaj", RecordValue(beam_major_rad));
    rec.set("bmin", RecordValue(beam_minor_rad));
    rec.set("bpa", RecordValue(beam_pa_rad));
    rec.set("objectname", RecordValue(object_name));
    rec.set("imagetype", RecordValue(image_type));
    return rec;
}

ImageInfo ImageInfo::from_record(const Record& rec) {
    ImageInfo info;
    if (auto* v = rec.find("bmaj"))
        if (auto* d = std::get_if<double>(&v->storage()))
            info.beam_major_rad = *d;
    if (auto* v = rec.find("bmin"))
        if (auto* d = std::get_if<double>(&v->storage()))
            info.beam_minor_rad = *d;
    if (auto* v = rec.find("bpa"))
        if (auto* d = std::get_if<double>(&v->storage()))
            info.beam_pa_rad = *d;
    if (auto* v = rec.find("objectname"))
        if (auto* s = std::get_if<std::string>(&v->storage()))
            info.object_name = *s;
    if (auto* v = rec.find("imagetype"))
        if (auto* s = std::get_if<std::string>(&v->storage()))
            info.image_type = *s;
    return info;
}

// ── ImageBeamSet ─────────────────────────────────────────────────────

ImageBeamSet::ImageBeamSet(Beam single) : beams(1, std::vector<Beam>(1, single)) {}

ImageBeamSet::ImageBeamSet(std::size_t nchan, std::size_t nstokes)
    : beams(nchan, std::vector<Beam>(nstokes)) {}

std::size_t ImageBeamSet::nchan() const {
    return beams.size();
}

std::size_t ImageBeamSet::nstokes() const {
    if (beams.empty()) return 0;
    return beams[0].size();
}

bool ImageBeamSet::is_single() const {
    return beams.size() == 1 && !beams[0].empty() && beams[0].size() == 1;
}

const ImageBeamSet::Beam& ImageBeamSet::get(std::size_t chan,
                                             std::size_t stokes) const {
    return beams.at(chan).at(stokes);
}

void ImageBeamSet::set(std::size_t chan, std::size_t stokes, Beam beam) {
    beams.at(chan).at(stokes) = beam;
}

Record ImageBeamSet::to_record() const {
    Record rec;
    rec.set("nchan", RecordValue(static_cast<double>(nchan())));
    rec.set("nstokes", RecordValue(static_cast<double>(nstokes())));
    // Flatten beams as parallel arrays.
    for (std::size_t c = 0; c < nchan(); ++c) {
        for (std::size_t s = 0; s < nstokes(); ++s) {
            auto key = "beam_" + std::to_string(c) + "_" + std::to_string(s);
            Record brec;
            brec.set("major", RecordValue(beams[c][s].major));
            brec.set("minor", RecordValue(beams[c][s].minor));
            brec.set("pa", RecordValue(beams[c][s].pa));
            rec.set(key, RecordValue::from_record(brec));
        }
    }
    return rec;
}

ImageBeamSet ImageBeamSet::from_record(const Record& rec) {
    std::size_t nc = 0;
    std::size_t ns = 0;
    if (auto* v = rec.find("nchan"))
        if (auto* d = std::get_if<double>(&v->storage()))
            nc = static_cast<std::size_t>(*d);
    if (auto* v = rec.find("nstokes"))
        if (auto* d = std::get_if<double>(&v->storage()))
            ns = static_cast<std::size_t>(*d);
    ImageBeamSet bs(nc, ns);
    for (std::size_t c = 0; c < nc; ++c) {
        for (std::size_t s = 0; s < ns; ++s) {
            auto key = "beam_" + std::to_string(c) + "_" + std::to_string(s);
            if (auto* v = rec.find(key)) {
                if (auto* rp = std::get_if<RecordValue::record_ptr>(&v->storage())) {
                    const auto& brec = **rp;
                    Beam b;
                    if (auto* d = brec.find("major"))
                        if (auto* dv = std::get_if<double>(&d->storage()))
                            b.major = *dv;
                    if (auto* d = brec.find("minor"))
                        if (auto* dv = std::get_if<double>(&d->storage()))
                            b.minor = *dv;
                    if (auto* d = brec.find("pa"))
                        if (auto* dv = std::get_if<double>(&d->storage()))
                            b.pa = *dv;
                    bs.beams[c][s] = b;
                }
            }
        }
    }
    return bs;
}

// ── Logtable creation ─────────────────────────────────────────────────

/// Create a minimal empty logtable subtable at <image_dir>/logtable.
/// The logtable is required by casacore's PagedImage for logging; an empty
/// one with the correct schema is sufficient for interoperability.
void create_logtable(const std::filesystem::path& image_dir) {
    const auto dir = image_dir / "logtable";
    std::filesystem::create_directories(dir);

    // Build column descriptors matching casacore's LoggerHolder schema.
    auto make_scalar = [](const std::string& name, DataType dt,
                          const std::string& comment = "") {
        ColumnDesc col;
        col.kind = ColumnKind::scalar;
        col.name = name;
        col.data_type = dt;
        col.dm_type = "StandardStMan";
        col.dm_group = "StandardStMan";
        const char* type_id = (dt == DataType::tp_double) ? "double  " : "String  ";
        col.type_string = std::string("ScalarColumnDesc<") + type_id;
        col.version = 1;
        col.comment = comment;
        return col;
    };

    std::vector<ColumnDesc> columns;

    // TIME column with measure keywords (UNIT/MEASURE_TYPE/MEASURE_REFERENCE).
    auto time_col = make_scalar("TIME", DataType::tp_double, "MJD in seconds");
    time_col.keywords.set("UNIT", RecordValue(std::string("s")));
    time_col.keywords.set("MEASURE_TYPE", RecordValue(std::string("EPOCH")));
    time_col.keywords.set("MEASURE_REFERENCE", RecordValue(std::string("UTC")));
    columns.push_back(std::move(time_col));

    columns.push_back(make_scalar("PRIORITY", DataType::tp_string));
    columns.push_back(make_scalar("MESSAGE", DataType::tp_string));
    columns.push_back(make_scalar("LOCATION", DataType::tp_string));
    columns.push_back(make_scalar("OBJECT_ID", DataType::tp_string));

    // Build TableDatFull.
    TableDatFull full;
    full.table_version = 2;
    full.row_count = 0;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "Log message table";
    full.table_desc.columns = columns;

    StorageManagerSetup sm;
    sm.type_name = "StandardStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : columns) {
        ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        full.column_setups.push_back(std::move(cms));
    }

    full.post_td_row_count = 0;

    // Generate SSM blob and attach to the storage manager entry.
    SsmWriter ssm;
    ssm.setup(columns, 0, false, "StandardStMan");
    full.storage_managers[0].data_blob = ssm.make_blob();

    // Write table.dat.
    auto bytes = serialize_table_dat_full(full);
    {
        std::ofstream out(dir / "table.dat", std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    // Write empty SSM data file (.f0).
    ssm.write_file(dir.string(), 0);

    // Write table.info.
    {
        std::ofstream out(dir / "table.info");
        out << "Type = Log message\n"
            << "SubType = \n"
            << "\n"
            << "Repository for software-generated logging messages\n";
    }

    // Write empty table.lock.
    { std::ofstream(dir / "table.lock"); }
}

// ── PagedImage<T> ────────────────────────────────────────────────────

// Helper: map C++ type to DataType enum for table column creation.
template <typename T> static DataType image_data_type();
template <> DataType image_data_type<float>() { return DataType::tp_float; }
template <> DataType image_data_type<double>() { return DataType::tp_double; }
template <> DataType image_data_type<std::complex<float>>() { return DataType::tp_complex; }

// PagedImage::Impl holds the table and cached metadata.
template <typename T>
struct PagedImage<T>::Impl {
    std::shared_ptr<Table> table;
    std::filesystem::path path;
    IPosition shape;
    CoordinateSystem cs;
    std::string units;
    ImageInfo info;
    Record misc;
    bool writable = false;
    bool has_mask = false;
    std::string default_mask_name;
};

template <typename T>
PagedImage<T>::PagedImage(IPosition shape, CoordinateSystem cs,
                          const std::filesystem::path& path)
    : impl_(std::make_shared<Impl>()) {
    impl_->shape = std::move(shape);
    impl_->cs = std::move(cs);
    impl_->path = path;
    impl_->writable = true;

    // Create table with "map" array column.
    TableColumnSpec col;
    col.name = "map";
    col.data_type = image_data_type<T>();
    col.kind = ColumnKind::array;
    for (std::size_t i = 0; i < impl_->shape.ndim(); ++i) {
        col.shape.push_back(impl_->shape[i]);
    }

    TableCreateOptions opts;
    opts.big_endian = false; // LE to match native byte order (no byte-swapping in TSM writer)
    opts.sm_type = "TiledShapeStMan";
    opts.sm_group = "map";
    for (std::size_t i = 0; i < impl_->shape.ndim(); ++i) {
        opts.tile_shape.push_back(
            std::min(impl_->shape[i], std::int64_t{32}));
    }
    opts.tile_shape.push_back(1); // row dimension

    impl_->table = std::make_shared<Table>(
        Table::create(path, {col}, 1, opts));

    // Create the logtable subtable (required by casacore's PagedImage).
    create_logtable(path);

    // Initialize pixels to zero.
    auto zeros = LatticeArray<T>(IPosition(impl_->shape));
    put(zeros);

    // Save initial metadata as table keywords.
    save_metadata();
}

template <typename T>
PagedImage<T>::PagedImage(const std::filesystem::path& path, bool writable)
    : impl_(std::make_shared<Impl>()) {
    impl_->path = path;
    impl_->writable = writable;
    impl_->table = writable
                       ? std::make_shared<Table>(Table::open_rw(path))
                       : std::make_shared<Table>(Table::open(path));

    // Read shape from column descriptor, falling back to per-cell shape
    // for variable-shape columns (e.g. upstream casacore's TiledShapeStMan).
    auto* cd = impl_->table->find_column_desc("map");
    if (!cd)
        throw std::runtime_error("PagedImage: no 'map' column in table");
    if (!cd->shape.empty()) {
        impl_->shape = IPosition(cd->shape);
    } else if (impl_->table->nrow() > 0) {
        impl_->shape = IPosition(impl_->table->cell_shape("map", 0));
    } else {
        throw std::runtime_error("PagedImage: cannot determine shape (no rows and no fixed shape)");
    }

    // Load metadata from table keywords.
    load_metadata();
}

template <typename T>
const IPosition& PagedImage<T>::shape() const {
    return impl_->shape;
}

template <typename T>
bool PagedImage<T>::is_writable() const {
    return impl_->writable;
}

template <typename T>
bool PagedImage<T>::has_pixel_mask() const {
    return impl_->has_mask;
}

template <typename T>
LatticeArray<bool> PagedImage<T>::get_mask() const {
    if (!impl_->has_mask) {
        return LatticeArray<bool>(impl_->shape, true);
    }
    // Read the mask from the mask subtable.
    auto mask_path = impl_->path / impl_->default_mask_name;
    Table mask_table = Table::open(mask_path);
    auto bool_vec = mask_table.read_array_bool_cell("PagedArray", 0);
    // Convert to LatticeArray<bool>.
    std::vector<bool> storage(bool_vec.begin(), bool_vec.end());
    return LatticeArray<bool>(impl_->shape, std::move(storage));
}

template <typename T>
LatticeArray<bool> PagedImage<T>::get_mask_slice(const Slicer& slicer) const {
    if (!impl_->has_mask) {
        return LatticeArray<bool>(IPosition(slicer.length()), true);
    }
    // Full mask read + slice for simplicity.
    return get_mask().get_slice(slicer);
}

template <typename T>
const CoordinateSystem& PagedImage<T>::coordinates() const {
    return impl_->cs;
}

template <typename T>
void PagedImage<T>::set_coordinates(CoordinateSystem cs) {
    impl_->cs = std::move(cs);
}

template <typename T>
const std::string& PagedImage<T>::units() const {
    return impl_->units;
}

template <typename T>
void PagedImage<T>::set_units(std::string unit_str) {
    impl_->units = std::move(unit_str);
}

template <typename T>
const ImageInfo& PagedImage<T>::image_info() const {
    return impl_->info;
}

template <typename T>
void PagedImage<T>::set_image_info(ImageInfo info) {
    impl_->info = std::move(info);
}

template <typename T>
const Record& PagedImage<T>::misc_info() const {
    return impl_->misc;
}

template <typename T>
void PagedImage<T>::set_misc_info(Record info) {
    impl_->misc = std::move(info);
}

template <typename T>
std::string PagedImage<T>::name() const {
    return impl_->path.string();
}

template <typename T>
void PagedImage<T>::make_mask(const std::string& mask_name,
                               const LatticeArray<bool>& mask_data,
                               bool set_default) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");

    // Create the mask subtable (same structure as a PagedArray<Bool>).
    const auto mask_dir = impl_->path / mask_name;

    TableColumnSpec col;
    col.name = "PagedArray";
    col.data_type = DataType::tp_bool;
    col.kind = ColumnKind::array;
    for (std::size_t i = 0; i < impl_->shape.ndim(); ++i) {
        col.shape.push_back(impl_->shape[i]);
    }

    TableCreateOptions opts;
    opts.big_endian = false;
    opts.sm_type = "TiledShapeStMan";
    opts.sm_group = "PagedArray";
    for (std::size_t i = 0; i < impl_->shape.ndim(); ++i) {
        opts.tile_shape.push_back(
            std::min(impl_->shape[i], std::int64_t{32}));
    }
    opts.tile_shape.push_back(1);

    auto mask_table = Table::create(mask_dir, {col}, 1, opts);

    // Write Bool data: convert bool array to uint8 vector.
    auto nel = impl_->shape.product();
    std::vector<std::uint8_t> bool_data(static_cast<std::size_t>(nel));
    for (std::size_t i = 0; i < static_cast<std::size_t>(nel); ++i) {
        bool_data[i] = mask_data[i] ? 1 : 0;
    }
    std::vector<std::int32_t> shape_i32;
    for (std::size_t i = 0; i < impl_->shape.ndim(); ++i) {
        shape_i32.push_back(static_cast<std::int32_t>(impl_->shape[i]));
    }
    std::vector<bool> bool_vals(bool_data.begin(), bool_data.end());
    mask_table.write_array_bool_cell("PagedArray", 0, bool_vals, shape_i32);

    // Write table.info for the mask subtable.
    {
        std::ofstream out(mask_dir / "table.info");
        out << "Type = Paged Array\nSubType = \n";
    }

    // Add mask keywords to parent table.
    auto& kw = impl_->table->rw_keywords();

    // Build mask record: masks.mask0 = { isRegion, name, comment, mask, box }
    Record mask_rec;
    mask_rec.set("isRegion", RecordValue(static_cast<std::int32_t>(1)));
    mask_rec.set("name", RecordValue(std::string("LCPagedMask")));
    mask_rec.set("comment", RecordValue(std::string()));

    // The "mask" field is a table reference to the mask subtable.
    mask_rec.set("mask", RecordValue(std::string("Table: ././" + mask_name)));

    // Box record describing the full bounding box.
    Record box;
    box.set("isRegion", RecordValue(static_cast<std::int32_t>(1)));
    box.set("name", RecordValue(std::string("LCBox")));
    box.set("comment", RecordValue(std::string()));
    box.set("oneRel", RecordValue(true));

    auto ndim = impl_->shape.ndim();
    std::vector<float> blc(ndim, 1.0F);   // 1-based (oneRel=true)
    std::vector<float> trc(ndim);
    std::vector<std::int32_t> shape_vec(ndim);
    for (std::size_t i = 0; i < ndim; ++i) {
        trc[i] = static_cast<float>(impl_->shape[i]); // 1-based
        shape_vec[i] = static_cast<std::int32_t>(impl_->shape[i]);
    }
    box.set("blc", RecordValue(RecordValue::float_array{
                        {static_cast<std::uint64_t>(ndim)}, std::move(blc)}));
    box.set("trc", RecordValue(RecordValue::float_array{
                        {static_cast<std::uint64_t>(ndim)}, std::move(trc)}));
    box.set("shape", RecordValue(RecordValue::int32_array{
                         {static_cast<std::uint64_t>(ndim)}, std::move(shape_vec)}));
    mask_rec.set("box", RecordValue::from_record(std::move(box)));

    // Build or update the "masks" record.
    Record masks_parent;
    if (auto* existing = kw.find("masks")) {
        if (auto* rp = std::get_if<RecordValue::record_ptr>(&existing->storage())) {
            if (*rp) masks_parent = **rp;
        }
    }
    masks_parent.set(mask_name, RecordValue::from_record(std::move(mask_rec)));
    kw.set("masks", RecordValue::from_record(std::move(masks_parent)));

    if (set_default) {
        kw.set("Image_defaultmask", RecordValue(mask_name));
        impl_->default_mask_name = mask_name;
        impl_->has_mask = true;
    }

    // Flush the mask subtable.
    mask_table.flush();
}

template <typename T>
void PagedImage<T>::flush() {
    if (impl_->table && impl_->writable) {
        save_metadata();
        impl_->table->flush();
    }
}

template <typename T>
void PagedImage<T>::save_metadata() {
    auto& kw = impl_->table->rw_keywords();
    // Subtable reference for the logtable (tp_table keyword).
    // "Table: " prefix triggers type code 12 (tp_table) in the record writer.
    kw.set("logtable", RecordValue(std::string("Table: ././logtable")));
    kw.set("coords", RecordValue::from_record(impl_->cs.save()));
    kw.set("imageinfo", RecordValue::from_record(impl_->info.to_record()));
    kw.set("miscinfo", RecordValue::from_record(impl_->misc));
    kw.set("units", RecordValue(impl_->units));
}

template <typename T>
void PagedImage<T>::load_metadata() {
    const auto& kw = impl_->table->keywords();
    // Coordinates.
    if (auto* v = kw.find("coords")) {
        if (auto* rp = std::get_if<RecordValue::record_ptr>(&v->storage())) {
            impl_->cs = CoordinateSystem::restore(**rp);
        }
    }
    // Image info.
    if (auto* v = kw.find("imageinfo")) {
        if (auto* rp = std::get_if<RecordValue::record_ptr>(&v->storage())) {
            impl_->info = ImageInfo::from_record(**rp);
        }
    }
    // Misc info.
    if (auto* v = kw.find("miscinfo")) {
        if (auto* rp = std::get_if<RecordValue::record_ptr>(&v->storage())) {
            impl_->misc = **rp;
        }
    }
    // Units.
    if (auto* v = kw.find("units")) {
        if (auto* s = std::get_if<std::string>(&v->storage())) {
            impl_->units = *s;
        }
    }
    // Default mask.
    if (auto* v = kw.find("Image_defaultmask")) {
        if (auto* s = std::get_if<std::string>(&v->storage())) {
            impl_->default_mask_name = *s;
            impl_->has_mask = !s->empty();
        }
    }
}

// ── PagedImage<float> I/O ────────────────────────────────────────────

template <>
LatticeArray<float> PagedImage<float>::get() const {
    auto vals = impl_->table->read_array_float_cell("map", 0);
    return LatticeArray<float>(IPosition(impl_->shape), std::move(vals));
}

template <>
LatticeArray<float> PagedImage<float>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <>
float PagedImage<float>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedImage<float>::put(const LatticeArray<float>& data) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    impl_->table->write_array_float_cell("map", 0, data.flat());
}

template <>
void PagedImage<float>::put_slice(const LatticeArray<float>& data,
                                  const Slicer& slicer) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    impl_->table->write_array_float_cell("map", 0, full.flat());
}

template <>
void PagedImage<float>::put_at(const float& value, const IPosition& where) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    impl_->table->write_array_float_cell("map", 0, full.flat());
}

// ── PagedImage<double> I/O ───────────────────────────────────────────

template <>
LatticeArray<double> PagedImage<double>::get() const {
    auto vals = impl_->table->read_array_double_cell("map", 0);
    return LatticeArray<double>(IPosition(impl_->shape), std::move(vals));
}

template <>
LatticeArray<double> PagedImage<double>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <>
double PagedImage<double>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedImage<double>::put(const LatticeArray<double>& data) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    impl_->table->write_array_double_cell("map", 0, data.flat());
}

template <>
void PagedImage<double>::put_slice(const LatticeArray<double>& data,
                                   const Slicer& slicer) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    impl_->table->write_array_double_cell("map", 0, full.flat());
}

template <>
void PagedImage<double>::put_at(const double& value, const IPosition& where) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    impl_->table->write_array_double_cell("map", 0, full.flat());
}

// ── PagedImage<complex<float>> I/O ───────────────────────────────────

template <>
LatticeArray<std::complex<float>> PagedImage<std::complex<float>>::get() const {
    auto vals = impl_->table->read_array_complex_cell("map", 0);
    return LatticeArray<std::complex<float>>(IPosition(impl_->shape),
                                             std::move(vals));
}

template <>
LatticeArray<std::complex<float>>
PagedImage<std::complex<float>>::get_slice(const Slicer& slicer) const {
    auto full = get();
    return full.get_slice(slicer);
}

template <>
std::complex<float>
PagedImage<std::complex<float>>::get_at(const IPosition& where) const {
    auto full = get();
    return full.at(where);
}

template <>
void PagedImage<std::complex<float>>::put(
    const LatticeArray<std::complex<float>>& data) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    std::vector<std::int32_t> sh;
    for (std::size_t i = 0; i < data.shape().ndim(); ++i) {
        sh.push_back(static_cast<std::int32_t>(data.shape()[i]));
    }
    impl_->table->write_array_complex_cell("map", 0, data.flat(), sh);
}

template <>
void PagedImage<std::complex<float>>::put_slice(
    const LatticeArray<std::complex<float>>& data, const Slicer& slicer) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put_slice(data, slicer);
    put(full);
}

template <>
void PagedImage<std::complex<float>>::put_at(
    const std::complex<float>& value, const IPosition& where) {
    if (!impl_->writable)
        throw std::runtime_error("PagedImage is read-only");
    auto full = get();
    full.make_unique();
    full.put(where, value);
    put(full);
}

// Explicit instantiations.
template class PagedImage<float>;
template class PagedImage<double>;
template class PagedImage<std::complex<float>>;

} // namespace casacore_mini
