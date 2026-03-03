// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace casacore_mini {

/// @file
/// @brief Core measure type system: enums, value structs, and the Measure aggregate.
///
///
///
///
/// A "measure" in the casacore sense is a physical quantity with an
/// associated reference frame.  This header defines the complete type
/// taxonomy for all 9 measure types supported by casacore-mini:
///
/// <dl>
///   <dt>MEpoch</dt>           <dd>Time (UTC, TAI, TDT/TT, TDB, …)</dd>
///   <dt>MDirection</dt>       <dd>Sky direction (J2000, GALACTIC, AZEL, …)</dd>
///   <dt>MPosition</dt>        <dd>Geocentric location (ITRF, WGS84)</dd>
///   <dt>MFrequency</dt>       <dd>Spectral frequency (LSRK, BARY, TOPO, …)</dd>
///   <dt>MDoppler</dt>         <dd>Doppler shift (RADIO, Z, BETA, …)</dd>
///   <dt>MRadialVelocity</dt>  <dd>Radial velocity (LSRK, BARY, TOPO, …)</dd>
///   <dt>MBaseline</dt>        <dd>Baseline vector (J2000, ITRF, AZEL, …)</dd>
///   <dt>Muvw</dt>             <dd>UVW coordinates (same frames as MBaseline)</dd>
///   <dt>MEarthMagnetic</dt>   <dd>Geomagnetic field vector (IGRF, ITRF, …)</dd>
/// </dl>
///
/// Each measure type contributes three things:
///   1. A reference frame enum (e.g. `EpochRef`).
///   2. A value struct (e.g. `EpochValue`).
///   3. String conversion functions for the Reference keyword.
///
/// These are combined into the `Measure` aggregate via
/// `MeasureRefType` (variant of all ref enums) and
/// `MeasureValue` (variant of all value structs).
///
/// Use `measure_types.hpp` for type definitions,
/// `measure_record.hpp` for serialization, and
/// `measure_convert.hpp` for frame conversions.
///
///
/// @par Example
/// @code{.cpp}
///   // Construct an epoch measure at MJD 59000.0 in UTC
///   Measure ep;
///   ep.type = MeasureType::epoch;
///   ep.ref  = MeasureRef{EpochRef::utc, std::nullopt};
///   ep.value = EpochValue{59000.0, 0.0};
///
///   // Construct a J2000 direction (RA=6h, Dec=+30deg)
///   Measure dir;
///   dir.type  = MeasureType::direction;
///   dir.ref   = MeasureRef{DirectionRef::j2000, std::nullopt};
///   dir.value = DirectionValue{6.0 * M_PI / 12.0, 30.0 * M_PI / 180.0};
///
///   // Inspect via variant
///   auto& dv = std::get<DirectionValue>(dir.value);
///   std::cout << "RA  rad: " << dv.lon_rad << '\n';
///   std::cout << "Dec rad: " << dv.lat_rad << '\n';
/// @endcode
///
/// @ingroup measures
/// @addtogroup measures
/// @{

// ---------------------------------------------------------------------------
// Measure type discriminator
// ---------------------------------------------------------------------------

/// Discriminator for the 9 supported measure types.
///
///
/// Used as the `type` field in Measure to identify which reference
/// frame enum and value struct alternatives are active in the
/// `MeasureRefType` and `MeasureValue` variants.
/// String representations match casacore-original's Record type field
/// (lowercase, e.g. "epoch", "direction").
///
enum class MeasureType : std::uint8_t {
    epoch,
    direction,
    position,
    frequency,
    doppler,
    radial_velocity,
    baseline,
    uvw,
    earth_magnetic,
};

/// Convert MeasureType to its Record string (lowercase, e.g. "epoch").
[[nodiscard]] std::string_view measure_type_to_string(MeasureType t);
/// Parse a Record type string to MeasureType. Case-insensitive.
/// @throws std::invalid_argument if unrecognized.
[[nodiscard]] MeasureType string_to_measure_type(std::string_view s);

// ---------------------------------------------------------------------------
// Reference frame enums (one per measure type)
// ---------------------------------------------------------------------------

/// MEpoch reference frames (time standards).
///
///
/// Covers all time systems used in radio astronomy:
/// <dl>
///   <dt>utc</dt>  <dd>Coordinated Universal Time (atomic-based, with leap seconds).</dd>
///   <dt>tai</dt>  <dd>International Atomic Time (UTC + 37 s as of 2024).</dd>
///   <dt>tdt</dt>  <dd>Terrestrial Dynamical Time (= TT; TAI + 32.184 s).</dd>
///   <dt>tdb</dt>  <dd>Barycentric Dynamical Time (relativistic; used for pulsar timing).</dd>
///   <dt>tcg</dt>  <dd>Geocentric Coordinate Time.</dd>
///   <dt>tcb</dt>  <dd>Barycentric Coordinate Time.</dd>
///   <dt>ut1</dt>  <dd>Universal Time 1 (UT1 = UTC + dut1; tracks Earth rotation).</dd>
///   <dt>gmst1</dt><dd>Greenwich Mean Sidereal Time (UT1-based).</dd>
///   <dt>gast</dt> <dd>Greenwich Apparent Sidereal Time.</dd>
///   <dt>last</dt> <dd>Local Apparent Sidereal Time.</dd>
///   <dt>lmst</dt> <dd>Local Mean Sidereal Time.</dd>
///   <dt>ut2</dt>  <dd>Universal Time 2 (smoothed UT1 for seasonal variation).</dd>
/// </dl>
///
enum class EpochRef : std::uint8_t {
    last,
    lmst,
    gmst1,
    gast,
    ut1,
    ut2,
    utc,
    tai,
    tdt,
    tcg,
    tdb,
    tcb,
};
[[nodiscard]] std::string_view epoch_ref_to_string(EpochRef r);
[[nodiscard]] EpochRef string_to_epoch_ref(std::string_view s);

/// MDirection reference frames (celestial and topocentric coordinate systems).
///
///
/// Covers standard astronomical frames including:
/// <dl>
///   <dt>j2000</dt>    <dd>Mean equatorial, epoch J2000.0 (FK5).</dd>
///   <dt>icrs</dt>     <dd>International Celestial Reference System (successor to FK5).</dd>
///   <dt>galactic</dt> <dd>Galactic coordinates (IAU 1958).</dd>
///   <dt>b1950</dt>    <dd>Mean equatorial, epoch B1950.0 (FK4).</dd>
///   <dt>azel</dt>     <dd>Azimuth/elevation (topocentric, requires position+epoch).</dd>
///   <dt>hadec</dt>    <dd>Hour angle / declination (requires position+epoch).</dd>
///   <dt>ecliptic</dt> <dd>Mean ecliptic, epoch J2000.0.</dd>
///   <dt>supergal</dt> <dd>Supergalactic coordinates.</dd>
///   <dt>itrf</dt>     <dd>ITRF-aligned (used for near-field sources).</dd>
/// </dl>
/// Apparent and mean variants (app, jmean, jtrue, bmean, btrue) account for
/// nutation, aberration, and refraction corrections.
///
enum class DirectionRef : std::uint8_t {
    j2000,
    jmean,
    jtrue,
    app,
    b1950,
    b1950_vla,
    bmean,
    btrue,
    galactic,
    hadec,
    azel,
    azelsw,
    azelgeo,
    azelswgeo,
    jnat,
    ecliptic,
    mecliptic,
    tecliptic,
    supergal,
    itrf,
    topo,
    icrs,
};
[[nodiscard]] std::string_view direction_ref_to_string(DirectionRef r);
[[nodiscard]] DirectionRef string_to_direction_ref(std::string_view s);

/// MPosition reference frames for telescope/antenna positions.
///
///
/// <dl>
///   <dt>itrf</dt>  <dd>IERS Terrestrial Reference Frame (geocentric Cartesian XYZ).</dd>
///   <dt>wgs84</dt> <dd>WGS84 geodetic ellipsoid (latitude, longitude, height).</dd>
/// </dl>
///
enum class PositionRef : std::uint8_t { itrf, wgs84 };
[[nodiscard]] std::string_view position_ref_to_string(PositionRef r);
[[nodiscard]] PositionRef string_to_position_ref(std::string_view s);

/// MFrequency reference frames (spectral windows).
///
///
/// <dl>
///   <dt>rest</dt>    <dd>Rest frequency of the spectral line (no motion).</dd>
///   <dt>lsrk</dt>   <dd>Kinematic Local Standard of Rest (~20 km/s towards Vega).</dd>
///   <dt>lsrd</dt>   <dd>Dynamic Local Standard of Rest (Galactic plane definition).</dd>
///   <dt>bary</dt>   <dd>Barycentric (Solar System barycentre).</dd>
///   <dt>geo</dt>    <dd>Geocentric (Earth centre).</dd>
///   <dt>topo</dt>   <dd>Topocentric (telescope site; requires position+epoch).</dd>
///   <dt>galacto</dt><dd>Galactocentric.</dd>
///   <dt>lgroup</dt> <dd>Local Group of galaxies rest frame.</dd>
///   <dt>cmb</dt>    <dd>CMB dipole rest frame.</dd>
/// </dl>
///
enum class FrequencyRef : std::uint8_t {
    rest,
    lsrk,
    lsrd,
    bary,
    geo,
    topo,
    galacto,
    lgroup,
    cmb,
};
[[nodiscard]] std::string_view frequency_ref_to_string(FrequencyRef r);
[[nodiscard]] FrequencyRef string_to_frequency_ref(std::string_view s);

/// MDoppler reference conventions.
///
///
/// Different communities define Doppler shift differently:
/// <dl>
///   <dt>radio</dt> <dd>v/c = (f0-f)/f0  (radio convention, most common in cm/mm radio).</dd>
///   <dt>z</dt>     <dd>Redshift z = (f0-f)/f  (optical/cosmology convention).</dd>
///   <dt>ratio</dt> <dd>f/f0 (frequency ratio).</dd>
///   <dt>beta</dt>  <dd>v/c relativistic beta = (f0^2-f^2)/(f0^2+f^2).</dd>
///   <dt>gamma</dt> <dd>Lorentz gamma factor.</dd>
/// </dl>
///
enum class DopplerRef : std::uint8_t { radio, z, ratio, beta, gamma };
[[nodiscard]] std::string_view doppler_ref_to_string(DopplerRef r);
[[nodiscard]] DopplerRef string_to_doppler_ref(std::string_view s);

/// MRadialVelocity reference frames (same set as MFrequency minus REST).
enum class RadialVelocityRef : std::uint8_t {
    lsrk,
    lsrd,
    bary,
    geo,
    topo,
    galacto,
    lgroup,
    cmb,
};
[[nodiscard]] std::string_view radial_velocity_ref_to_string(RadialVelocityRef r);
[[nodiscard]] RadialVelocityRef string_to_radial_velocity_ref(std::string_view s);

/// MBaseline reference frames (same set as MDirection, minus planet-tracking frames).
enum class BaselineRef : std::uint8_t {
    j2000,
    jmean,
    jtrue,
    app,
    b1950,
    b1950_vla,
    bmean,
    btrue,
    galactic,
    hadec,
    azel,
    azelsw,
    azelgeo,
    azelswgeo,
    jnat,
    ecliptic,
    mecliptic,
    tecliptic,
    supergal,
    itrf,
    topo,
    icrs,
};
[[nodiscard]] std::string_view baseline_ref_to_string(BaselineRef r);
[[nodiscard]] BaselineRef string_to_baseline_ref(std::string_view s);

/// Muvw reference frames (same set as MBaseline).
enum class UvwRef : std::uint8_t {
    j2000,
    jmean,
    jtrue,
    app,
    b1950,
    b1950_vla,
    bmean,
    btrue,
    galactic,
    hadec,
    azel,
    azelsw,
    azelgeo,
    azelswgeo,
    jnat,
    ecliptic,
    mecliptic,
    tecliptic,
    supergal,
    itrf,
    topo,
    icrs,
};
[[nodiscard]] std::string_view uvw_ref_to_string(UvwRef r);
[[nodiscard]] UvwRef string_to_uvw_ref(std::string_view s);

/// MEarthMagnetic reference frames (includes IGRF model-based frame).
enum class EarthMagneticRef : std::uint8_t {
    j2000,
    jmean,
    jtrue,
    app,
    b1950,
    bmean,
    btrue,
    galactic,
    hadec,
    azel,
    azelsw,
    azelgeo,
    azelswgeo,
    jnat,
    ecliptic,
    mecliptic,
    tecliptic,
    supergal,
    itrf,
    topo,
    icrs,
    igrf, ///< International Geomagnetic Reference Field model frame.
};
[[nodiscard]] std::string_view earth_magnetic_ref_to_string(EarthMagneticRef r);
[[nodiscard]] EarthMagneticRef string_to_earth_magnetic_ref(std::string_view s);

/// Variant of all reference frame enum types.
///
///
/// Held in `MeasureRef::ref_type` to carry the active reference
/// frame for a Measure without virtual dispatch.  The active alternative
/// must match the `MeasureType` of the enclosing Measure.
///
using MeasureRefType = std::variant<EpochRef, DirectionRef, PositionRef, FrequencyRef, DopplerRef,
                                    RadialVelocityRef, BaselineRef, UvwRef, EarthMagneticRef>;

/// Convert any MeasureRefType variant to its string representation.
[[nodiscard]] std::string measure_ref_to_string(const MeasureRefType& ref);

// ---------------------------------------------------------------------------
// Value structs (one per measure type)
// ---------------------------------------------------------------------------

/// Epoch value: Modified Julian Date split for numerical precision.
///
///
/// MJD is stored as integer `day` and fractional
/// `fraction` to preserve sub-nanosecond precision that would
/// be lost in a single 64-bit double (which has only ~15 significant
/// decimal digits).  The full MJD is `day + fraction`.
///
struct EpochValue {
    double day = 0.0;      ///< Integer MJD part.
    double fraction = 0.0; ///< Fractional day remainder.
    [[nodiscard]] bool operator==(const EpochValue&) const = default;
};

/// Direction value: celestial longitude and latitude in radians.
///
///
/// Angles are stored in radians.  For equatorial frames `lon_rad`
/// is right ascension and `lat_rad` is declination; for
/// horizontal frames `lon_rad` is azimuth and `lat_rad`
/// is elevation.
///
struct DirectionValue {
    double lon_rad = 0.0;
    double lat_rad = 0.0;
    [[nodiscard]] bool operator==(const DirectionValue&) const = default;
};

/// Position value: geocentric Cartesian XYZ in metres (ITRF convention).
struct PositionValue {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    [[nodiscard]] bool operator==(const PositionValue&) const = default;
};

/// Frequency value in Hz.
struct FrequencyValue {
    double hz = 0.0;
    [[nodiscard]] bool operator==(const FrequencyValue&) const = default;
};

/// Doppler value (dimensionless; interpretation depends on DopplerRef).
struct DopplerValue {
    double ratio = 0.0;
    [[nodiscard]] bool operator==(const DopplerValue&) const = default;
};

/// Radial velocity value in m/s.
struct RadialVelocityValue {
    double mps = 0.0;
    [[nodiscard]] bool operator==(const RadialVelocityValue&) const = default;
};

/// Baseline vector value: geocentric XYZ in metres.
struct BaselineValue {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    [[nodiscard]] bool operator==(const BaselineValue&) const = default;
};

/// UVW coordinate value in metres.
///
///
/// `u_m` and `v_m` are the east and north components of
/// the baseline projected onto the plane perpendicular to the phase centre
/// direction.  `w_m` is the component along the phase centre
/// direction (delay axis).
///
struct UvwValue {
    double u_m = 0.0;
    double v_m = 0.0;
    double w_m = 0.0;
    [[nodiscard]] bool operator==(const UvwValue&) const = default;
};

/// Earth magnetic field value: XYZ field components in Tesla.
struct EarthMagneticValue {
    double x_t = 0.0;
    double y_t = 0.0;
    double z_t = 0.0;
    [[nodiscard]] bool operator==(const EarthMagneticValue&) const = default;
};

/// Variant of all measure value types.
///
///
/// Held in `Measure::value`.  The active alternative must be
/// consistent with `Measure::type`:
/// <dl>
///   <dt>epoch</dt>           <dd>EpochValue</dd>
///   <dt>direction</dt>       <dd>DirectionValue</dd>
///   <dt>position</dt>        <dd>PositionValue</dd>
///   <dt>frequency</dt>       <dd>FrequencyValue</dd>
///   <dt>doppler</dt>         <dd>DopplerValue</dd>
///   <dt>radial_velocity</dt> <dd>RadialVelocityValue</dd>
///   <dt>baseline</dt>        <dd>BaselineValue</dd>
///   <dt>uvw</dt>             <dd>UvwValue</dd>
///   <dt>earth_magnetic</dt>  <dd>EarthMagneticValue</dd>
/// </dl>
///
using MeasureValue =
    std::variant<EpochValue, DirectionValue, PositionValue, FrequencyValue, DopplerValue,
                 RadialVelocityValue, BaselineValue, UvwValue, EarthMagneticValue>;

// ---------------------------------------------------------------------------
// Measure aggregate
// ---------------------------------------------------------------------------

struct Measure; // forward

/// Reference frame with an optional chained offset measure.
///
///
/// `ref_type` holds the primary reference frame for the measure.
/// `offset` is a recursive Measure used to express a difference
/// from a base epoch or direction (e.g. an epoch offset for a pulsar
/// timing reference).  The offset measure must have the same
/// `MeasureType` as the parent.
///
struct MeasureRef {
    MeasureRefType ref_type;
    std::optional<std::shared_ptr<const Measure>> offset;

    [[nodiscard]] bool operator==(const MeasureRef& other) const;
};

///
/// A concrete physical measure: type discriminator, reference frame, and value.
///
///
///
///
///
/// Measure is the central aggregate that binds together:
/// <dl>
///   <dt>type</dt>  <dd>MeasureType enum selecting the physical quantity kind.</dd>
///   <dt>ref</dt>   <dd>MeasureRef holding the reference frame and optional offset.</dd>
///   <dt>value</dt> <dd>MeasureValue variant holding the numerical data.</dd>
/// </dl>
///
/// The combination mirrors casacore-original's Measure class hierarchy
/// (MEpoch, MDirection, etc.) but without virtual dispatch, enabling
/// storage in standard containers and cheap copies.
///
/// Invariant: `ref.ref_type` must hold the alternative that
/// corresponds to `type` (e.g. type==epoch implies
/// `std::holds_alternative<EpochRef>(ref.ref_type)`).  Functions
/// in `measure_convert.hpp` and `measure_record.hpp`
/// enforce this invariant and throw `std::invalid_argument` on
/// violation.
///
///
/// @par Example
/// @code{.cpp}
///   // Build a UTC epoch at MJD 59000.5
///   Measure utc_epoch;
///   utc_epoch.type  = MeasureType::epoch;
///   utc_epoch.ref   = MeasureRef{EpochRef::utc, std::nullopt};
///   utc_epoch.value = EpochValue{59000.0, 0.5};
///
///   // Convert to TAI using measure_convert
///   MeasureFrame frame;
///   auto tai_epoch = convert_measure(utc_epoch, EpochRef::tai, frame);
///   double tai_mjd = std::get<EpochValue>(tai_epoch.value).day
///                  + std::get<EpochValue>(tai_epoch.value).fraction;
/// @endcode
///
struct Measure {
    MeasureType type;
    MeasureRef ref;
    MeasureValue value;

    [[nodiscard]] bool operator==(const Measure& other) const = default;
};

/// Return the default reference type for a given measure type.
///
///
/// Returns the most natural "zero-point" reference frame for each measure
/// kind (e.g. EpochRef::utc for epoch, DirectionRef::j2000 for direction).
/// Used when constructing measures where the reference is not yet known.
///
[[nodiscard]] MeasureRefType default_ref_for_type(MeasureType t);

/// @}
} // namespace casacore_mini
