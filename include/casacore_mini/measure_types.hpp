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

// ---------------------------------------------------------------------------
// Measure type discriminator
// ---------------------------------------------------------------------------

/// Discriminator for the 9 supported measure types.
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

/// MEpoch reference frames.
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

/// MDirection reference frames.
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

/// MPosition reference frames.
enum class PositionRef : std::uint8_t { itrf, wgs84 };
[[nodiscard]] std::string_view position_ref_to_string(PositionRef r);
[[nodiscard]] PositionRef string_to_position_ref(std::string_view s);

/// MFrequency reference frames.
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

/// MDoppler reference frames.
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

/// MBaseline reference frames (same set as MDirection, minus planets).
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

/// MEarthMagnetic reference frames.
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
    igrf,
};
[[nodiscard]] std::string_view earth_magnetic_ref_to_string(EarthMagneticRef r);
[[nodiscard]] EarthMagneticRef string_to_earth_magnetic_ref(std::string_view s);

/// Variant of all reference frame enum types.
using MeasureRefType = std::variant<EpochRef, DirectionRef, PositionRef, FrequencyRef, DopplerRef,
                                    RadialVelocityRef, BaselineRef, UvwRef, EarthMagneticRef>;

/// Convert any MeasureRefType variant to its string representation.
[[nodiscard]] std::string measure_ref_to_string(const MeasureRefType& ref);

// ---------------------------------------------------------------------------
// Value structs (one per measure type)
// ---------------------------------------------------------------------------

/// Epoch value: Modified Julian Date split for precision.
struct EpochValue {
    double day = 0.0;      ///< Integer MJD part.
    double fraction = 0.0; ///< Fractional day remainder.
    [[nodiscard]] bool operator==(const EpochValue&) const = default;
};

/// Direction value: longitude/latitude in radians.
struct DirectionValue {
    double lon_rad = 0.0;
    double lat_rad = 0.0;
    [[nodiscard]] bool operator==(const DirectionValue&) const = default;
};

/// Position value: ITRF geocentric XYZ in meters.
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

/// Doppler value (dimensionless ratio).
struct DopplerValue {
    double ratio = 0.0;
    [[nodiscard]] bool operator==(const DopplerValue&) const = default;
};

/// Radial velocity value in m/s.
struct RadialVelocityValue {
    double mps = 0.0;
    [[nodiscard]] bool operator==(const RadialVelocityValue&) const = default;
};

/// Baseline value: XYZ in meters.
struct BaselineValue {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    [[nodiscard]] bool operator==(const BaselineValue&) const = default;
};

/// UVW value in meters.
struct UvwValue {
    double u_m = 0.0;
    double v_m = 0.0;
    double w_m = 0.0;
    [[nodiscard]] bool operator==(const UvwValue&) const = default;
};

/// Earth magnetic field value: XYZ in Tesla.
struct EarthMagneticValue {
    double x_t = 0.0;
    double y_t = 0.0;
    double z_t = 0.0;
    [[nodiscard]] bool operator==(const EarthMagneticValue&) const = default;
};

/// Variant of all measure value types.
using MeasureValue =
    std::variant<EpochValue, DirectionValue, PositionValue, FrequencyValue, DopplerValue,
                 RadialVelocityValue, BaselineValue, UvwValue, EarthMagneticValue>;

// ---------------------------------------------------------------------------
// Measure aggregate
// ---------------------------------------------------------------------------

struct Measure; // forward

/// Reference frame with optional offset measure.
struct MeasureRef {
    MeasureRefType ref_type;
    std::optional<std::shared_ptr<const Measure>> offset;

    [[nodiscard]] bool operator==(const MeasureRef& other) const;
};

/// A concrete measure: type + reference + value.
struct Measure {
    MeasureType type;
    MeasureRef ref;
    MeasureValue value;

    [[nodiscard]] bool operator==(const Measure& other) const = default;
};

/// Return the default reference type for a given measure type.
[[nodiscard]] MeasureRefType default_ref_for_type(MeasureType t);

} // namespace casacore_mini
