#include "casacore_mini/measure_types.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {

namespace {

// Case-insensitive string comparison.
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// Generic lookup: array of {enum, canonical_name, synonyms...}.
template <typename Enum, std::size_t N, std::size_t MaxSyns> struct EnumEntry {
    Enum value;
    std::string_view canonical;
    std::array<std::string_view, MaxSyns> synonyms;
    std::size_t num_synonyms;
};

} // namespace

// ---------------------------------------------------------------------------
// MeasureType
// ---------------------------------------------------------------------------

std::string_view measure_type_to_string(MeasureType t) {
    switch (t) {
    case MeasureType::epoch:
        return "epoch";
    case MeasureType::direction:
        return "direction";
    case MeasureType::position:
        return "position";
    case MeasureType::frequency:
        return "frequency";
    case MeasureType::doppler:
        return "doppler";
    case MeasureType::radial_velocity:
        return "radialvelocity";
    case MeasureType::baseline:
        return "baseline";
    case MeasureType::uvw:
        return "uvw";
    case MeasureType::earth_magnetic:
        return "earthmagnetic";
    }
    return "epoch"; // unreachable, silence warning
}

MeasureType string_to_measure_type(std::string_view s) {
    if (iequals(s, "epoch"))
        return MeasureType::epoch;
    if (iequals(s, "direction"))
        return MeasureType::direction;
    if (iequals(s, "position"))
        return MeasureType::position;
    if (iequals(s, "frequency"))
        return MeasureType::frequency;
    if (iequals(s, "doppler"))
        return MeasureType::doppler;
    if (iequals(s, "radialvelocity"))
        return MeasureType::radial_velocity;
    if (iequals(s, "baseline"))
        return MeasureType::baseline;
    if (iequals(s, "uvw"))
        return MeasureType::uvw;
    if (iequals(s, "earthmagnetic"))
        return MeasureType::earth_magnetic;
    throw std::invalid_argument("Unknown measure type: " + std::string(s));
}

// ---------------------------------------------------------------------------
// EpochRef
// ---------------------------------------------------------------------------

std::string_view epoch_ref_to_string(EpochRef r) {
    switch (r) {
    case EpochRef::last:
        return "LAST";
    case EpochRef::lmst:
        return "LMST";
    case EpochRef::gmst1:
        return "GMST1";
    case EpochRef::gast:
        return "GAST";
    case EpochRef::ut1:
        return "UT1";
    case EpochRef::ut2:
        return "UT2";
    case EpochRef::utc:
        return "UTC";
    case EpochRef::tai:
        return "TAI";
    case EpochRef::tdt:
        return "TDT";
    case EpochRef::tcg:
        return "TCG";
    case EpochRef::tdb:
        return "TDB";
    case EpochRef::tcb:
        return "TCB";
    }
    return "UTC"; // unreachable
}

EpochRef string_to_epoch_ref(std::string_view s) {
    if (iequals(s, "LAST"))
        return EpochRef::last;
    if (iequals(s, "LMST"))
        return EpochRef::lmst;
    if (iequals(s, "GMST1") || iequals(s, "GMST"))
        return EpochRef::gmst1;
    if (iequals(s, "GAST"))
        return EpochRef::gast;
    if (iequals(s, "UT1") || iequals(s, "UT"))
        return EpochRef::ut1;
    if (iequals(s, "UT2"))
        return EpochRef::ut2;
    if (iequals(s, "UTC"))
        return EpochRef::utc;
    if (iequals(s, "TAI") || iequals(s, "IAT"))
        return EpochRef::tai;
    if (iequals(s, "TDT") || iequals(s, "TT") || iequals(s, "ET"))
        return EpochRef::tdt;
    if (iequals(s, "TCG"))
        return EpochRef::tcg;
    if (iequals(s, "TDB"))
        return EpochRef::tdb;
    if (iequals(s, "TCB"))
        return EpochRef::tcb;
    throw std::invalid_argument("Unknown epoch reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// DirectionRef
// ---------------------------------------------------------------------------

std::string_view direction_ref_to_string(DirectionRef r) {
    switch (r) {
    case DirectionRef::j2000:
        return "J2000";
    case DirectionRef::jmean:
        return "JMEAN";
    case DirectionRef::jtrue:
        return "JTRUE";
    case DirectionRef::app:
        return "APP";
    case DirectionRef::b1950:
        return "B1950";
    case DirectionRef::b1950_vla:
        return "B1950_VLA";
    case DirectionRef::bmean:
        return "BMEAN";
    case DirectionRef::btrue:
        return "BTRUE";
    case DirectionRef::galactic:
        return "GALACTIC";
    case DirectionRef::hadec:
        return "HADEC";
    case DirectionRef::azel:
        return "AZEL";
    case DirectionRef::azelsw:
        return "AZELSW";
    case DirectionRef::azelgeo:
        return "AZELGEO";
    case DirectionRef::azelswgeo:
        return "AZELSWGEO";
    case DirectionRef::jnat:
        return "JNAT";
    case DirectionRef::ecliptic:
        return "ECLIPTIC";
    case DirectionRef::mecliptic:
        return "MECLIPTIC";
    case DirectionRef::tecliptic:
        return "TECLIPTIC";
    case DirectionRef::supergal:
        return "SUPERGAL";
    case DirectionRef::itrf:
        return "ITRF";
    case DirectionRef::topo:
        return "TOPO";
    case DirectionRef::icrs:
        return "ICRS";
    }
    return "J2000"; // unreachable
}

DirectionRef string_to_direction_ref(std::string_view s) {
    if (iequals(s, "J2000"))
        return DirectionRef::j2000;
    if (iequals(s, "JMEAN"))
        return DirectionRef::jmean;
    if (iequals(s, "JTRUE"))
        return DirectionRef::jtrue;
    if (iequals(s, "APP"))
        return DirectionRef::app;
    if (iequals(s, "B1950"))
        return DirectionRef::b1950;
    if (iequals(s, "B1950_VLA"))
        return DirectionRef::b1950_vla;
    if (iequals(s, "BMEAN"))
        return DirectionRef::bmean;
    if (iequals(s, "BTRUE"))
        return DirectionRef::btrue;
    if (iequals(s, "GALACTIC"))
        return DirectionRef::galactic;
    if (iequals(s, "HADEC"))
        return DirectionRef::hadec;
    if (iequals(s, "AZEL") || iequals(s, "AZELNE"))
        return DirectionRef::azel;
    if (iequals(s, "AZELSW"))
        return DirectionRef::azelsw;
    if (iequals(s, "AZELGEO") || iequals(s, "AZELNEGEO"))
        return DirectionRef::azelgeo;
    if (iequals(s, "AZELSWGEO"))
        return DirectionRef::azelswgeo;
    if (iequals(s, "JNAT"))
        return DirectionRef::jnat;
    if (iequals(s, "ECLIPTIC"))
        return DirectionRef::ecliptic;
    if (iequals(s, "MECLIPTIC"))
        return DirectionRef::mecliptic;
    if (iequals(s, "TECLIPTIC"))
        return DirectionRef::tecliptic;
    if (iequals(s, "SUPERGAL"))
        return DirectionRef::supergal;
    if (iequals(s, "ITRF"))
        return DirectionRef::itrf;
    if (iequals(s, "TOPO"))
        return DirectionRef::topo;
    if (iequals(s, "ICRS"))
        return DirectionRef::icrs;
    throw std::invalid_argument("Unknown direction reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// PositionRef
// ---------------------------------------------------------------------------

std::string_view position_ref_to_string(PositionRef r) {
    switch (r) {
    case PositionRef::itrf:
        return "ITRF";
    case PositionRef::wgs84:
        return "WGS84";
    }
    return "ITRF"; // unreachable
}

PositionRef string_to_position_ref(std::string_view s) {
    if (iequals(s, "ITRF"))
        return PositionRef::itrf;
    if (iequals(s, "WGS84"))
        return PositionRef::wgs84;
    throw std::invalid_argument("Unknown position reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// FrequencyRef
// ---------------------------------------------------------------------------

std::string_view frequency_ref_to_string(FrequencyRef r) {
    switch (r) {
    case FrequencyRef::rest:
        return "REST";
    case FrequencyRef::lsrk:
        return "LSRK";
    case FrequencyRef::lsrd:
        return "LSRD";
    case FrequencyRef::bary:
        return "BARY";
    case FrequencyRef::geo:
        return "GEO";
    case FrequencyRef::topo:
        return "TOPO";
    case FrequencyRef::galacto:
        return "GALACTO";
    case FrequencyRef::lgroup:
        return "LGROUP";
    case FrequencyRef::cmb:
        return "CMB";
    }
    return "REST"; // unreachable
}

FrequencyRef string_to_frequency_ref(std::string_view s) {
    if (iequals(s, "REST"))
        return FrequencyRef::rest;
    if (iequals(s, "LSRK") || iequals(s, "LSR"))
        return FrequencyRef::lsrk;
    if (iequals(s, "LSRD"))
        return FrequencyRef::lsrd;
    if (iequals(s, "BARY"))
        return FrequencyRef::bary;
    if (iequals(s, "GEO"))
        return FrequencyRef::geo;
    if (iequals(s, "TOPO"))
        return FrequencyRef::topo;
    if (iequals(s, "GALACTO"))
        return FrequencyRef::galacto;
    if (iequals(s, "LGROUP"))
        return FrequencyRef::lgroup;
    if (iequals(s, "CMB"))
        return FrequencyRef::cmb;
    throw std::invalid_argument("Unknown frequency reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// DopplerRef
// ---------------------------------------------------------------------------

std::string_view doppler_ref_to_string(DopplerRef r) {
    switch (r) {
    case DopplerRef::radio:
        return "RADIO";
    case DopplerRef::z:
        return "Z";
    case DopplerRef::ratio:
        return "RATIO";
    case DopplerRef::beta:
        return "BETA";
    case DopplerRef::gamma:
        return "GAMMA";
    }
    return "RADIO"; // unreachable
}

DopplerRef string_to_doppler_ref(std::string_view s) {
    if (iequals(s, "RADIO"))
        return DopplerRef::radio;
    if (iequals(s, "Z") || iequals(s, "OPTICAL"))
        return DopplerRef::z;
    if (iequals(s, "RATIO"))
        return DopplerRef::ratio;
    if (iequals(s, "BETA") || iequals(s, "RELATIVISTIC"))
        return DopplerRef::beta;
    if (iequals(s, "GAMMA"))
        return DopplerRef::gamma;
    throw std::invalid_argument("Unknown doppler reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// RadialVelocityRef
// ---------------------------------------------------------------------------

std::string_view radial_velocity_ref_to_string(RadialVelocityRef r) {
    switch (r) {
    case RadialVelocityRef::lsrk:
        return "LSRK";
    case RadialVelocityRef::lsrd:
        return "LSRD";
    case RadialVelocityRef::bary:
        return "BARY";
    case RadialVelocityRef::geo:
        return "GEO";
    case RadialVelocityRef::topo:
        return "TOPO";
    case RadialVelocityRef::galacto:
        return "GALACTO";
    case RadialVelocityRef::lgroup:
        return "LGROUP";
    case RadialVelocityRef::cmb:
        return "CMB";
    }
    return "LSRK"; // unreachable
}

RadialVelocityRef string_to_radial_velocity_ref(std::string_view s) {
    if (iequals(s, "LSRK") || iequals(s, "LSR"))
        return RadialVelocityRef::lsrk;
    if (iequals(s, "LSRD"))
        return RadialVelocityRef::lsrd;
    if (iequals(s, "BARY"))
        return RadialVelocityRef::bary;
    if (iequals(s, "GEO"))
        return RadialVelocityRef::geo;
    if (iequals(s, "TOPO"))
        return RadialVelocityRef::topo;
    if (iequals(s, "GALACTO"))
        return RadialVelocityRef::galacto;
    if (iequals(s, "LGROUP"))
        return RadialVelocityRef::lgroup;
    if (iequals(s, "CMB"))
        return RadialVelocityRef::cmb;
    throw std::invalid_argument("Unknown radial velocity reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// BaselineRef
// ---------------------------------------------------------------------------

std::string_view baseline_ref_to_string(BaselineRef r) {
    switch (r) {
    case BaselineRef::j2000:
        return "J2000";
    case BaselineRef::jmean:
        return "JMEAN";
    case BaselineRef::jtrue:
        return "JTRUE";
    case BaselineRef::app:
        return "APP";
    case BaselineRef::b1950:
        return "B1950";
    case BaselineRef::b1950_vla:
        return "B1950_VLA";
    case BaselineRef::bmean:
        return "BMEAN";
    case BaselineRef::btrue:
        return "BTRUE";
    case BaselineRef::galactic:
        return "GALACTIC";
    case BaselineRef::hadec:
        return "HADEC";
    case BaselineRef::azel:
        return "AZEL";
    case BaselineRef::azelsw:
        return "AZELSW";
    case BaselineRef::azelgeo:
        return "AZELGEO";
    case BaselineRef::azelswgeo:
        return "AZELSWGEO";
    case BaselineRef::jnat:
        return "JNAT";
    case BaselineRef::ecliptic:
        return "ECLIPTIC";
    case BaselineRef::mecliptic:
        return "MECLIPTIC";
    case BaselineRef::tecliptic:
        return "TECLIPTIC";
    case BaselineRef::supergal:
        return "SUPERGAL";
    case BaselineRef::itrf:
        return "ITRF";
    case BaselineRef::topo:
        return "TOPO";
    case BaselineRef::icrs:
        return "ICRS";
    }
    return "J2000"; // unreachable
}

BaselineRef string_to_baseline_ref(std::string_view s) {
    if (iequals(s, "J2000"))
        return BaselineRef::j2000;
    if (iequals(s, "JMEAN"))
        return BaselineRef::jmean;
    if (iequals(s, "JTRUE"))
        return BaselineRef::jtrue;
    if (iequals(s, "APP"))
        return BaselineRef::app;
    if (iequals(s, "B1950"))
        return BaselineRef::b1950;
    if (iequals(s, "B1950_VLA"))
        return BaselineRef::b1950_vla;
    if (iequals(s, "BMEAN"))
        return BaselineRef::bmean;
    if (iequals(s, "BTRUE"))
        return BaselineRef::btrue;
    if (iequals(s, "GALACTIC"))
        return BaselineRef::galactic;
    if (iequals(s, "HADEC"))
        return BaselineRef::hadec;
    if (iequals(s, "AZEL") || iequals(s, "AZELNE"))
        return BaselineRef::azel;
    if (iequals(s, "AZELSW"))
        return BaselineRef::azelsw;
    if (iequals(s, "AZELGEO") || iequals(s, "AZELNEGEO"))
        return BaselineRef::azelgeo;
    if (iequals(s, "AZELSWGEO"))
        return BaselineRef::azelswgeo;
    if (iequals(s, "JNAT"))
        return BaselineRef::jnat;
    if (iequals(s, "ECLIPTIC"))
        return BaselineRef::ecliptic;
    if (iequals(s, "MECLIPTIC"))
        return BaselineRef::mecliptic;
    if (iequals(s, "TECLIPTIC"))
        return BaselineRef::tecliptic;
    if (iequals(s, "SUPERGAL"))
        return BaselineRef::supergal;
    if (iequals(s, "ITRF"))
        return BaselineRef::itrf;
    if (iequals(s, "TOPO"))
        return BaselineRef::topo;
    if (iequals(s, "ICRS"))
        return BaselineRef::icrs;
    throw std::invalid_argument("Unknown baseline reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// UvwRef
// ---------------------------------------------------------------------------

std::string_view uvw_ref_to_string(UvwRef r) {
    switch (r) {
    case UvwRef::j2000:
        return "J2000";
    case UvwRef::jmean:
        return "JMEAN";
    case UvwRef::jtrue:
        return "JTRUE";
    case UvwRef::app:
        return "APP";
    case UvwRef::b1950:
        return "B1950";
    case UvwRef::b1950_vla:
        return "B1950_VLA";
    case UvwRef::bmean:
        return "BMEAN";
    case UvwRef::btrue:
        return "BTRUE";
    case UvwRef::galactic:
        return "GALACTIC";
    case UvwRef::hadec:
        return "HADEC";
    case UvwRef::azel:
        return "AZEL";
    case UvwRef::azelsw:
        return "AZELSW";
    case UvwRef::azelgeo:
        return "AZELGEO";
    case UvwRef::azelswgeo:
        return "AZELSWGEO";
    case UvwRef::jnat:
        return "JNAT";
    case UvwRef::ecliptic:
        return "ECLIPTIC";
    case UvwRef::mecliptic:
        return "MECLIPTIC";
    case UvwRef::tecliptic:
        return "TECLIPTIC";
    case UvwRef::supergal:
        return "SUPERGAL";
    case UvwRef::itrf:
        return "ITRF";
    case UvwRef::topo:
        return "TOPO";
    case UvwRef::icrs:
        return "ICRS";
    }
    return "J2000"; // unreachable
}

UvwRef string_to_uvw_ref(std::string_view s) {
    if (iequals(s, "J2000"))
        return UvwRef::j2000;
    if (iequals(s, "JMEAN"))
        return UvwRef::jmean;
    if (iequals(s, "JTRUE"))
        return UvwRef::jtrue;
    if (iequals(s, "APP"))
        return UvwRef::app;
    if (iequals(s, "B1950"))
        return UvwRef::b1950;
    if (iequals(s, "B1950_VLA"))
        return UvwRef::b1950_vla;
    if (iequals(s, "BMEAN"))
        return UvwRef::bmean;
    if (iequals(s, "BTRUE"))
        return UvwRef::btrue;
    if (iequals(s, "GALACTIC"))
        return UvwRef::galactic;
    if (iequals(s, "HADEC"))
        return UvwRef::hadec;
    if (iequals(s, "AZEL") || iequals(s, "AZELNE"))
        return UvwRef::azel;
    if (iequals(s, "AZELSW"))
        return UvwRef::azelsw;
    if (iequals(s, "AZELGEO") || iequals(s, "AZELNEGEO"))
        return UvwRef::azelgeo;
    if (iequals(s, "AZELSWGEO"))
        return UvwRef::azelswgeo;
    if (iequals(s, "JNAT"))
        return UvwRef::jnat;
    if (iequals(s, "ECLIPTIC"))
        return UvwRef::ecliptic;
    if (iequals(s, "MECLIPTIC"))
        return UvwRef::mecliptic;
    if (iequals(s, "TECLIPTIC"))
        return UvwRef::tecliptic;
    if (iequals(s, "SUPERGAL"))
        return UvwRef::supergal;
    if (iequals(s, "ITRF"))
        return UvwRef::itrf;
    if (iequals(s, "TOPO"))
        return UvwRef::topo;
    if (iequals(s, "ICRS"))
        return UvwRef::icrs;
    throw std::invalid_argument("Unknown uvw reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// EarthMagneticRef
// ---------------------------------------------------------------------------

std::string_view earth_magnetic_ref_to_string(EarthMagneticRef r) {
    switch (r) {
    case EarthMagneticRef::j2000:
        return "J2000";
    case EarthMagneticRef::jmean:
        return "JMEAN";
    case EarthMagneticRef::jtrue:
        return "JTRUE";
    case EarthMagneticRef::app:
        return "APP";
    case EarthMagneticRef::b1950:
        return "B1950";
    case EarthMagneticRef::bmean:
        return "BMEAN";
    case EarthMagneticRef::btrue:
        return "BTRUE";
    case EarthMagneticRef::galactic:
        return "GALACTIC";
    case EarthMagneticRef::hadec:
        return "HADEC";
    case EarthMagneticRef::azel:
        return "AZEL";
    case EarthMagneticRef::azelsw:
        return "AZELSW";
    case EarthMagneticRef::azelgeo:
        return "AZELGEO";
    case EarthMagneticRef::azelswgeo:
        return "AZELSWGEO";
    case EarthMagneticRef::jnat:
        return "JNAT";
    case EarthMagneticRef::ecliptic:
        return "ECLIPTIC";
    case EarthMagneticRef::mecliptic:
        return "MECLIPTIC";
    case EarthMagneticRef::tecliptic:
        return "TECLIPTIC";
    case EarthMagneticRef::supergal:
        return "SUPERGAL";
    case EarthMagneticRef::itrf:
        return "ITRF";
    case EarthMagneticRef::topo:
        return "TOPO";
    case EarthMagneticRef::icrs:
        return "ICRS";
    case EarthMagneticRef::igrf:
        return "IGRF";
    }
    return "J2000"; // unreachable
}

EarthMagneticRef string_to_earth_magnetic_ref(std::string_view s) {
    if (iequals(s, "J2000"))
        return EarthMagneticRef::j2000;
    if (iequals(s, "JMEAN"))
        return EarthMagneticRef::jmean;
    if (iequals(s, "JTRUE"))
        return EarthMagneticRef::jtrue;
    if (iequals(s, "APP"))
        return EarthMagneticRef::app;
    if (iequals(s, "B1950"))
        return EarthMagneticRef::b1950;
    if (iequals(s, "BMEAN"))
        return EarthMagneticRef::bmean;
    if (iequals(s, "BTRUE"))
        return EarthMagneticRef::btrue;
    if (iequals(s, "GALACTIC"))
        return EarthMagneticRef::galactic;
    if (iequals(s, "HADEC"))
        return EarthMagneticRef::hadec;
    if (iequals(s, "AZEL") || iequals(s, "AZELNE"))
        return EarthMagneticRef::azel;
    if (iequals(s, "AZELSW"))
        return EarthMagneticRef::azelsw;
    if (iequals(s, "AZELGEO") || iequals(s, "AZELNEGEO"))
        return EarthMagneticRef::azelgeo;
    if (iequals(s, "AZELSWGEO"))
        return EarthMagneticRef::azelswgeo;
    if (iequals(s, "JNAT"))
        return EarthMagneticRef::jnat;
    if (iequals(s, "ECLIPTIC"))
        return EarthMagneticRef::ecliptic;
    if (iequals(s, "MECLIPTIC"))
        return EarthMagneticRef::mecliptic;
    if (iequals(s, "TECLIPTIC"))
        return EarthMagneticRef::tecliptic;
    if (iequals(s, "SUPERGAL"))
        return EarthMagneticRef::supergal;
    if (iequals(s, "ITRF"))
        return EarthMagneticRef::itrf;
    if (iequals(s, "TOPO"))
        return EarthMagneticRef::topo;
    if (iequals(s, "ICRS"))
        return EarthMagneticRef::icrs;
    if (iequals(s, "IGRF"))
        return EarthMagneticRef::igrf;
    throw std::invalid_argument("Unknown earth magnetic reference: " + std::string(s));
}

// ---------------------------------------------------------------------------
// MeasureRefType variant -> string
// ---------------------------------------------------------------------------

std::string measure_ref_to_string(const MeasureRefType& ref) {
    return std::visit(
        [](auto r) -> std::string {
            using ref_t = decltype(r); // NOLINT(readability-identifier-naming)
            if constexpr (std::is_same_v<ref_t, EpochRef>)
                return std::string(epoch_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, DirectionRef>)
                return std::string(direction_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, PositionRef>)
                return std::string(position_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, FrequencyRef>)
                return std::string(frequency_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, DopplerRef>)
                return std::string(doppler_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, RadialVelocityRef>)
                return std::string(radial_velocity_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, BaselineRef>)
                return std::string(baseline_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, UvwRef>)
                return std::string(uvw_ref_to_string(r));
            else if constexpr (std::is_same_v<ref_t, EarthMagneticRef>)
                return std::string(earth_magnetic_ref_to_string(r));
            else
                return "";
        },
        ref);
}

// ---------------------------------------------------------------------------
// MeasureRef::operator==
// ---------------------------------------------------------------------------

bool MeasureRef::operator==(const MeasureRef& other) const {
    if (ref_type != other.ref_type)
        return false;
    if (offset.has_value() != other.offset.has_value())
        return false;
    if (offset.has_value() && other.offset.has_value()) {
        if (!*offset && !*other.offset)
            return true;
        if (!*offset || !*other.offset)
            return false;
        return **offset == **other.offset;
    }
    return true;
}

// ---------------------------------------------------------------------------
// default_ref_for_type
// ---------------------------------------------------------------------------

MeasureRefType default_ref_for_type(MeasureType t) {
    switch (t) {
    case MeasureType::epoch:
        return EpochRef::utc;
    case MeasureType::direction:
        return DirectionRef::j2000;
    case MeasureType::position:
        return PositionRef::itrf;
    case MeasureType::frequency:
        return FrequencyRef::rest;
    case MeasureType::doppler:
        return DopplerRef::radio;
    case MeasureType::radial_velocity:
        return RadialVelocityRef::lsrk;
    case MeasureType::baseline:
        return BaselineRef::j2000;
    case MeasureType::uvw:
        return UvwRef::j2000;
    case MeasureType::earth_magnetic:
        return EarthMagneticRef::j2000;
    }
    return EpochRef::utc; // unreachable
}

} // namespace casacore_mini
