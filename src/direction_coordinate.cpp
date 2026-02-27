#include "casacore_mini/direction_coordinate.hpp"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <wcslib/wcs.h>
#include <wcslib/wcsfix.h>
}

namespace casacore_mini {

namespace {

constexpr double kRad2Deg = 180.0 / M_PI;
constexpr double kDeg2Rad = M_PI / 180.0;

// Build FITS CTYPE strings from reference frame and projection.
// Returns {ctype1, ctype2} e.g. {"RA---SIN", "DEC--SIN"}.
std::pair<std::string, std::string> make_ctypes(DirectionRef ref, const Projection& proj) {
    std::string proj_code = projection_type_to_string(proj.type);
    // Pad to 3 chars.
    while (proj_code.size() < 3) {
        proj_code += ' ';
    }

    std::string axis1;
    std::string axis2;
    switch (ref) {
    case DirectionRef::galactic:
        axis1 = "GLON";
        axis2 = "GLAT";
        break;
    case DirectionRef::ecliptic:
    case DirectionRef::mecliptic:
    case DirectionRef::tecliptic:
        axis1 = "ELON";
        axis2 = "ELAT";
        break;
    case DirectionRef::supergal:
        axis1 = "SLON";
        axis2 = "SLAT";
        break;
    default:
        // J2000, B1950, ICRS, etc. use RA/DEC.
        axis1 = "RA--";
        axis2 = "DEC-";
        break;
    }

    // CTYPE format: "XXXX-YYY" (8 chars: 4 axis + '-' + 3 projection)
    return {axis1 + "-" + proj_code, axis2 + "-" + proj_code};
}

} // namespace

struct DirectionCoordinate::WcsData {
    wcsprm wcs{};
    bool initialized = false;

    WcsData() = default;

    ~WcsData() {
        if (initialized) {
            wcsfree(&wcs);
        }
    }

    WcsData(const WcsData&) = delete;
    WcsData& operator=(const WcsData&) = delete;
    WcsData(WcsData&&) = delete;
    WcsData& operator=(WcsData&&) = delete;
};

void DirectionCoordinate::init_wcs() {
    wcs_data_ = std::make_unique<WcsData>();

    wcs_data_->wcs.flag = -1;
    int status = wcsini(1, 2, &wcs_data_->wcs);
    if (status != 0) {
        throw std::runtime_error("wcsini failed: " + std::to_string(status));
    }
    wcs_data_->initialized = true;

    auto [ctype1, ctype2] = make_ctypes(ref_, proj_);
    std::strncpy(wcs_data_->wcs.ctype[0], ctype1.c_str(), 72);
    std::strncpy(wcs_data_->wcs.ctype[1], ctype2.c_str(), 72);

    // WCSLIB uses 1-based pixels, we store 0-based.
    wcs_data_->wcs.crpix[0] = crpix_x_ + 1.0;
    wcs_data_->wcs.crpix[1] = crpix_y_ + 1.0;

    // WCSLIB uses degrees for celestial coordinates.
    wcs_data_->wcs.crval[0] = ref_lon_rad_ * kRad2Deg;
    wcs_data_->wcs.crval[1] = ref_lat_rad_ * kRad2Deg;
    wcs_data_->wcs.cdelt[0] = inc_lon_rad_ * kRad2Deg;
    wcs_data_->wcs.cdelt[1] = inc_lat_rad_ * kRad2Deg;

    // PC matrix.
    if (pc_.size() >= 4) {
        wcs_data_->wcs.pc[0] = pc_[0];
        wcs_data_->wcs.pc[1] = pc_[1];
        wcs_data_->wcs.pc[2] = pc_[2];
        wcs_data_->wcs.pc[3] = pc_[3];
    }
    // else identity (wcsini default)

    wcs_data_->wcs.lonpole = longpole_deg_;
    wcs_data_->wcs.latpole = latpole_deg_;

    // Set projection parameters.
    if (!proj_.parameters.empty()) {
        for (std::size_t i = 0; i < proj_.parameters.size() && i < 30; ++i) {
            wcs_data_->wcs.pv[static_cast<int>(i)].i = 2; // latitude axis
            wcs_data_->wcs.pv[static_cast<int>(i)].m = static_cast<int>(i);
            wcs_data_->wcs.pv[static_cast<int>(i)].value = proj_.parameters[i];
        }
        wcs_data_->wcs.npv = static_cast<int>(proj_.parameters.size());
    }

    status = wcsset(&wcs_data_->wcs);
    if (status != 0) {
        throw std::runtime_error("wcsset failed: " + std::string(wcs_errmsg[status]));
    }
}

DirectionCoordinate::DirectionCoordinate(DirectionRef ref, Projection proj, double ref_lon_rad,
                                         double ref_lat_rad, double inc_lon_rad, double inc_lat_rad,
                                         std::vector<double> pc, double crpix_x, double crpix_y,
                                         double longpole_deg, double latpole_deg)
    : ref_(ref), proj_(std::move(proj)), ref_lon_rad_(ref_lon_rad), ref_lat_rad_(ref_lat_rad),
      inc_lon_rad_(inc_lon_rad), inc_lat_rad_(inc_lat_rad), pc_(std::move(pc)), crpix_x_(crpix_x),
      crpix_y_(crpix_y), longpole_deg_(longpole_deg), latpole_deg_(latpole_deg) {
    init_wcs();
}

DirectionCoordinate::~DirectionCoordinate() = default;

DirectionCoordinate::DirectionCoordinate(DirectionCoordinate&& other) noexcept = default;
DirectionCoordinate& DirectionCoordinate::operator=(DirectionCoordinate&& other) noexcept = default;

DirectionCoordinate::DirectionCoordinate(const DirectionCoordinate& other)
    : Coordinate(other), ref_(other.ref_), proj_(other.proj_), ref_lon_rad_(other.ref_lon_rad_),
      ref_lat_rad_(other.ref_lat_rad_), inc_lon_rad_(other.inc_lon_rad_),
      inc_lat_rad_(other.inc_lat_rad_), pc_(other.pc_), crpix_x_(other.crpix_x_),
      crpix_y_(other.crpix_y_), longpole_deg_(other.longpole_deg_),
      latpole_deg_(other.latpole_deg_) {
    init_wcs();
}

DirectionCoordinate& DirectionCoordinate::operator=(const DirectionCoordinate& other) {
    if (this != &other) {
        ref_ = other.ref_;
        proj_ = other.proj_;
        ref_lon_rad_ = other.ref_lon_rad_;
        ref_lat_rad_ = other.ref_lat_rad_;
        inc_lon_rad_ = other.inc_lon_rad_;
        inc_lat_rad_ = other.inc_lat_rad_;
        pc_ = other.pc_;
        crpix_x_ = other.crpix_x_;
        crpix_y_ = other.crpix_y_;
        longpole_deg_ = other.longpole_deg_;
        latpole_deg_ = other.latpole_deg_;
        init_wcs();
    }
    return *this;
}

std::vector<std::string> DirectionCoordinate::world_axis_names() const {
    switch (ref_) {
    case DirectionRef::galactic:
        return {"Galactic Longitude", "Galactic Latitude"};
    case DirectionRef::ecliptic:
    case DirectionRef::mecliptic:
    case DirectionRef::tecliptic:
        return {"Ecliptic Longitude", "Ecliptic Latitude"};
    default:
        return {"Right Ascension", "Declination"};
    }
}

std::vector<std::string> DirectionCoordinate::world_axis_units() const {
    return {"rad", "rad"};
}

std::vector<double> DirectionCoordinate::reference_value() const {
    return {ref_lon_rad_, ref_lat_rad_};
}

std::vector<double> DirectionCoordinate::reference_pixel() const {
    return {crpix_x_, crpix_y_};
}

std::vector<double> DirectionCoordinate::increment() const {
    return {inc_lon_rad_, inc_lat_rad_};
}

std::vector<double> DirectionCoordinate::to_world(const std::vector<double>& pixel) const {
    if (pixel.size() != 2) {
        throw std::invalid_argument("DirectionCoordinate::to_world: expected 2 pixel axes");
    }

    // Convert to WCSLIB 1-based pixels.
    double pixcrd[2] = {pixel[0] + 1.0, pixel[1] + 1.0};
    double imgcrd[2] = {};
    double phi = 0.0;
    double theta = 0.0;
    double world[2] = {};
    int stat = 0;

    int status = wcsp2s(&wcs_data_->wcs, 1, 2, pixcrd, imgcrd, &phi, &theta, world, &stat);
    if (status != 0 || stat != 0) {
        throw std::runtime_error("wcsp2s failed: " + std::string(wcs_errmsg[status]));
    }

    // WCSLIB returns degrees; convert to radians.
    return {world[0] * kDeg2Rad, world[1] * kDeg2Rad};
}

std::vector<double> DirectionCoordinate::to_pixel(const std::vector<double>& world) const {
    if (world.size() != 2) {
        throw std::invalid_argument("DirectionCoordinate::to_pixel: expected 2 world axes");
    }

    // WCSLIB expects degrees.
    double worldcrd[2] = {world[0] * kRad2Deg, world[1] * kRad2Deg};
    double imgcrd[2] = {};
    double phi = 0.0;
    double theta = 0.0;
    double pixcrd[2] = {};
    int stat = 0;

    int status = wcss2p(&wcs_data_->wcs, 1, 2, worldcrd, &phi, &theta, imgcrd, pixcrd, &stat);
    if (status != 0 || stat != 0) {
        throw std::runtime_error("wcss2p failed: " + std::string(wcs_errmsg[status]));
    }

    // Convert from WCSLIB 1-based to 0-based.
    return {pixcrd[0] - 1.0, pixcrd[1] - 1.0};
}

Record DirectionCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("direction")));
    rec.set("system", RecordValue(std::string(direction_ref_to_string(ref_))));
    rec.set("projection", RecordValue(projection_type_to_string(proj_.type)));
    // casacore requires projection_parameters even when empty.
    rec.set("projection_parameters",
            RecordValue(RecordValue::double_array{
                {static_cast<std::uint64_t>(proj_.parameters.size())}, proj_.parameters}));
    rec.set("crval", RecordValue(RecordValue::double_array{{2}, {ref_lon_rad_, ref_lat_rad_}}));
    rec.set("crpix", RecordValue(RecordValue::double_array{{2}, {crpix_x_, crpix_y_}}));
    rec.set("cdelt", RecordValue(RecordValue::double_array{{2}, {inc_lon_rad_, inc_lat_rad_}}));
    // PC matrix — casacore requires 2D (2×2).
    {
        std::vector<double> pc_data;
        if (pc_.size() >= 4) {
            pc_data = pc_;
        } else {
            pc_data = {1.0, 0.0, 0.0, 1.0};
        }
        rec.set("pc", RecordValue(RecordValue::double_array{{2, 2}, std::move(pc_data)}));
    }
    rec.set("axes", RecordValue(RecordValue::string_array{{2}, world_axis_names()}));
    rec.set("units", RecordValue(RecordValue::string_array{{2}, {"rad", "rad"}}));
    rec.set("longpole", RecordValue(longpole_deg_));
    rec.set("latpole", RecordValue(latpole_deg_));
    return rec;
}

std::unique_ptr<Coordinate> DirectionCoordinate::clone() const {
    return std::make_unique<DirectionCoordinate>(*this);
}

std::unique_ptr<DirectionCoordinate> DirectionCoordinate::from_record(const Record& rec) {
    DirectionRef ref = DirectionRef::j2000;
    if (const auto* v = rec.find("system")) {
        if (const auto* sp = std::get_if<std::string>(&v->storage())) {
            ref = string_to_direction_ref(*sp);
        }
    }

    Projection proj;
    if (const auto* v = rec.find("projection")) {
        if (const auto* sp = std::get_if<std::string>(&v->storage())) {
            proj.type = string_to_projection_type(*sp);
        }
    }
    if (const auto* v = rec.find("projection_parameters")) {
        if (const auto* a = std::get_if<RecordValue::double_array>(&v->storage())) {
            proj.parameters = a->elements;
        }
    }

    auto get_doubles = [&](const char* key) -> std::vector<double> {
        const auto* v = rec.find(key);
        if (v == nullptr) {
            return {};
        }
        if (const auto* a = std::get_if<RecordValue::double_array>(&v->storage())) {
            return a->elements;
        }
        return {};
    };

    auto get_double = [&](const char* key, double dflt) -> double {
        const auto* v = rec.find(key);
        if (v == nullptr) {
            return dflt;
        }
        if (const auto* dp = std::get_if<double>(&v->storage())) {
            return *dp;
        }
        return dflt;
    };

    auto crval = get_doubles("crval");
    auto crpix = get_doubles("crpix");
    auto cdelt = get_doubles("cdelt");
    auto pc = get_doubles("pc");

    double ref_lon = crval.size() >= 1 ? crval[0] : 0.0;
    double ref_lat = crval.size() >= 2 ? crval[1] : 0.0;
    double inc_lon = cdelt.size() >= 1 ? cdelt[0] : 0.0;
    double inc_lat = cdelt.size() >= 2 ? cdelt[1] : 0.0;
    double crpix_x = crpix.size() >= 1 ? crpix[0] : 0.0;
    double crpix_y = crpix.size() >= 2 ? crpix[1] : 0.0;

    return std::make_unique<DirectionCoordinate>(
        ref, std::move(proj), ref_lon, ref_lat, inc_lon, inc_lat, std::move(pc), crpix_x, crpix_y,
        get_double("longpole", 180.0), get_double("latpole", 0.0));
}

} // namespace casacore_mini
