#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

#include <optional>
#include <string>

namespace casacore_mini {

/// @file
/// @brief Observatory/observation metadata (telescope, observer, obsdate).

/// Observation metadata typically stored in CoordinateSystem keywords.
struct ObsInfo {
    std::string telescope;
    std::string observer;
    /// Observation date as an epoch measure (optional).
    std::optional<Measure> obs_date;
    /// Telescope position as a position measure (optional).
    std::optional<Measure> telescope_position;
    /// Pointing center [lon_rad, lat_rad] (optional).
    std::optional<std::pair<double, double>> pointing_center;
};

/// Serialize ObsInfo fields into a Record.
/// Writes telescope, observer, obsdate (as measure record), etc.
void obs_info_to_record(const ObsInfo& info, Record& rec);

/// Reconstruct ObsInfo from a Record.
/// Reads telescope, observer, obsdate fields if present.
[[nodiscard]] ObsInfo obs_info_from_record(const Record& rec);

} // namespace casacore_mini
