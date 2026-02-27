#include "casacore_mini/lattice_region.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace casacore_mini {

// ── LcRegion base ────────────────────────────────────────────────────

LatticeArray<bool> LcRegion::get_mask() const {
    return LatticeArray<bool>(shape(), true);
}

std::unique_ptr<LcRegion>
LcRegion::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto* tv = rec.find("type");
    if (!tv) throw std::runtime_error("LcRegion::from_record: missing 'type'");
    auto* ts = std::get_if<std::string>(&tv->storage());
    if (!ts) throw std::runtime_error("LcRegion::from_record: 'type' not string");

    if (*ts == "LcBox") return LcBox::from_record(rec, lattice_shape);
    if (*ts == "LcPixelSet") return LcPixelSet::from_record(rec, lattice_shape);
    if (*ts == "LcEllipsoid") return LcEllipsoid::from_record(rec, lattice_shape);
    if (*ts == "LcPolygon") return LcPolygon::from_record(rec, lattice_shape);
    throw std::runtime_error("LcRegion::from_record: unknown type '" + *ts + "'");
}

// ── Helper: serialize/deserialize IPosition as int64 array ───────────

static void put_iposition(Record& rec, const std::string& key,
                          const IPosition& pos) {
    RecordArray<std::int64_t> arr;
    arr.shape = {pos.ndim()};
    for (std::size_t i = 0; i < pos.ndim(); ++i) {
        arr.elements.push_back(pos[i]);
    }
    rec.set(key, RecordValue(std::move(arr)));
}

static IPosition get_iposition(const Record& rec, const std::string& key) {
    auto* v = rec.find(key);
    if (!v) throw std::runtime_error("missing key '" + key + "'");
    auto* arr = std::get_if<RecordValue::int64_array>(&v->storage());
    if (!arr) throw std::runtime_error("key '" + key + "' not int64 array");
    std::vector<std::int64_t> vals(arr->elements.begin(), arr->elements.end());
    return IPosition(std::move(vals));
}

// ── Helpers (must precede LcBox) ──────────────────────────────────────

// Compute length from blc/trc.
static IPosition compute_length(const IPosition& blc, const IPosition& trc) {
    IPosition len(blc.ndim());
    for (std::size_t i = 0; i < blc.ndim(); ++i) {
        len[i] = trc[i] - blc[i] + 1;
    }
    return len;
}

// Compute trc from slicer.
static IPosition compute_trc(const Slicer& s) {
    IPosition trc(s.ndim());
    for (std::size_t i = 0; i < s.ndim(); ++i) {
        trc[i] = s.start()[i] + s.length()[i] - 1;
    }
    return trc;
}

// ── LcBox ────────────────────────────────────────────────────────────

LcBox::LcBox(IPosition blc, IPosition trc, IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(blc), compute_length(blc, trc))),
      blc_(std::move(blc)), trc_(std::move(trc)) {
    if (blc_.ndim() != trc_.ndim())
        throw std::invalid_argument("LcBox: blc/trc ndim mismatch");
}

LcBox::LcBox(Slicer slicer, IPosition lattice_shape)
    : LcRegion(std::move(lattice_shape), slicer),
      blc_(slicer.start()),
      trc_(compute_trc(slicer)) {}

std::unique_ptr<LcRegion> LcBox::clone() const {
    return std::make_unique<LcBox>(blc_, trc_, lattice_shape());
}

Record LcBox::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcBox")));
    put_iposition(rec, "blc", blc_);
    put_iposition(rec, "trc", trc_);
    return rec;
}

std::unique_ptr<LcBox>
LcBox::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto blc = get_iposition(rec, "blc");
    auto trc = get_iposition(rec, "trc");
    return std::make_unique<LcBox>(std::move(blc), std::move(trc),
                                   IPosition(lattice_shape));
}

// ── LcPixelSet ───────────────────────────────────────────────────────

LcPixelSet::LcPixelSet(LatticeArray<bool> mask, IPosition blc,
                       IPosition lattice_shape)
    : LcRegion(std::move(lattice_shape),
               Slicer(IPosition(blc), mask.shape())),
      mask_(std::move(mask)) {}

std::unique_ptr<LcRegion> LcPixelSet::clone() const {
    return std::make_unique<LcPixelSet>(
        mask_, bounding_box().start(), lattice_shape());
}

Record LcPixelSet::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcPixelSet")));
    put_iposition(rec, "blc", bounding_box().start());
    // Store mask as flattened bool array.
    RecordArray<std::int32_t> mask_arr;
    mask_arr.shape.resize(mask_.shape().ndim());
    for (std::size_t i = 0; i < mask_.shape().ndim(); ++i) {
        mask_arr.shape[i] = static_cast<std::uint64_t>(mask_.shape()[i]);
    }
    for (std::size_t i = 0; i < mask_.nelements(); ++i) {
        mask_arr.elements.push_back(mask_.flat()[i] ? 1 : 0);
    }
    rec.set("mask", RecordValue(std::move(mask_arr)));
    return rec;
}

std::unique_ptr<LcPixelSet>
LcPixelSet::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto blc = get_iposition(rec, "blc");
    auto* mv = rec.find("mask");
    if (!mv) throw std::runtime_error("LcPixelSet: missing 'mask'");
    auto* marr = std::get_if<RecordValue::int32_array>(&mv->storage());
    if (!marr) throw std::runtime_error("LcPixelSet: 'mask' not int32 array");

    IPosition mshape(marr->shape.size());
    for (std::size_t i = 0; i < marr->shape.size(); ++i) {
        mshape[i] = static_cast<std::int64_t>(marr->shape[i]);
    }
    LatticeArray<bool> mask(mshape, false);
    mask.make_unique();
    for (std::size_t i = 0; i < marr->elements.size(); ++i) {
        if (marr->elements[i] != 0) {
            auto pos = delinearize(static_cast<std::int64_t>(i), mshape);
            mask.put(pos, true);
        }
    }
    return std::make_unique<LcPixelSet>(std::move(mask), std::move(blc),
                                        IPosition(lattice_shape));
}

// ── LcEllipsoid ──────────────────────────────────────────────────────

LcEllipsoid::LcEllipsoid(std::vector<double> center,
                         std::vector<double> semi_axes,
                         IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      center_(std::move(center)),
      semi_axes_(std::move(semi_axes)) {
    if (center_.size() != semi_axes_.size())
        throw std::invalid_argument("LcEllipsoid: center/semi_axes size mismatch");
    compute_bounding_box();
}

void LcEllipsoid::compute_bounding_box() {
    auto nd = center_.size();
    IPosition blc(nd);
    IPosition trc(nd);
    for (std::size_t i = 0; i < nd; ++i) {
        blc[i] = std::max(std::int64_t{0},
                          static_cast<std::int64_t>(std::floor(center_[i] - semi_axes_[i])));
        trc[i] = std::min(lattice_shape()[i] - 1,
                          static_cast<std::int64_t>(std::ceil(center_[i] + semi_axes_[i])));
    }
    set_bounding_box(Slicer(std::move(blc), compute_length(bounding_box().start(), trc)));
    // Recompute with proper blc/trc:
    IPosition blc2(nd);
    IPosition len(nd);
    for (std::size_t i = 0; i < nd; ++i) {
        blc2[i] = std::max(std::int64_t{0},
                           static_cast<std::int64_t>(std::floor(center_[i] - semi_axes_[i])));
        auto trc_i = std::min(lattice_shape()[i] - 1,
                              static_cast<std::int64_t>(std::ceil(center_[i] + semi_axes_[i])));
        len[i] = trc_i - blc2[i] + 1;
    }
    set_bounding_box(Slicer(std::move(blc2), std::move(len)));
}

LatticeArray<bool> LcEllipsoid::get_mask() const {
    auto sh = shape();
    LatticeArray<bool> mask(sh, false);
    mask.make_unique();
    auto nd = center_.size();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        double dist_sq = 0.0;
        for (std::size_t d = 0; d < nd; ++d) {
            double offset = static_cast<double>(bounding_box().start()[d] + pos[d]) - center_[d];
            if (semi_axes_[d] > 0.0) {
                dist_sq += (offset * offset) / (semi_axes_[d] * semi_axes_[d]);
            }
        }
        if (dist_sq <= 1.0) {
            mask.put(delinearize(static_cast<std::int64_t>(idx), sh), true);
        }
    }
    return mask;
}

std::unique_ptr<LcRegion> LcEllipsoid::clone() const {
    return std::make_unique<LcEllipsoid>(center_, semi_axes_, lattice_shape());
}

Record LcEllipsoid::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcEllipsoid")));
    RecordArray<double> ca;
    ca.shape = {center_.size()};
    ca.elements = center_;
    rec.set("center", RecordValue(std::move(ca)));
    RecordArray<double> sa;
    sa.shape = {semi_axes_.size()};
    sa.elements = semi_axes_;
    rec.set("semi_axes", RecordValue(std::move(sa)));
    return rec;
}

std::unique_ptr<LcEllipsoid>
LcEllipsoid::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto* cv = rec.find("center");
    auto* sv = rec.find("semi_axes");
    if (!cv || !sv) throw std::runtime_error("LcEllipsoid: missing fields");
    auto* ca = std::get_if<RecordValue::double_array>(&cv->storage());
    auto* sa = std::get_if<RecordValue::double_array>(&sv->storage());
    if (!ca || !sa) throw std::runtime_error("LcEllipsoid: bad field types");
    return std::make_unique<LcEllipsoid>(ca->elements, sa->elements,
                                         IPosition(lattice_shape));
}

// ── LcPolygon ────────────────────────────────────────────────────────

LcPolygon::LcPolygon(std::vector<double> x, std::vector<double> y,
                     IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      x_(std::move(x)), y_(std::move(y)) {
    if (this->lattice_shape().ndim() != 2)
        throw std::invalid_argument("LcPolygon: requires 2D lattice");
    if (x_.size() != y_.size() || x_.size() < 3)
        throw std::invalid_argument("LcPolygon: need >= 3 vertices");
    compute_bounding_box_and_mask();
}

void LcPolygon::compute_bounding_box_and_mask() {
    double xmin = *std::min_element(x_.begin(), x_.end());
    double xmax = *std::max_element(x_.begin(), x_.end());
    double ymin = *std::min_element(y_.begin(), y_.end());
    double ymax = *std::max_element(y_.begin(), y_.end());

    auto ix0 = std::max(std::int64_t{0}, static_cast<std::int64_t>(std::floor(xmin)));
    auto ix1 = std::min(lattice_shape()[0] - 1, static_cast<std::int64_t>(std::ceil(xmax)));
    auto iy0 = std::max(std::int64_t{0}, static_cast<std::int64_t>(std::floor(ymin)));
    auto iy1 = std::min(lattice_shape()[1] - 1, static_cast<std::int64_t>(std::ceil(ymax)));

    IPosition blc{ix0, iy0};
    IPosition len{ix1 - ix0 + 1, iy1 - iy0 + 1};
    set_bounding_box(Slicer(std::move(blc), std::move(len)));

    // Compute mask using ray-casting point-in-polygon test.
    auto sh = shape();
    mask_cache_ = LatticeArray<bool>(sh, false);
    mask_cache_.make_unique();
    auto nv = x_.size();
    for (std::int64_t jy = 0; jy < sh[1]; ++jy) {
        for (std::int64_t jx = 0; jx < sh[0]; ++jx) {
            double px = static_cast<double>(bounding_box().start()[0] + jx) + 0.5;
            double py = static_cast<double>(bounding_box().start()[1] + jy) + 0.5;
            bool inside = false;
            for (std::size_t i = 0, j = nv - 1; i < nv; j = i++) {
                if (((y_[i] > py) != (y_[j] > py)) &&
                    (px < (x_[j] - x_[i]) * (py - y_[i]) / (y_[j] - y_[i]) + x_[i])) {
                    inside = !inside;
                }
            }
            if (inside) {
                mask_cache_.put(IPosition{jx, jy}, true);
            }
        }
    }
}

LatticeArray<bool> LcPolygon::get_mask() const {
    return mask_cache_;
}

std::unique_ptr<LcRegion> LcPolygon::clone() const {
    return std::make_unique<LcPolygon>(x_, y_, lattice_shape());
}

Record LcPolygon::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcPolygon")));
    RecordArray<double> xa;
    xa.shape = {x_.size()};
    xa.elements = x_;
    rec.set("x", RecordValue(std::move(xa)));
    RecordArray<double> ya;
    ya.shape = {y_.size()};
    ya.elements = y_;
    rec.set("y", RecordValue(std::move(ya)));
    return rec;
}

std::unique_ptr<LcPolygon>
LcPolygon::from_record(const Record& rec, const IPosition& lattice_shape) {
    auto* xv = rec.find("x");
    auto* yv = rec.find("y");
    if (!xv || !yv) throw std::runtime_error("LcPolygon: missing fields");
    auto* xa = std::get_if<RecordValue::double_array>(&xv->storage());
    auto* ya = std::get_if<RecordValue::double_array>(&yv->storage());
    if (!xa || !ya) throw std::runtime_error("LcPolygon: bad field types");
    return std::make_unique<LcPolygon>(xa->elements, ya->elements,
                                       IPosition(lattice_shape));
}

// ── LcMask ───────────────────────────────────────────────────────────

LcMask::LcMask(LatticeArray<bool> mask, IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      mask_(std::move(mask)) {}

std::unique_ptr<LcRegion> LcMask::clone() const {
    return std::make_unique<LcMask>(mask_, lattice_shape());
}

Record LcMask::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcMask")));
    return rec;
}

// ── LcPagedMask ──────────────────────────────────────────────────────

LcPagedMask::LcPagedMask(IPosition lattice_shape, IPosition mask_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      mask_shape_(std::move(mask_shape)) {}

std::unique_ptr<LcRegion> LcPagedMask::clone() const {
    return std::make_unique<LcPagedMask>(lattice_shape(), mask_shape_);
}

Record LcPagedMask::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcPagedMask")));
    return rec;
}

LatticeArray<bool> LcPagedMask::get_mask() const {
    // Stub: return all-true for now.
    return LatticeArray<bool>(mask_shape_, true);
}

// ── Compound: compute bounding box from sub-regions ──────────────────

static Slicer compute_union_bb(
    const std::vector<std::unique_ptr<LcRegion>>& regions,
    const IPosition& lattice_shape) {
    if (regions.empty())
        return Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape);
    auto nd = lattice_shape.ndim();
    IPosition blc(nd, std::numeric_limits<std::int64_t>::max());
    IPosition trc(nd, 0);
    for (auto& r : regions) {
        for (std::size_t d = 0; d < nd; ++d) {
            blc[d] = std::min(blc[d], r->bounding_box().start()[d]);
            auto r_trc = r->bounding_box().start()[d] + r->bounding_box().length()[d] - 1;
            trc[d] = std::max(trc[d], r_trc);
        }
    }
    IPosition len(nd);
    for (std::size_t d = 0; d < nd; ++d) {
        len[d] = trc[d] - blc[d] + 1;
    }
    return Slicer(std::move(blc), std::move(len));
}

static Slicer compute_intersection_bb(
    const std::vector<std::unique_ptr<LcRegion>>& regions,
    const IPosition& lattice_shape) {
    if (regions.empty())
        return Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape);
    auto nd = lattice_shape.ndim();
    IPosition blc(nd, 0);
    IPosition trc(nd);
    for (std::size_t d = 0; d < nd; ++d) {
        trc[d] = lattice_shape[d] - 1;
    }
    for (auto& r : regions) {
        for (std::size_t d = 0; d < nd; ++d) {
            blc[d] = std::max(blc[d], r->bounding_box().start()[d]);
            auto r_trc = r->bounding_box().start()[d] + r->bounding_box().length()[d] - 1;
            trc[d] = std::min(trc[d], r_trc);
        }
    }
    IPosition len(nd);
    for (std::size_t d = 0; d < nd; ++d) {
        len[d] = std::max(std::int64_t{0}, trc[d] - blc[d] + 1);
    }
    return Slicer(std::move(blc), std::move(len));
}

// ── Helper: is pixel in region's mask? ───────────────────────────────

static bool pixel_in_region(const LcRegion& reg, const IPosition& pos) {
    // Check if pos is within region's bounding box.
    auto nd = pos.ndim();
    for (std::size_t d = 0; d < nd; ++d) {
        if (pos[d] < reg.bounding_box().start()[d] ||
            pos[d] >= reg.bounding_box().start()[d] + reg.bounding_box().length()[d]) {
            return false;
        }
    }
    if (!reg.has_mask()) return true;
    // Convert to local coordinates within bounding box.
    IPosition local(nd);
    for (std::size_t d = 0; d < nd; ++d) {
        local[d] = pos[d] - reg.bounding_box().start()[d];
    }
    auto mask = reg.get_mask();
    return mask.at(local);
}

// ── LcUnion ──────────────────────────────────────────────────────────

LcUnion::LcUnion(std::vector<std::unique_ptr<LcRegion>> regions,
                 IPosition lattice_shape)
    : LcRegion(lattice_shape, compute_union_bb(regions, lattice_shape)),
      regions_(std::move(regions)) {}

std::unique_ptr<LcRegion> LcUnion::clone() const {
    std::vector<std::unique_ptr<LcRegion>> clones;
    for (auto& r : regions_) clones.push_back(r->clone());
    return std::make_unique<LcUnion>(std::move(clones), lattice_shape());
}

Record LcUnion::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcUnion")));
    return rec;
}

LatticeArray<bool> LcUnion::get_mask() const {
    auto sh = shape();
    LatticeArray<bool> mask(sh, false);
    mask.make_unique();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        // Convert to lattice coordinates.
        IPosition lpos(pos.ndim());
        for (std::size_t d = 0; d < pos.ndim(); ++d) {
            lpos[d] = bounding_box().start()[d] + pos[d];
        }
        for (auto& r : regions_) {
            if (pixel_in_region(*r, lpos)) {
                mask.put(delinearize(static_cast<std::int64_t>(idx), sh), true);
                break;
            }
        }
    }
    return mask;
}

// ── LcIntersection ───────────────────────────────────────────────────

LcIntersection::LcIntersection(
    std::vector<std::unique_ptr<LcRegion>> regions, IPosition lattice_shape)
    : LcRegion(lattice_shape,
               compute_intersection_bb(regions, lattice_shape)),
      regions_(std::move(regions)) {}

std::unique_ptr<LcRegion> LcIntersection::clone() const {
    std::vector<std::unique_ptr<LcRegion>> clones;
    for (auto& r : regions_) clones.push_back(r->clone());
    return std::make_unique<LcIntersection>(std::move(clones), lattice_shape());
}

Record LcIntersection::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcIntersection")));
    return rec;
}

LatticeArray<bool> LcIntersection::get_mask() const {
    auto sh = shape();
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        IPosition lpos(pos.ndim());
        for (std::size_t d = 0; d < pos.ndim(); ++d) {
            lpos[d] = bounding_box().start()[d] + pos[d];
        }
        for (auto& r : regions_) {
            if (!pixel_in_region(*r, lpos)) {
                mask.put(delinearize(static_cast<std::int64_t>(idx), sh), false);
                break;
            }
        }
    }
    return mask;
}

// ── LcDifference ─────────────────────────────────────────────────────

LcDifference::LcDifference(std::unique_ptr<LcRegion> region1,
                           std::unique_ptr<LcRegion> region2,
                           IPosition lattice_shape)
    : LcRegion(lattice_shape, region1->bounding_box()),
      region1_(std::move(region1)), region2_(std::move(region2)) {}

std::unique_ptr<LcRegion> LcDifference::clone() const {
    return std::make_unique<LcDifference>(
        region1_->clone(), region2_->clone(), lattice_shape());
}

Record LcDifference::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcDifference")));
    return rec;
}

LatticeArray<bool> LcDifference::get_mask() const {
    auto sh = shape();
    LatticeArray<bool> mask(sh, false);
    mask.make_unique();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        IPosition lpos(pos.ndim());
        for (std::size_t d = 0; d < pos.ndim(); ++d) {
            lpos[d] = bounding_box().start()[d] + pos[d];
        }
        if (pixel_in_region(*region1_, lpos) &&
            !pixel_in_region(*region2_, lpos)) {
            mask.put(delinearize(static_cast<std::int64_t>(idx), sh), true);
        }
    }
    return mask;
}

// ── LcComplement ─────────────────────────────────────────────────────

LcComplement::LcComplement(std::unique_ptr<LcRegion> region,
                           IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      region_(std::move(region)) {}

std::unique_ptr<LcRegion> LcComplement::clone() const {
    return std::make_unique<LcComplement>(region_->clone(), lattice_shape());
}

Record LcComplement::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcComplement")));
    return rec;
}

LatticeArray<bool> LcComplement::get_mask() const {
    auto sh = shape();
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        IPosition lpos(pos.ndim());
        for (std::size_t d = 0; d < pos.ndim(); ++d) {
            lpos[d] = bounding_box().start()[d] + pos[d];
        }
        if (pixel_in_region(*region_, lpos)) {
            mask.put(delinearize(static_cast<std::int64_t>(idx), sh), false);
        }
    }
    return mask;
}

// ── LcExtension ──────────────────────────────────────────────────────

LcExtension::LcExtension(std::unique_ptr<LcRegion> region,
                         std::vector<std::size_t> extend_axes,
                         IPosition extend_lengths,
                         IPosition lattice_shape)
    : LcRegion(lattice_shape,
               Slicer(IPosition(lattice_shape.ndim(), 0), lattice_shape)),
      region_(std::move(region)),
      extend_axes_(std::move(extend_axes)),
      extend_lengths_(std::move(extend_lengths)) {}

std::unique_ptr<LcRegion> LcExtension::clone() const {
    return std::make_unique<LcExtension>(
        region_->clone(), extend_axes_, extend_lengths_, lattice_shape());
}

Record LcExtension::to_record() const {
    Record rec;
    rec.set("type", RecordValue(std::string("LcExtension")));
    return rec;
}

bool LcExtension::has_mask() const {
    return region_->has_mask();
}

LatticeArray<bool> LcExtension::get_mask() const {
    // Full lattice mask: true where the inner region's mask is true,
    // replicated along extension axes.
    auto sh = lattice_shape();
    LatticeArray<bool> mask(sh, false);
    mask.make_unique();
    auto inner_mask = region_->get_mask();
    auto nel = mask.nelements();
    for (std::size_t idx = 0; idx < nel; ++idx) {
        auto pos = delinearize(static_cast<std::int64_t>(idx), sh);
        // Project to inner region's axes.
        if (pixel_in_region(*region_, pos)) {
            mask.put(delinearize(static_cast<std::int64_t>(idx), sh), true);
        }
    }
    return mask;
}

// ── LatticeRegion ────────────────────────────────────────────────────

LatticeRegion::LatticeRegion(std::unique_ptr<LcRegion> region)
    : region_(std::move(region)) {}

LatticeRegion::LatticeRegion(std::unique_ptr<LcRegion> region, Slicer slicer)
    : region_(std::move(region)),
      slicer_(std::move(slicer)),
      has_slicer_(true) {}

Record LatticeRegion::to_record() const {
    Record rec;
    if (region_) {
        rec.set("region", RecordValue::from_record(region_->to_record()));
        rec.set("region_type", RecordValue(region_->type()));
    }
    if (has_slicer_) {
        put_iposition(rec, "slicer_start", slicer_.start());
        put_iposition(rec, "slicer_length", slicer_.length());
        put_iposition(rec, "slicer_stride", slicer_.stride());
    }
    return rec;
}

LatticeRegion LatticeRegion::from_record(
    const Record& rec, const IPosition& lattice_shape) {
    LatticeRegion lr;
    if (auto* rv = rec.find("region")) {
        if (auto* rp = std::get_if<RecordValue::record_ptr>(&rv->storage())) {
            lr.region_ = LcRegion::from_record(**rp, lattice_shape);
        }
    }
    if (rec.find("slicer_start")) {
        auto start = get_iposition(rec, "slicer_start");
        auto length = get_iposition(rec, "slicer_length");
        auto stride = get_iposition(rec, "slicer_stride");
        lr.slicer_ = Slicer(std::move(start), std::move(length), std::move(stride));
        lr.has_slicer_ = true;
    }
    return lr;
}

} // namespace casacore_mini
