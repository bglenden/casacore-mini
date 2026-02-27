#include "casacore_mini/image_region.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace casacore_mini {

// ── Helpers ──────────────────────────────────────────────────────────

/// Find the pixel axis index in the coordinate system matching an axis name.
/// Returns the system pixel axis index, or throws if not found.
static std::size_t find_pixel_axis_by_name(const CoordinateSystem& cs,
                                           const std::string& name) {
    for (std::size_t ci = 0; ci < cs.n_coordinates(); ++ci) {
        auto& coord = cs.coordinate(ci);
        auto names = coord.world_axis_names();
        for (std::size_t ai = 0; ai < names.size(); ++ai) {
            if (names[ai] == name) {
                auto found = cs.find_world_axis(ci);
                // The system pixel axis for this coordinate's axis ai.
                // For simple systems, pixel axis i maps to world axis i.
                // We search by iterating pixel axes.
                for (std::size_t px = 0; px < cs.n_pixel_axes(); ++px) {
                    auto pair = cs.find_pixel_axis(px);
                    if (pair && pair->first == ci && pair->second == ai) {
                        return px;
                    }
                }
                (void)found;
            }
        }
    }
    throw std::runtime_error("find_pixel_axis_by_name: axis '" + name + "' not found");
}

/// Convert a world coordinate value to pixel using the coordinate system.
/// Returns the pixel value for a single axis.
static double world_to_pixel_1d(const CoordinateSystem& cs,
                                std::size_t pixel_axis,
                                double world_val,
                                const std::string& /*axis_name*/) {
    auto pair = cs.find_pixel_axis(pixel_axis);
    if (!pair) throw std::runtime_error("world_to_pixel_1d: axis not found");
    auto& coord = cs.coordinate(pair->first);
    // Build full world vector with reference values, replacing our axis.
    auto ref_world = coord.reference_value();
    ref_world.at(pair->second) = world_val;
    auto pixels = coord.to_pixel(ref_world);
    return pixels.at(pair->second);
}

// ── Helper: serialize/deserialize vectors ────────────────────────────

static void put_double_vec(Record& rec, const std::string& key,
                           const std::vector<double>& v) {
    RecordArray<double> arr;
    arr.shape = {v.size()};
    arr.elements = v;
    rec.set(key, RecordValue(std::move(arr)));
}

static std::vector<double> get_double_vec(const Record& rec,
                                          const std::string& key) {
    auto* v = rec.find(key);
    if (!v) throw std::runtime_error("missing key '" + key + "'");
    auto* arr = std::get_if<RecordValue::double_array>(&v->storage());
    if (!arr) throw std::runtime_error("key '" + key + "' not double array");
    return arr->elements;
}

static void put_string_vec(Record& rec, const std::string& key,
                           const std::vector<std::string>& v) {
    RecordArray<std::string> arr;
    arr.shape = {v.size()};
    arr.elements = v;
    rec.set(key, RecordValue(std::move(arr)));
}

static std::vector<std::string> get_string_vec(const Record& rec,
                                               const std::string& key) {
    auto* v = rec.find(key);
    if (!v) throw std::runtime_error("missing key '" + key + "'");
    auto* arr = std::get_if<RecordValue::string_array>(&v->storage());
    if (!arr) throw std::runtime_error("key '" + key + "' not string array");
    return arr->elements;
}

static std::string get_string(const Record& rec, const std::string& key) {
    auto* v = rec.find(key);
    if (!v) throw std::runtime_error("missing key '" + key + "'");
    auto* s = std::get_if<std::string>(&v->storage());
    if (!s) throw std::runtime_error("key '" + key + "' not string");
    return *s;
}

// ── WcRegion dispatch ────────────────────────────────────────────────

std::unique_ptr<WcRegion> WcRegion::from_record(const Record& rec) {
    auto t = get_string(rec, "type");
    if (t == "WcBox") return WcBox::from_record(rec);
    if (t == "WcEllipsoid") return WcEllipsoid::from_record(rec);
    if (t == "WcPolygon") return WcPolygon::from_record(rec);
    throw std::runtime_error("WcRegion::from_record: unknown type '" + t + "'");
}

// ── WcBox ────────────────────────────────────────────────────────────

WcBox::WcBox(std::vector<double> blc, std::vector<double> trc,
             std::vector<std::string> axis_names,
             std::vector<std::string> axis_units)
    : blc_(std::move(blc)), trc_(std::move(trc)),
      axis_names_(std::move(axis_names)),
      axis_units_(std::move(axis_units)) {
    if (blc_.size() != trc_.size() || blc_.size() != axis_names_.size() ||
        blc_.size() != axis_units_.size())
        throw std::invalid_argument("WcBox: dimension mismatch");
}

std::unique_ptr<WcRegion> WcBox::clone() const {
    return std::make_unique<WcBox>(blc_, trc_, axis_names_, axis_units_);
}

Record WcBox::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcBox")));
    put_double_vec(rec, "blc", blc_);
    put_double_vec(rec, "trc", trc_);
    put_string_vec(rec, "axis_names", axis_names_);
    put_string_vec(rec, "axis_units", axis_units_);
    return rec;
}

std::unique_ptr<WcBox> WcBox::from_record(const Record& rec) {
    return std::make_unique<WcBox>(
        get_double_vec(rec, "blc"), get_double_vec(rec, "trc"),
        get_string_vec(rec, "axis_names"), get_string_vec(rec, "axis_units"));
}

std::unique_ptr<LcRegion>
WcBox::to_lc_region(const CoordinateSystem& cs,
                    const IPosition& lattice_shape) const {
    auto nd = lattice_shape.ndim();
    IPosition pix_blc(nd, 0);
    IPosition pix_trc(nd);
    for (std::size_t d = 0; d < nd; ++d) {
        pix_trc[d] = lattice_shape[d] - 1;
    }

    for (std::size_t i = 0; i < blc_.size(); ++i) {
        auto px = find_pixel_axis_by_name(cs, axis_names_[i]);
        auto p_blc = world_to_pixel_1d(cs, px, blc_[i], axis_names_[i]);
        auto p_trc = world_to_pixel_1d(cs, px, trc_[i], axis_names_[i]);
        if (p_blc > p_trc) std::swap(p_blc, p_trc);
        pix_blc[px] = std::max(std::int64_t{0},
                                static_cast<std::int64_t>(std::floor(p_blc)));
        pix_trc[px] = std::min(lattice_shape[px] - 1,
                                static_cast<std::int64_t>(std::ceil(p_trc)));
    }
    return std::make_unique<LcBox>(std::move(pix_blc), std::move(pix_trc),
                                   IPosition(lattice_shape));
}

// ── WcEllipsoid ──────────────────────────────────────────────────────

WcEllipsoid::WcEllipsoid(std::vector<double> center, std::vector<double> radii,
                         std::vector<std::string> axis_names,
                         std::vector<std::string> axis_units)
    : center_(std::move(center)), radii_(std::move(radii)),
      axis_names_(std::move(axis_names)),
      axis_units_(std::move(axis_units)) {
    if (center_.size() != radii_.size() || center_.size() != axis_names_.size())
        throw std::invalid_argument("WcEllipsoid: dimension mismatch");
}

std::unique_ptr<WcRegion> WcEllipsoid::clone() const {
    return std::make_unique<WcEllipsoid>(center_, radii_, axis_names_, axis_units_);
}

Record WcEllipsoid::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcEllipsoid")));
    put_double_vec(rec, "center", center_);
    put_double_vec(rec, "radii", radii_);
    put_string_vec(rec, "axis_names", axis_names_);
    put_string_vec(rec, "axis_units", axis_units_);
    return rec;
}

std::unique_ptr<WcEllipsoid> WcEllipsoid::from_record(const Record& rec) {
    return std::make_unique<WcEllipsoid>(
        get_double_vec(rec, "center"), get_double_vec(rec, "radii"),
        get_string_vec(rec, "axis_names"), get_string_vec(rec, "axis_units"));
}

std::unique_ptr<LcRegion>
WcEllipsoid::to_lc_region(const CoordinateSystem& cs,
                          const IPosition& lattice_shape) const {
    std::vector<double> pix_center(center_.size());
    std::vector<double> pix_radii(radii_.size());
    for (std::size_t i = 0; i < center_.size(); ++i) {
        auto px = find_pixel_axis_by_name(cs, axis_names_[i]);
        pix_center[i] = world_to_pixel_1d(cs, px, center_[i], axis_names_[i]);
        // Compute radius in pixels: convert center+radius and center-radius.
        auto p_plus = world_to_pixel_1d(cs, px, center_[i] + radii_[i],
                                        axis_names_[i]);
        auto p_minus = world_to_pixel_1d(cs, px, center_[i] - radii_[i],
                                         axis_names_[i]);
        pix_radii[i] = std::abs(p_plus - p_minus) / 2.0;
    }
    return std::make_unique<LcEllipsoid>(
        std::move(pix_center), std::move(pix_radii),
        IPosition(lattice_shape));
}

// ── WcPolygon ────────────────────────────────────────────────────────

WcPolygon::WcPolygon(std::vector<double> x, std::vector<double> y,
                     std::string x_axis_name, std::string y_axis_name,
                     std::string x_axis_unit, std::string y_axis_unit)
    : x_(std::move(x)), y_(std::move(y)),
      x_axis_name_(std::move(x_axis_name)),
      y_axis_name_(std::move(y_axis_name)),
      x_axis_unit_(std::move(x_axis_unit)),
      y_axis_unit_(std::move(y_axis_unit)) {
    if (x_.size() != y_.size() || x_.size() < 3)
        throw std::invalid_argument("WcPolygon: need >= 3 vertices with matching sizes");
}

std::unique_ptr<WcRegion> WcPolygon::clone() const {
    return std::make_unique<WcPolygon>(x_, y_, x_axis_name_, y_axis_name_,
                                       x_axis_unit_, y_axis_unit_);
}

Record WcPolygon::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcPolygon")));
    put_double_vec(rec, "x", x_);
    put_double_vec(rec, "y", y_);
    rec.set("x_axis_name", RecordValue(x_axis_name_));
    rec.set("y_axis_name", RecordValue(y_axis_name_));
    rec.set("x_axis_unit", RecordValue(x_axis_unit_));
    rec.set("y_axis_unit", RecordValue(y_axis_unit_));
    return rec;
}

std::unique_ptr<WcPolygon> WcPolygon::from_record(const Record& rec) {
    return std::make_unique<WcPolygon>(
        get_double_vec(rec, "x"), get_double_vec(rec, "y"),
        get_string(rec, "x_axis_name"), get_string(rec, "y_axis_name"),
        get_string(rec, "x_axis_unit"), get_string(rec, "y_axis_unit"));
}

std::unique_ptr<LcRegion>
WcPolygon::to_lc_region(const CoordinateSystem& cs,
                        const IPosition& lattice_shape) const {
    auto px_x = find_pixel_axis_by_name(cs, x_axis_name_);
    auto px_y = find_pixel_axis_by_name(cs, y_axis_name_);
    (void)px_x; (void)px_y;
    // Convert each vertex from world to pixel.
    std::vector<double> pix_x(x_.size());
    std::vector<double> pix_y(y_.size());
    for (std::size_t i = 0; i < x_.size(); ++i) {
        pix_x[i] = world_to_pixel_1d(cs, px_x, x_[i], x_axis_name_);
        pix_y[i] = world_to_pixel_1d(cs, px_y, y_[i], y_axis_name_);
    }
    // Construct 2D lattice shape for the polygon.
    IPosition poly_lat{lattice_shape[px_x], lattice_shape[px_y]};
    return std::make_unique<LcPolygon>(std::move(pix_x), std::move(pix_y),
                                       std::move(poly_lat));
}

// ── WcUnion ──────────────────────────────────────────────────────────

WcUnion::WcUnion(std::vector<std::unique_ptr<WcRegion>> regions)
    : regions_(std::move(regions)) {
    if (regions_.empty())
        throw std::invalid_argument("WcUnion: need >= 1 region");
}

std::size_t WcUnion::ndim() const {
    return regions_.empty() ? 0 : regions_[0]->ndim();
}

std::unique_ptr<WcRegion> WcUnion::clone() const {
    std::vector<std::unique_ptr<WcRegion>> clones;
    for (auto& r : regions_) clones.push_back(r->clone());
    return std::make_unique<WcUnion>(std::move(clones));
}

Record WcUnion::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcUnion")));
    rec.set("n_regions", RecordValue(static_cast<std::int64_t>(regions_.size())));
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        rec.set("region_" + std::to_string(i),
                RecordValue::from_record(regions_[i]->to_record()));
    }
    return rec;
}

std::unique_ptr<LcRegion>
WcUnion::to_lc_region(const CoordinateSystem& cs,
                      const IPosition& lattice_shape) const {
    std::vector<std::unique_ptr<LcRegion>> lc_regions;
    for (auto& r : regions_) {
        lc_regions.push_back(r->to_lc_region(cs, lattice_shape));
    }
    return std::make_unique<LcUnion>(std::move(lc_regions),
                                     IPosition(lattice_shape));
}

// ── WcIntersection ───────────────────────────────────────────────────

WcIntersection::WcIntersection(std::vector<std::unique_ptr<WcRegion>> regions)
    : regions_(std::move(regions)) {
    if (regions_.empty())
        throw std::invalid_argument("WcIntersection: need >= 1 region");
}

std::size_t WcIntersection::ndim() const {
    return regions_.empty() ? 0 : regions_[0]->ndim();
}

std::unique_ptr<WcRegion> WcIntersection::clone() const {
    std::vector<std::unique_ptr<WcRegion>> clones;
    for (auto& r : regions_) clones.push_back(r->clone());
    return std::make_unique<WcIntersection>(std::move(clones));
}

Record WcIntersection::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcIntersection")));
    rec.set("n_regions", RecordValue(static_cast<std::int64_t>(regions_.size())));
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        rec.set("region_" + std::to_string(i),
                RecordValue::from_record(regions_[i]->to_record()));
    }
    return rec;
}

std::unique_ptr<LcRegion>
WcIntersection::to_lc_region(const CoordinateSystem& cs,
                             const IPosition& lattice_shape) const {
    std::vector<std::unique_ptr<LcRegion>> lc_regions;
    for (auto& r : regions_) {
        lc_regions.push_back(r->to_lc_region(cs, lattice_shape));
    }
    return std::make_unique<LcIntersection>(std::move(lc_regions),
                                            IPosition(lattice_shape));
}

// ── WcDifference ─────────────────────────────────────────────────────

WcDifference::WcDifference(std::unique_ptr<WcRegion> region1,
                           std::unique_ptr<WcRegion> region2)
    : region1_(std::move(region1)), region2_(std::move(region2)) {}

std::size_t WcDifference::ndim() const {
    return region1_ ? region1_->ndim() : 0;
}

std::unique_ptr<WcRegion> WcDifference::clone() const {
    return std::make_unique<WcDifference>(region1_->clone(), region2_->clone());
}

Record WcDifference::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcDifference")));
    rec.set("region1", RecordValue::from_record(region1_->to_record()));
    rec.set("region2", RecordValue::from_record(region2_->to_record()));
    return rec;
}

std::unique_ptr<LcRegion>
WcDifference::to_lc_region(const CoordinateSystem& cs,
                           const IPosition& lattice_shape) const {
    return std::make_unique<LcDifference>(
        region1_->to_lc_region(cs, lattice_shape),
        region2_->to_lc_region(cs, lattice_shape),
        IPosition(lattice_shape));
}

// ── WcComplement ─────────────────────────────────────────────────────

WcComplement::WcComplement(std::unique_ptr<WcRegion> region)
    : region_(std::move(region)) {}

std::size_t WcComplement::ndim() const {
    return region_ ? region_->ndim() : 0;
}

std::unique_ptr<WcRegion> WcComplement::clone() const {
    return std::make_unique<WcComplement>(region_->clone());
}

Record WcComplement::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("WcComplement")));
    rec.set("region", RecordValue::from_record(region_->to_record()));
    return rec;
}

std::unique_ptr<LcRegion>
WcComplement::to_lc_region(const CoordinateSystem& cs,
                           const IPosition& lattice_shape) const {
    return std::make_unique<LcComplement>(
        region_->to_lc_region(cs, lattice_shape),
        IPosition(lattice_shape));
}

// ── ImageRegion ──────────────────────────────────────────────────────

ImageRegion::ImageRegion(std::unique_ptr<LcRegion> region)
    : kind_(RegionKind::lc_region), lc_region_(std::move(region)) {}

ImageRegion::ImageRegion(std::unique_ptr<WcRegion> region)
    : kind_(RegionKind::wc_region), wc_region_(std::move(region)) {}

ImageRegion::ImageRegion(Slicer slicer)
    : kind_(RegionKind::slicer), slicer_(std::move(slicer)) {}

std::unique_ptr<LcRegion>
ImageRegion::to_lc_region(const CoordinateSystem& cs,
                          const IPosition& lattice_shape) const {
    switch (kind_) {
    case RegionKind::lc_region:
        return lc_region_ ? lc_region_->clone() : nullptr;
    case RegionKind::wc_region:
        return wc_region_ ? wc_region_->to_lc_region(cs, lattice_shape) : nullptr;
    case RegionKind::slicer:
        return std::make_unique<LcBox>(slicer_, IPosition(lattice_shape));
    }
    return nullptr;
}

Record ImageRegion::to_record() const {
    Record rec;
    switch (kind_) {
    case RegionKind::lc_region:
        rec.set("region_kind", RecordValue(std::string("lc")));
        if (lc_region_) {
            rec.set("region", RecordValue::from_record(lc_region_->to_record()));
        }
        break;
    case RegionKind::wc_region:
        rec.set("region_kind", RecordValue(std::string("wc")));
        if (wc_region_) {
            rec.set("region", RecordValue::from_record(wc_region_->to_record()));
        }
        break;
    case RegionKind::slicer: {
        rec.set("region_kind", RecordValue(std::string("slicer")));
        RecordArray<std::int64_t> sa;
        sa.shape = {slicer_.ndim()};
        for (std::size_t i = 0; i < slicer_.ndim(); ++i) {
            sa.elements.push_back(slicer_.start()[i]);
        }
        rec.set("slicer_start", RecordValue(std::move(sa)));
        RecordArray<std::int64_t> la;
        la.shape = {slicer_.ndim()};
        for (std::size_t i = 0; i < slicer_.ndim(); ++i) {
            la.elements.push_back(slicer_.length()[i]);
        }
        rec.set("slicer_length", RecordValue(std::move(la)));
        break;
    }
    }
    return rec;
}

ImageRegion
ImageRegion::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto kind_str = get_string(rec, "region_kind");
    if (kind_str == "lc") {
        auto* rv = rec.find("region");
        if (!rv) throw std::runtime_error("ImageRegion: missing 'region'");
        auto* rp = std::get_if<RecordValue::record_ptr>(&rv->storage());
        if (!rp) throw std::runtime_error("ImageRegion: 'region' not record");
        return ImageRegion(LcRegion::from_record(**rp, lattice_shape));
    }
    if (kind_str == "wc") {
        auto* rv = rec.find("region");
        if (!rv) throw std::runtime_error("ImageRegion: missing 'region'");
        auto* rp = std::get_if<RecordValue::record_ptr>(&rv->storage());
        if (!rp) throw std::runtime_error("ImageRegion: 'region' not record");
        return ImageRegion(WcRegion::from_record(**rp));
    }
    if (kind_str == "slicer") {
        auto* sv = rec.find("slicer_start");
        auto* lv = rec.find("slicer_length");
        if (!sv || !lv) throw std::runtime_error("ImageRegion: missing slicer fields");
        auto* sa = std::get_if<RecordValue::int64_array>(&sv->storage());
        auto* la = std::get_if<RecordValue::int64_array>(&lv->storage());
        if (!sa || !la) throw std::runtime_error("ImageRegion: bad slicer types");
        IPosition start(sa->elements.size());
        IPosition length(la->elements.size());
        for (std::size_t i = 0; i < sa->elements.size(); ++i) {
            start[i] = sa->elements[i];
        }
        for (std::size_t i = 0; i < la->elements.size(); ++i) {
            length[i] = la->elements[i];
        }
        return ImageRegion(Slicer(std::move(start), std::move(length)));
    }
    throw std::runtime_error("ImageRegion: unknown kind '" + kind_str + "'");
}

// ── RegionHandler ────────────────────────────────────────────────────

void RegionHandler::define_region(std::string name, ImageRegion region) {
    regions_[std::move(name)] = std::move(region);
}

bool RegionHandler::has_region(const std::string& name) const {
    return regions_.count(name) > 0;
}

const ImageRegion& RegionHandler::get_region(const std::string& name) const {
    auto it = regions_.find(name);
    if (it == regions_.end())
        throw std::runtime_error("RegionHandler: no region '" + name + "'");
    return it->second;
}

void RegionHandler::remove_region(const std::string& name) {
    regions_.erase(name);
}

void RegionHandler::rename_region(const std::string& old_name,
                                  const std::string& new_name) {
    auto it = regions_.find(old_name);
    if (it == regions_.end())
        throw std::runtime_error("RegionHandler: no region '" + old_name + "'");
    auto region = std::move(it->second);
    regions_.erase(it);
    regions_[new_name] = std::move(region);
    if (default_mask_ == old_name) {
        default_mask_ = new_name;
    }
}

std::vector<std::string> RegionHandler::region_names() const {
    std::vector<std::string> names;
    names.reserve(regions_.size());
    for (auto& [k, v] : regions_) {
        names.push_back(k);
        (void)v;
    }
    std::sort(names.begin(), names.end());
    return names;
}

Record RegionHandler::to_record() const {
    Record rec;
    rec.set("n_regions", RecordValue(static_cast<std::int64_t>(regions_.size())));
    if (!default_mask_.empty()) {
        rec.set("default_mask", RecordValue(default_mask_));
    }
    for (auto& [name, region] : regions_) {
        rec.set(name, RecordValue::from_record(region.to_record()));
    }
    return rec;
}

void RegionHandler::from_record(const Record& rec,
                                const IPosition& lattice_shape) {
    regions_.clear();
    default_mask_.clear();

    if (auto* dm = rec.find("default_mask")) {
        if (auto* s = std::get_if<std::string>(&dm->storage())) {
            default_mask_ = *s;
        }
    }

    auto* nv = rec.find("n_regions");
    if (!nv) return;
    auto* ni = std::get_if<std::int64_t>(&nv->storage());
    if (!ni) return;

    for (auto& [key, val] : rec.entries()) {
        if (key == "n_regions" || key == "default_mask") continue;
        auto* rp = std::get_if<RecordValue::record_ptr>(&val.storage());
        if (!rp) continue;
        regions_[key] = ImageRegion::from_record(**rp, lattice_shape);
    }
}

// ── RegionManager ────────────────────────────────────────────────────

std::unique_ptr<WcBox>
RegionManager::wc_box(std::vector<double> blc, std::vector<double> trc,
                      std::vector<std::string> axis_names,
                      std::vector<std::string> axis_units) {
    return std::make_unique<WcBox>(std::move(blc), std::move(trc),
                                   std::move(axis_names), std::move(axis_units));
}

std::unique_ptr<WcEllipsoid>
RegionManager::wc_ellipsoid(std::vector<double> center,
                            std::vector<double> radii,
                            std::vector<std::string> axis_names,
                            std::vector<std::string> axis_units) {
    return std::make_unique<WcEllipsoid>(
        std::move(center), std::move(radii),
        std::move(axis_names), std::move(axis_units));
}

std::unique_ptr<WcPolygon>
RegionManager::wc_polygon(std::vector<double> x, std::vector<double> y,
                          std::string x_axis_name, std::string y_axis_name,
                          std::string x_axis_unit, std::string y_axis_unit) {
    return std::make_unique<WcPolygon>(
        std::move(x), std::move(y),
        std::move(x_axis_name), std::move(y_axis_name),
        std::move(x_axis_unit), std::move(y_axis_unit));
}

std::unique_ptr<WcUnion>
RegionManager::make_union(std::vector<std::unique_ptr<WcRegion>> regions) {
    return std::make_unique<WcUnion>(std::move(regions));
}

std::unique_ptr<WcIntersection>
RegionManager::make_intersection(std::vector<std::unique_ptr<WcRegion>> regions) {
    return std::make_unique<WcIntersection>(std::move(regions));
}

std::unique_ptr<WcDifference>
RegionManager::make_difference(std::unique_ptr<WcRegion> r1,
                               std::unique_ptr<WcRegion> r2) {
    return std::make_unique<WcDifference>(std::move(r1), std::move(r2));
}

std::unique_ptr<WcComplement>
RegionManager::make_complement(std::unique_ptr<WcRegion> region) {
    return std::make_unique<WcComplement>(std::move(region));
}

} // namespace casacore_mini
