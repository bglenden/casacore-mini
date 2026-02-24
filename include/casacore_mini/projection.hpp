#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief WCS projection types and parameter validation.

/// WCS map projection types (FITS 3-letter codes).
enum class ProjectionType : std::uint8_t {
    azp,
    szp,
    tan,
    stg,
    sin,
    arc,
    zpn,
    zea,
    air,
    cyp,
    cea,
    car,
    mer,
    cop,
    cod,
    coe,
    coo,
    sfl,
    par,
    mol,
    ait,
    bon,
    pco,
    tsc,
    csc,
    qsc,
    hpx,
    xph,
};

/// Convert ProjectionType to its 3-letter FITS code (uppercase, e.g. "TAN").
[[nodiscard]] std::string projection_type_to_string(ProjectionType p);
/// Parse a FITS 3-letter projection code to ProjectionType. Case-insensitive.
/// @throws std::invalid_argument if unrecognized.
[[nodiscard]] ProjectionType string_to_projection_type(std::string_view s);

/// Return the expected parameter count for a projection type.
/// Most projections have 0 parameters. SIN has 2, ZPN has up to 20, etc.
[[nodiscard]] std::size_t projection_parameter_count(ProjectionType p);

/// A WCS projection with type and optional parameters.
struct Projection {
    ProjectionType type = ProjectionType::sin;
    std::vector<double> parameters;

    [[nodiscard]] bool operator==(const Projection& other) const = default;
};

} // namespace casacore_mini
