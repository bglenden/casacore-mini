#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <string>

namespace casacore_mini {

/// @file
/// @brief Text summary generation for a MeasurementSet.
///
/// Produces a human-readable summary similar to casacore's `msoverview` tool.

/// Generate a text summary of a MeasurementSet.
///
/// Includes: row count, antenna list, field list, spectral window list,
/// scan numbers, observation info, and subtable row counts.
///
/// @param ms  The MeasurementSet to summarize.
/// @return  Multi-line summary string.
[[nodiscard]] std::string ms_summary(MeasurementSet& ms);

} // namespace casacore_mini
