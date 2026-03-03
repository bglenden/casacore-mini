// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief WCS projection types and parameter validation.
///
///
///
///
/// Provides the full set of WCS (World Coordinate System) map projections
/// as defined in the FITS WCS Papers I-IV and implemented by the WCSLIB
/// library.  Each projection is identified by its canonical 3-letter FITS
/// code (e.g. "TAN", "SIN", "ZEA") and may carry a small set of numeric
/// parameters that refine the projection geometry.
///
/// The two primary types are:
/// <dl>
///   <dt>ProjectionType</dt>
///   <dd>Strongly typed enum of all 28 supported projections.</dd>
///   <dt>Projection</dt>
///   <dd>A pair of ProjectionType and a parameter vector, suitable for
///       use in DirectionCoordinate.</dd>
/// </dl>
///
/// Helper free functions convert between the enum and its FITS code string
/// and report the expected parameter count for each projection.
///
///
/// @par Example
/// @code{.cpp}
///   // Most common radio-astronomy projection
///   Projection tan_proj{ProjectionType::tan, {}};
///
///   // SIN projection with slant parameters (SIN/NCP variant)
///   Projection sin_proj{ProjectionType::sin, {0.0, 0.0}};
///
///   // Round-trip through string
///   std::string code = projection_type_to_string(ProjectionType::ait); // "AIT"
///   ProjectionType pt  = string_to_projection_type("AIT");
///   assert(pt == ProjectionType::ait);
///
///   // Parameter count
///   assert(projection_parameter_count(ProjectionType::tan) == 0);
///   assert(projection_parameter_count(ProjectionType::sin) == 2);
/// @endcode
///

/// WCS map projection types identified by their FITS 3-letter codes.
///
///
/// All 28 zenithal, cylindrical, pseudo-cylindrical, conic, and
/// HEALPix projections defined in the FITS WCS standard are represented.
/// The enumerator names are the lowercase 3-letter codes.  Use
/// `projection_type_to_string` to obtain the uppercase FITS code
/// and `string_to_projection_type` for the reverse mapping.
///
/// Common choices for radio interferometry data:
/// <dl>
///   <dt>tan</dt>    <dd>Gnomonic (tangent-plane) projection — the most common choice.</dd>
///   <dt>sin</dt>    <dd>Slant orthographic, NCP special case with parameters (0,0).</dd>
///   <dt>zea</dt>    <dd>Zenithal equal-area.</dd>
///   <dt>ait</dt>    <dd>Hammer-Aitoff all-sky projection.</dd>
///   <dt>car</dt>    <dd>Plate Carree (equirectangular).</dd>
/// </dl>
///
enum class ProjectionType : std::uint8_t {
    azp, ///< Zenithal/azimuthal perspective
    szp, ///< Slant zenithal perspective
    tan, ///< Gnomonic (tangent plane)
    stg, ///< Stereographic
    sin, ///< Slant orthographic (NCP when params are 0,0)
    arc, ///< Zenithal equidistant
    zpn, ///< Zenithal polynomial (up to 20 parameters)
    zea, ///< Zenithal equal-area
    air, ///< Airy
    cyp, ///< Cylindrical perspective
    cea, ///< Cylindrical equal-area
    car, ///< Plate Carree
    mer, ///< Mercator
    cop, ///< Conic perspective
    cod, ///< Conic equidistant
    coe, ///< Conic equal-area
    coo, ///< Conic orthomorphic
    sfl, ///< Sanson-Flamsteed
    par, ///< Parabolic
    mol, ///< Mollweide
    ait, ///< Hammer-Aitoff
    bon, ///< Bonne's equal-area
    pco, ///< Polyconic
    tsc, ///< Tangential spherical cube
    csc, ///< COBE spherical cube
    qsc, ///< Quad-cube spherical
    hpx, ///< HEALPix
    xph, ///< HEALPix polar cap
};

/// Convert a ProjectionType to its uppercase 3-letter FITS code (e.g. "TAN").
[[nodiscard]] std::string projection_type_to_string(ProjectionType p);

/// Parse a FITS 3-letter projection code to ProjectionType (case-insensitive).
///
/// @throws std::invalid_argument if the code is not recognised.
[[nodiscard]] ProjectionType string_to_projection_type(std::string_view s);

/// Return the expected parameter count for a projection type.
///
///
/// Most projections have 0 parameters and are fully defined by the WCS
/// CRVAL/CRPIX/CD keywords.  Projections with non-zero counts:
/// <dl>
///   <dt>azp</dt> <dd>2</dd>
///   <dt>szp</dt> <dd>3</dd>
///   <dt>sin</dt> <dd>2</dd>
///   <dt>air</dt> <dd>1</dd>
///   <dt>cyp</dt> <dd>2</dd>
///   <dt>cea</dt> <dd>1</dd>
///   <dt>cop/cod/coe/coo</dt> <dd>2</dd>
///   <dt>bon</dt> <dd>1</dd>
///   <dt>hpx</dt> <dd>2</dd>
///   <dt>zpn</dt> <dd>up to 20</dd>
/// </dl>
///
[[nodiscard]] std::size_t projection_parameter_count(ProjectionType p);

/// A WCS projection with type and optional numeric parameters.
///
///
/// Projection pairs a ProjectionType with its parameter vector.  The
/// parameter vector may be empty for projections that require none.  When
/// non-empty its length should match the value returned by
/// `projection_parameter_count`, though this is not enforced at
/// construction time; validation is deferred to the WCS library call site.
///
/// Projection objects compare equal when both type and all parameters match.
///
struct Projection {
    ProjectionType type = ProjectionType::sin;
    std::vector<double> parameters;

    [[nodiscard]] bool operator==(const Projection& other) const = default;
};

} // namespace casacore_mini
