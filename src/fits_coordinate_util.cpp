#include "casacore_mini/fits_coordinate_util.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/linear_coordinate.hpp"
#include "casacore_mini/linear_xform.hpp"
#include "casacore_mini/projection.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace casacore_mini {

namespace {

constexpr double kDeg2Rad = M_PI / 180.0;
constexpr double kRad2Deg = 180.0 / M_PI;

const std::string* find_string(const Record& rec, const std::string& key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    return std::get_if<std::string>(&val->storage());
}

double find_double(const Record& rec, const std::string& key, double dflt) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return dflt;
    }
    if (const auto* dp = std::get_if<double>(&val->storage())) {
        return *dp;
    }
    if (const auto* fp = std::get_if<float>(&val->storage())) {
        return static_cast<double>(*fp);
    }
    return dflt;
}

std::int32_t find_int(const Record& rec, const std::string& key, std::int32_t dflt) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return dflt;
    }
    if (const auto* ip = std::get_if<std::int32_t>(&val->storage())) {
        return *ip;
    }
    return dflt;
}

// Check if CTYPE represents a celestial longitude axis.
bool is_lon_axis(const std::string& ctype) {
    return ctype.substr(0, 4) == "RA--" || ctype.substr(0, 4) == "GLON" ||
           ctype.substr(0, 4) == "ELON" || ctype.substr(0, 4) == "SLON";
}

bool is_lat_axis(const std::string& ctype) {
    return ctype.substr(0, 3) == "DEC" || ctype.substr(0, 4) == "GLAT" ||
           ctype.substr(0, 4) == "ELAT" || ctype.substr(0, 4) == "SLAT";
}

// Extract projection code from CTYPE (last 3 characters after '-').
std::string extract_projection(const std::string& ctype) {
    auto pos = ctype.rfind('-');
    if (pos != std::string::npos && pos + 1 < ctype.size()) {
        std::string code = ctype.substr(pos + 1);
        // Strip trailing whitespace.
        while (!code.empty() && (code.back() == ' ' || code.back() == '-')) {
            code.pop_back();
        }
        if (code.size() == 3) {
            return code;
        }
    }
    return "SIN"; // default
}

DirectionRef ref_from_ctype(const std::string& ctype) {
    if (ctype.substr(0, 4) == "GLON" || ctype.substr(0, 4) == "GLAT") {
        return DirectionRef::galactic;
    }
    if (ctype.substr(0, 4) == "ELON" || ctype.substr(0, 4) == "ELAT") {
        return DirectionRef::ecliptic;
    }
    return DirectionRef::j2000;
}

} // namespace

CoordinateSystem coordinate_system_from_fits_header(const Record& fits_keywords) {
    CoordinateSystem cs;

    // Determine number of axes from NAXIS or by scanning CTYPE keys.
    int naxis = find_int(fits_keywords, "NAXIS", 0);
    if (naxis <= 0) {
        // Try counting CTYPE keys.
        for (int i = 1; i <= 10; ++i) {
            if (find_string(fits_keywords, "CTYPE" + std::to_string(i)) != nullptr) {
                naxis = i;
            }
        }
    }

    // Collect axis info.
    struct AxisInfo {
        std::string ctype;
        double crval = 0.0;
        double crpix = 0.0;
        double cdelt = 1.0;
        bool used = false;
    };
    std::vector<AxisInfo> axes(static_cast<std::size_t>(naxis));

    for (int i = 0; i < naxis; ++i) {
        std::string idx = std::to_string(i + 1);
        const auto* ct = find_string(fits_keywords, "CTYPE" + idx);
        if (ct != nullptr) {
            axes[static_cast<std::size_t>(i)].ctype = *ct;
        }
        axes[static_cast<std::size_t>(i)].crval = find_double(fits_keywords, "CRVAL" + idx, 0.0);
        axes[static_cast<std::size_t>(i)].crpix =
            find_double(fits_keywords, "CRPIX" + idx, 1.0) - 1.0; // FITS 1-based to 0-based
        axes[static_cast<std::size_t>(i)].cdelt = find_double(fits_keywords, "CDELT" + idx, 1.0);
    }

    // Find and create direction coordinate (paired lon/lat axes).
    int lon_idx = -1;
    int lat_idx = -1;
    for (int i = 0; i < naxis; ++i) {
        if (is_lon_axis(axes[static_cast<std::size_t>(i)].ctype)) {
            lon_idx = i;
        }
        if (is_lat_axis(axes[static_cast<std::size_t>(i)].ctype)) {
            lat_idx = i;
        }
    }

    if (lon_idx >= 0 && lat_idx >= 0) {
        auto& lon = axes[static_cast<std::size_t>(lon_idx)];
        auto& lat = axes[static_cast<std::size_t>(lat_idx)];
        std::string proj_code = extract_projection(lon.ctype);
        DirectionRef ref = ref_from_ctype(lon.ctype);

        cs.add_coordinate(std::make_unique<DirectionCoordinate>(
            ref, Projection{string_to_projection_type(proj_code), {}}, lon.crval * kDeg2Rad,
            lat.crval * kDeg2Rad, lon.cdelt * kDeg2Rad, lat.cdelt * kDeg2Rad, std::vector<double>{},
            lon.crpix, lat.crpix));

        lon.used = true;
        lat.used = true;
    }

    // Find spectral axis.
    for (int i = 0; i < naxis; ++i) {
        auto& ax = axes[static_cast<std::size_t>(i)];
        if (ax.used) {
            continue;
        }
        if (ax.ctype.substr(0, 4) == "FREQ" || ax.ctype.substr(0, 4) == "VELO") {
            cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::topo, ax.crval,
                                                                   ax.cdelt, ax.crpix));
            ax.used = true;
        }
    }

    // Find Stokes axis.
    for (int i = 0; i < naxis; ++i) {
        auto& ax = axes[static_cast<std::size_t>(i)];
        if (ax.used) {
            continue;
        }
        if (ax.ctype.substr(0, 6) == "STOKES" || ax.ctype == "STOKES") {
            // Read NAXIS for the Stokes dimension.
            int nstokes = find_int(fits_keywords, "NAXIS" + std::to_string(i + 1), 1);
            std::vector<std::int32_t> stokes_vals;
            if (nstokes > 0) {
                stokes_vals.reserve(static_cast<std::size_t>(nstokes));
            }
            for (int s = 0; s < nstokes; ++s) {
                stokes_vals.push_back(
                    static_cast<std::int32_t>(ax.crval + ax.cdelt * (s - ax.crpix)));
            }
            if (stokes_vals.empty()) {
                stokes_vals.push_back(static_cast<std::int32_t>(ax.crval));
            }
            cs.add_coordinate(std::make_unique<StokesCoordinate>(std::move(stokes_vals)));
            ax.used = true;
        }
    }

    // Remaining axes become linear coordinates (one per axis).
    for (int i = 0; i < naxis; ++i) {
        auto& ax = axes[static_cast<std::size_t>(i)];
        if (ax.used) {
            continue;
        }
        LinearXform xf;
        xf.crpix = {ax.crpix};
        xf.cdelt = {ax.cdelt};
        xf.crval = {ax.crval};
        std::string name = ax.ctype.empty() ? ("Axis" + std::to_string(i + 1)) : ax.ctype;
        cs.add_coordinate(std::make_unique<LinearCoordinate>(
            std::vector<std::string>{name}, std::vector<std::string>{""}, std::move(xf)));
    }

    return cs;
}

Record coordinate_system_to_fits_header(const CoordinateSystem& cs) {
    Record rec;
    rec.set("NAXIS", RecordValue(static_cast<std::int32_t>(cs.n_pixel_axes())));

    int axis_num = 1;
    for (std::size_t ci = 0; ci < cs.n_coordinates(); ++ci) {
        const auto& coord = cs.coordinate(ci);

        if (coord.type() == CoordinateType::direction) {
            const auto& dc = static_cast<const DirectionCoordinate&>(coord);
            auto rv = dc.reference_value();
            auto rp = dc.reference_pixel();
            auto inc = dc.increment();

            // Determine CTYPE prefix.
            std::string lon_prefix = "RA--";
            std::string lat_prefix = "DEC-";
            if (dc.ref_frame() == DirectionRef::galactic) {
                lon_prefix = "GLON";
                lat_prefix = "GLAT";
            } else if (dc.ref_frame() == DirectionRef::ecliptic) {
                lon_prefix = "ELON";
                lat_prefix = "ELAT";
            }
            std::string proj_code = projection_type_to_string(dc.projection().type);
            std::string lon_ctype = lon_prefix;
            lon_ctype.push_back('-');
            lon_ctype.append(proj_code);
            std::string lat_ctype = lat_prefix;
            lat_ctype.push_back('-');
            lat_ctype.append(proj_code);

            std::string idx1 = std::to_string(axis_num);
            std::string idx2 = std::to_string(axis_num + 1);
            rec.set("CTYPE" + idx1, RecordValue(std::move(lon_ctype)));
            rec.set("CTYPE" + idx2, RecordValue(std::move(lat_ctype)));
            rec.set("CRVAL" + idx1, RecordValue(rv[0] * kRad2Deg));
            rec.set("CRVAL" + idx2, RecordValue(rv[1] * kRad2Deg));
            rec.set("CRPIX" + idx1, RecordValue(rp[0] + 1.0)); // 0-based to FITS 1-based
            rec.set("CRPIX" + idx2, RecordValue(rp[1] + 1.0));
            rec.set("CDELT" + idx1, RecordValue(inc[0] * kRad2Deg));
            rec.set("CDELT" + idx2, RecordValue(inc[1] * kRad2Deg));
            axis_num += 2;
        } else if (coord.type() == CoordinateType::spectral) {
            std::string idx = std::to_string(axis_num);
            auto rv = coord.reference_value();
            auto rp = coord.reference_pixel();
            auto inc = coord.increment();
            rec.set("CTYPE" + idx, RecordValue(std::string("FREQ")));
            rec.set("CRVAL" + idx, RecordValue(rv[0]));
            rec.set("CRPIX" + idx, RecordValue(rp[0] + 1.0));
            rec.set("CDELT" + idx, RecordValue(inc[0]));
            ++axis_num;
        } else if (coord.type() == CoordinateType::stokes) {
            std::string idx = std::to_string(axis_num);
            auto rv = coord.reference_value();
            rec.set("CTYPE" + idx, RecordValue(std::string("STOKES")));
            rec.set("CRVAL" + idx, RecordValue(rv[0]));
            rec.set("CRPIX" + idx, RecordValue(1.0));
            rec.set("CDELT" + idx, RecordValue(1.0));
            ++axis_num;
        } else {
            // Linear/tabular/quality.
            for (std::size_t ai = 0; ai < coord.n_pixel_axes(); ++ai) {
                std::string idx = std::to_string(axis_num);
                auto names = coord.world_axis_names();
                auto rv = coord.reference_value();
                auto rp = coord.reference_pixel();
                auto inc = coord.increment();
                rec.set("CTYPE" + idx, RecordValue(ai < names.size() ? names[ai] : ""));
                rec.set("CRVAL" + idx, RecordValue(ai < rv.size() ? rv[ai] : 0.0));
                rec.set("CRPIX" + idx, RecordValue((ai < rp.size() ? rp[ai] : 0.0) + 1.0));
                rec.set("CDELT" + idx, RecordValue(ai < inc.size() ? inc[ai] : 1.0));
                ++axis_num;
            }
        }
    }

    return rec;
}

} // namespace casacore_mini
