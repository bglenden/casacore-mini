#include "casacore_mini/measure_convert.hpp"

#include "casacore_mini/erfa_bridge.hpp"
#include "casacore_mini/velocity_machine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

// MJD zero-point as JD: JD = 2400000.5 + MJD.
constexpr double kMjdJd0 = 2400000.5;

// Convert an EpochValue (MJD day + fraction) to a 2-part JD.
void epoch_to_jd(const EpochValue& ev, double& jd1, double& jd2) {
    jd1 = kMjdJd0;
    jd2 = ev.day + ev.fraction;
}

// Convert a 2-part JD back to EpochValue (split into integer MJD + fractional day).
EpochValue jd_to_epoch(double jd1, double jd2) {
    double mjd = (jd1 - kMjdJd0) + jd2;
    double day = std::floor(mjd);
    double frac = mjd - day;
    return EpochValue{day, frac};
}

// --- Epoch conversions -------------------------------------------------------

// Convert epoch between two reference frames.
// Supported: UTC, TAI, TDT(TT), TDB, UT1 (UT1 requires dut1 in frame).
EpochValue convert_epoch(const EpochValue& val, EpochRef from, EpochRef to,
                         const MeasureFrame& frame) {
    if (from == to) {
        return val;
    }

    double j1 = 0.0;
    double j2 = 0.0;
    epoch_to_jd(val, j1, j2);

    // Convert from source to UTC as intermediate.
    double utc1 = 0.0;
    double utc2 = 0.0;
    switch (from) {
    case EpochRef::utc:
        utc1 = j1;
        utc2 = j2;
        break;
    case EpochRef::tai: {
        tai_to_utc(j1, j2, utc1, utc2);
        break;
    }
    case EpochRef::tdt: {
        double tai1 = 0.0;
        double tai2 = 0.0;
        tt_to_tai(j1, j2, tai1, tai2);
        tai_to_utc(tai1, tai2, utc1, utc2);
        break;
    }
    case EpochRef::tdb: {
        double dtr = approx_dtdb(j1, j2);
        double tt1 = 0.0;
        double tt2 = 0.0;
        tdb_to_tt(j1, j2, dtr, tt1, tt2);
        double tai1 = 0.0;
        double tai2 = 0.0;
        tt_to_tai(tt1, tt2, tai1, tai2);
        tai_to_utc(tai1, tai2, utc1, utc2);
        break;
    }
    case EpochRef::ut1: {
        ut1_to_utc(j1, j2, frame.dut1, utc1, utc2);
        break;
    }
    default:
        throw std::invalid_argument("Unsupported source epoch reference: " +
                                    std::string(epoch_ref_to_string(from)));
    }

    // Convert from UTC to target.
    double out1 = 0.0;
    double out2 = 0.0;
    switch (to) {
    case EpochRef::utc:
        out1 = utc1;
        out2 = utc2;
        break;
    case EpochRef::tai:
        utc_to_tai(utc1, utc2, out1, out2);
        break;
    case EpochRef::tdt: {
        double tai1 = 0.0;
        double tai2 = 0.0;
        utc_to_tai(utc1, utc2, tai1, tai2);
        tai_to_tt(tai1, tai2, out1, out2);
        break;
    }
    case EpochRef::tdb: {
        double tai1 = 0.0;
        double tai2 = 0.0;
        utc_to_tai(utc1, utc2, tai1, tai2);
        double tt1 = 0.0;
        double tt2 = 0.0;
        tai_to_tt(tai1, tai2, tt1, tt2);
        double dtr = approx_dtdb(tt1, tt2);
        tt_to_tdb(tt1, tt2, dtr, out1, out2);
        break;
    }
    case EpochRef::ut1:
        utc_to_ut1(utc1, utc2, frame.dut1, out1, out2);
        break;
    default:
        throw std::invalid_argument("Unsupported target epoch reference: " +
                                    std::string(epoch_ref_to_string(to)));
    }

    return jd_to_epoch(out1, out2);
}

// --- Position conversions ----------------------------------------------------

PositionValue convert_position(const PositionValue& val, PositionRef from, PositionRef to) {
    if (from == to) {
        return val;
    }

    if (from == PositionRef::wgs84 && to == PositionRef::itrf) {
        // WGS84 stores as (lon_rad, lat_rad, height_m) in the XYZ fields.
        double xyz[3]; // NOLINT
        wgs84_to_itrf(val.x_m, val.y_m, val.z_m, xyz);
        return PositionValue{xyz[0], xyz[1], xyz[2]};
    }

    if (from == PositionRef::itrf && to == PositionRef::wgs84) {
        double xyz[3] = {val.x_m, val.y_m, val.z_m};
        double elong = 0.0;
        double phi = 0.0;
        double height = 0.0;
        itrf_to_wgs84(xyz, elong, phi, height);
        return PositionValue{elong, phi, height};
    }

    throw std::invalid_argument("Unsupported position conversion");
}

// --- Direction conversions ---------------------------------------------------

DirectionValue convert_direction(const DirectionValue& val, DirectionRef from, DirectionRef to,
                                 const MeasureFrame& frame) {
    if (from == to) {
        return val;
    }

    // Convert from source to J2000 as intermediate.
    double ra_j2000 = 0.0;
    double dec_j2000 = 0.0;
    // B1950/FK4 conversions need a Besselian epoch; default to 1950.0.
    double bepoch = 1950.0;
    if ((from == DirectionRef::b1950 || from == DirectionRef::b1950_vla ||
         to == DirectionRef::b1950 || to == DirectionRef::b1950_vla) &&
        frame.epoch.has_value()) {
        const auto& ev = std::get<EpochValue>(frame.epoch->value);
        bepoch = jd_to_besselian_epoch(kMjdJd0, ev.day + ev.fraction);
    }
    switch (from) {
    case DirectionRef::j2000:
    case DirectionRef::icrs: // ICRS ≈ J2000 for our purposes
        ra_j2000 = val.lon_rad;
        dec_j2000 = val.lat_rad;
        break;
    case DirectionRef::b1950:
    case DirectionRef::b1950_vla:
        b1950_to_j2000(val.lon_rad, val.lat_rad, bepoch, ra_j2000, dec_j2000);
        break;
    case DirectionRef::galactic:
        galactic_to_j2000(val.lon_rad, val.lat_rad, ra_j2000, dec_j2000);
        break;
    case DirectionRef::ecliptic:
        ecliptic_to_j2000(val.lon_rad, val.lat_rad, ra_j2000, dec_j2000);
        break;
    case DirectionRef::app: {
        if (!frame.epoch.has_value()) {
            throw std::invalid_argument("APP→J2000 conversion requires epoch in MeasureFrame");
        }
        const auto& ev = std::get<EpochValue>(frame.epoch->value);
        double jd1 = kMjdJd0;
        double jd2 = ev.day + ev.fraction;
        // Convert epoch to TT for ERFA.
        double tai1 = 0.0;
        double tai2 = 0.0;
        utc_to_tai(jd1, jd2, tai1, tai2);
        double tt1 = 0.0;
        double tt2 = 0.0;
        tai_to_tt(tai1, tai2, tt1, tt2);
        std::array<std::array<double, 3>, 3> rbpn{};
        bias_precession_nutation_matrix(tt1, tt2, rbpn);
        rotate_direction_inverse(rbpn, val.lon_rad, val.lat_rad, ra_j2000, dec_j2000);
        break;
    }
    default:
        throw std::invalid_argument("Unsupported source direction reference: " +
                                    std::string(direction_ref_to_string(from)));
    }

    // Convert from J2000 to target.
    double lon_out = 0.0;
    double lat_out = 0.0;
    switch (to) {
    case DirectionRef::j2000:
    case DirectionRef::icrs:
        lon_out = ra_j2000;
        lat_out = dec_j2000;
        break;
    case DirectionRef::b1950:
    case DirectionRef::b1950_vla:
        j2000_to_b1950(ra_j2000, dec_j2000, bepoch, lon_out, lat_out);
        break;
    case DirectionRef::galactic:
        j2000_to_galactic(ra_j2000, dec_j2000, lon_out, lat_out);
        break;
    case DirectionRef::ecliptic:
        j2000_to_ecliptic(ra_j2000, dec_j2000, lon_out, lat_out);
        break;
    case DirectionRef::app: {
        if (!frame.epoch.has_value()) {
            throw std::invalid_argument("J2000→APP conversion requires epoch in MeasureFrame");
        }
        const auto& ev = std::get<EpochValue>(frame.epoch->value);
        double jd1 = kMjdJd0;
        double jd2 = ev.day + ev.fraction;
        double tai1 = 0.0;
        double tai2 = 0.0;
        utc_to_tai(jd1, jd2, tai1, tai2);
        double tt1 = 0.0;
        double tt2 = 0.0;
        tai_to_tt(tai1, tai2, tt1, tt2);
        std::array<std::array<double, 3>, 3> rbpn{};
        bias_precession_nutation_matrix(tt1, tt2, rbpn);
        rotate_direction(rbpn, ra_j2000, dec_j2000, lon_out, lat_out);
        break;
    }
    default:
        throw std::invalid_argument("Unsupported target direction reference: " +
                                    std::string(direction_ref_to_string(to)));
    }

    return DirectionValue{lon_out, lat_out};
}

// --- Doppler conversions (purely mathematical, no frame needed) ---------------

// Convert a Doppler value to a common intermediate (radio convention) then to target.
DopplerValue convert_doppler(const DopplerValue& val, DopplerRef from, DopplerRef to) {
    if (from == to) {
        return val;
    }

    // Convert source to radio as intermediate.
    double radio = 0.0;
    switch (from) {
    case DopplerRef::radio:
        radio = val.ratio;
        break;
    case DopplerRef::z:
        radio = z_to_radio(val.ratio);
        break;
    case DopplerRef::beta:
        radio = beta_to_radio(val.ratio);
        break;
    case DopplerRef::ratio:
        // ratio = f/f0, radio = 1 - f/f0
        radio = 1.0 - val.ratio;
        break;
    case DopplerRef::gamma: {
        // gamma = 1/sqrt(1 - beta^2), so beta = sqrt(1 - 1/gamma^2)
        double beta = std::sqrt(1.0 - 1.0 / (val.ratio * val.ratio));
        radio = beta_to_radio(beta);
        break;
    }
    }

    // Convert radio to target.
    switch (to) {
    case DopplerRef::radio:
        return DopplerValue{radio};
    case DopplerRef::z:
        return DopplerValue{radio_to_z(radio)};
    case DopplerRef::beta:
        return DopplerValue{radio_to_beta(radio)};
    case DopplerRef::ratio:
        return DopplerValue{1.0 - radio};
    case DopplerRef::gamma: {
        double beta = radio_to_beta(radio);
        return DopplerValue{1.0 / std::sqrt(1.0 - beta * beta)};
    }
    }

    return val; // unreachable
}

// --- Frequency conversions (identity only; cross-frame requires observer) ---

FrequencyValue convert_frequency(const FrequencyValue& val, FrequencyRef from, FrequencyRef to) {
    if (from == to) {
        return val;
    }
    throw std::invalid_argument(
        "Cross-frame frequency conversion (" + std::string(frequency_ref_to_string(from)) + " -> " +
        std::string(frequency_ref_to_string(to)) +
        ") requires observer velocity correction (position + epoch context); "
        "this is a Phase 9 carry-forward item");
}

// --- Radial velocity conversions (identity only; cross-frame requires observer) ---

RadialVelocityValue convert_radial_velocity(const RadialVelocityValue& val, RadialVelocityRef from,
                                            RadialVelocityRef to) {
    if (from == to) {
        return val;
    }
    throw std::invalid_argument(
        "Cross-frame radial velocity conversion (" +
        std::string(radial_velocity_ref_to_string(from)) + " -> " +
        std::string(radial_velocity_ref_to_string(to)) +
        ") requires observer velocity correction (position + epoch context); "
        "this is a Phase 9 carry-forward item");
}

// --- Helpers for XYZ frame rotations (baseline, uvw, earth_magnetic) ----------

// Map a BaselineRef to the corresponding DirectionRef for supported frames.
DirectionRef baseline_ref_to_direction(BaselineRef r) {
    switch (r) {
    case BaselineRef::j2000:
        return DirectionRef::j2000;
    case BaselineRef::icrs:
        return DirectionRef::icrs;
    case BaselineRef::galactic:
        return DirectionRef::galactic;
    case BaselineRef::ecliptic:
        return DirectionRef::ecliptic;
    case BaselineRef::app:
        return DirectionRef::app;
    default:
        throw std::invalid_argument("Unsupported baseline reference for conversion: " +
                                    std::string(baseline_ref_to_string(r)));
    }
}

DirectionRef uvw_ref_to_direction(UvwRef r) {
    switch (r) {
    case UvwRef::j2000:
        return DirectionRef::j2000;
    case UvwRef::icrs:
        return DirectionRef::icrs;
    case UvwRef::galactic:
        return DirectionRef::galactic;
    case UvwRef::ecliptic:
        return DirectionRef::ecliptic;
    case UvwRef::app:
        return DirectionRef::app;
    default:
        throw std::invalid_argument("Unsupported UVW reference for conversion: " +
                                    std::string(uvw_ref_to_string(r)));
    }
}

DirectionRef earth_magnetic_ref_to_direction(EarthMagneticRef r) {
    if (r == EarthMagneticRef::igrf) {
        throw std::runtime_error("IGRF not implemented");
    }
    switch (r) {
    case EarthMagneticRef::j2000:
        return DirectionRef::j2000;
    case EarthMagneticRef::icrs:
        return DirectionRef::icrs;
    case EarthMagneticRef::galactic:
        return DirectionRef::galactic;
    case EarthMagneticRef::ecliptic:
        return DirectionRef::ecliptic;
    case EarthMagneticRef::app:
        return DirectionRef::app;
    default:
        throw std::invalid_argument("Unsupported earth magnetic reference for conversion: " +
                                    std::string(earth_magnetic_ref_to_string(r)));
    }
}

// Convert an XYZ vector between direction frames via spherical round-trip.
// Works for Baseline, UVW, and EarthMagnetic values.
void convert_xyz_via_direction(double x_in, double y_in, double z_in, DirectionRef from_dir,
                               DirectionRef to_dir, const MeasureFrame& frame, double& x_out,
                               double& y_out, double& z_out) {
    if (from_dir == to_dir) {
        x_out = x_in;
        y_out = y_in;
        z_out = z_in;
        return;
    }

    double r = std::sqrt(x_in * x_in + y_in * y_in + z_in * z_in);
    if (r < 1.0e-30) {
        // Near-zero vector: return identity.
        x_out = x_in;
        y_out = y_in;
        z_out = z_in;
        return;
    }

    // XYZ → spherical (lon, lat).
    double lon = std::atan2(y_in, x_in);
    double lat = std::asin(std::clamp(z_in / r, -1.0, 1.0));

    // Convert direction.
    DirectionValue dir_in{lon, lat};
    auto dir_out = convert_direction(dir_in, from_dir, to_dir, frame);

    // Spherical → XYZ preserving r.
    double cd = std::cos(dir_out.lat_rad);
    x_out = r * std::cos(dir_out.lon_rad) * cd;
    y_out = r * std::sin(dir_out.lon_rad) * cd;
    z_out = r * std::sin(dir_out.lat_rad);
}

// --- Baseline conversions ---------------------------------------------------

BaselineValue convert_baseline(const BaselineValue& val, BaselineRef from, BaselineRef to,
                               const MeasureFrame& frame) {
    if (from == to) {
        return val;
    }
    auto from_dir = baseline_ref_to_direction(from);
    auto to_dir = baseline_ref_to_direction(to);
    double xo = 0.0;
    double yo = 0.0;
    double zo = 0.0;
    convert_xyz_via_direction(val.x_m, val.y_m, val.z_m, from_dir, to_dir, frame, xo, yo, zo);
    return BaselineValue{xo, yo, zo};
}

// --- UVW conversions --------------------------------------------------------

UvwValue convert_uvw(const UvwValue& val, UvwRef from, UvwRef to, const MeasureFrame& frame) {
    if (from == to) {
        return val;
    }
    auto from_dir = uvw_ref_to_direction(from);
    auto to_dir = uvw_ref_to_direction(to);
    double xo = 0.0;
    double yo = 0.0;
    double zo = 0.0;
    convert_xyz_via_direction(val.u_m, val.v_m, val.w_m, from_dir, to_dir, frame, xo, yo, zo);
    return UvwValue{xo, yo, zo};
}

// --- EarthMagnetic conversions -----------------------------------------------

EarthMagneticValue convert_earth_magnetic(const EarthMagneticValue& val, EarthMagneticRef from,
                                          EarthMagneticRef to, const MeasureFrame& frame) {
    if (from == to) {
        return val;
    }
    auto from_dir = earth_magnetic_ref_to_direction(from);
    auto to_dir = earth_magnetic_ref_to_direction(to);
    double xo = 0.0;
    double yo = 0.0;
    double zo = 0.0;
    convert_xyz_via_direction(val.x_t, val.y_t, val.z_t, from_dir, to_dir, frame, xo, yo, zo);
    return EarthMagneticValue{xo, yo, zo};
}

} // namespace

Measure convert_measure(const Measure& m, const MeasureRefType& target, const MeasureFrame& frame) {
    // Validate type match.
    auto target_index = target.index();
    auto source_index = m.ref.ref_type.index();
    if (target_index != source_index) {
        throw std::invalid_argument("Target reference type does not match measure type");
    }

    switch (m.type) {
    case MeasureType::epoch: {
        auto from_ref = std::get<EpochRef>(m.ref.ref_type);
        auto to_ref = std::get<EpochRef>(target);
        auto result = convert_epoch(std::get<EpochValue>(m.value), from_ref, to_ref, frame);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::position: {
        auto from_ref = std::get<PositionRef>(m.ref.ref_type);
        auto to_ref = std::get<PositionRef>(target);
        auto result = convert_position(std::get<PositionValue>(m.value), from_ref, to_ref);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::direction: {
        auto from_ref = std::get<DirectionRef>(m.ref.ref_type);
        auto to_ref = std::get<DirectionRef>(target);
        auto result = convert_direction(std::get<DirectionValue>(m.value), from_ref, to_ref, frame);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::doppler: {
        auto from_ref = std::get<DopplerRef>(m.ref.ref_type);
        auto to_ref = std::get<DopplerRef>(target);
        auto result = convert_doppler(std::get<DopplerValue>(m.value), from_ref, to_ref);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::frequency: {
        auto from_ref = std::get<FrequencyRef>(m.ref.ref_type);
        auto to_ref = std::get<FrequencyRef>(target);
        auto result = convert_frequency(std::get<FrequencyValue>(m.value), from_ref, to_ref);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::radial_velocity: {
        auto from_ref = std::get<RadialVelocityRef>(m.ref.ref_type);
        auto to_ref = std::get<RadialVelocityRef>(target);
        auto result =
            convert_radial_velocity(std::get<RadialVelocityValue>(m.value), from_ref, to_ref);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::baseline: {
        auto from_ref = std::get<BaselineRef>(m.ref.ref_type);
        auto to_ref = std::get<BaselineRef>(target);
        auto result = convert_baseline(std::get<BaselineValue>(m.value), from_ref, to_ref, frame);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::uvw: {
        auto from_ref = std::get<UvwRef>(m.ref.ref_type);
        auto to_ref = std::get<UvwRef>(target);
        auto result = convert_uvw(std::get<UvwValue>(m.value), from_ref, to_ref, frame);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    case MeasureType::earth_magnetic: {
        auto from_ref = std::get<EarthMagneticRef>(m.ref.ref_type);
        auto to_ref = std::get<EarthMagneticRef>(target);
        auto result =
            convert_earth_magnetic(std::get<EarthMagneticValue>(m.value), from_ref, to_ref, frame);
        return Measure{m.type, MeasureRef{target, std::nullopt}, result};
    }
    }
}

} // namespace casacore_mini
