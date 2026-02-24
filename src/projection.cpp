#include "casacore_mini/projection.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

struct ProjEntry {
    ProjectionType type;
    const char* code;
    std::size_t n_params; // typical/max parameter count
};

// NOLINTBEGIN(bugprone-branch-clone)
constexpr ProjEntry kProjs[] = {
    {ProjectionType::azp, "AZP", 2},  {ProjectionType::szp, "SZP", 3},
    {ProjectionType::tan, "TAN", 0},  {ProjectionType::stg, "STG", 0},
    {ProjectionType::sin, "SIN", 2},  {ProjectionType::arc, "ARC", 0},
    {ProjectionType::zpn, "ZPN", 20}, {ProjectionType::zea, "ZEA", 0},
    {ProjectionType::air, "AIR", 1},  {ProjectionType::cyp, "CYP", 2},
    {ProjectionType::cea, "CEA", 1},  {ProjectionType::car, "CAR", 0},
    {ProjectionType::mer, "MER", 0},  {ProjectionType::cop, "COP", 2},
    {ProjectionType::cod, "COD", 2},  {ProjectionType::coe, "COE", 2},
    {ProjectionType::coo, "COO", 2},  {ProjectionType::sfl, "SFL", 0},
    {ProjectionType::par, "PAR", 0},  {ProjectionType::mol, "MOL", 0},
    {ProjectionType::ait, "AIT", 0},  {ProjectionType::bon, "BON", 1},
    {ProjectionType::pco, "PCO", 0},  {ProjectionType::tsc, "TSC", 0},
    {ProjectionType::csc, "CSC", 0},  {ProjectionType::qsc, "QSC", 0},
    {ProjectionType::hpx, "HPX", 2},  {ProjectionType::xph, "XPH", 0},
};
// NOLINTEND(bugprone-branch-clone)

} // namespace

std::string projection_type_to_string(ProjectionType p) {
    for (const auto& e : kProjs) {
        if (e.type == p) {
            return e.code;
        }
    }
    return "???";
}

ProjectionType string_to_projection_type(std::string_view s) {
    std::string upper(s);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    for (const auto& e : kProjs) {
        if (upper == e.code) {
            return e.type;
        }
    }
    throw std::invalid_argument("Unknown projection type: " + std::string(s));
}

std::size_t projection_parameter_count(ProjectionType p) {
    for (const auto& e : kProjs) {
        if (e.type == p) {
            return e.n_params;
        }
    }
    return 0;
}

} // namespace casacore_mini
