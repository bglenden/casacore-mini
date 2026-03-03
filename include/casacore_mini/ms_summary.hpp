// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <string>

namespace casacore_mini {

/// @file
/// @brief Text summary generation for a MeasurementSet.
///
/// Produces a human-readable summary similar to casacore's `msoverview` tool.
/// @ingroup ms
/// @addtogroup ms
/// @{

///
/// Generate a human-readable text summary of a MeasurementSet.
///
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS container to be summarised
///   - MsMetaData — cached metadata queries used internally
///
///
///
/// `ms_summary()` produces a multi-line string that describes the
/// key properties of a MeasurementSet, analogous to the output of
/// casacore's `msoverview` tool or CASA's `listobs` task.
///
/// The summary includes:
///
///   - Total main-table row count
///   - Antenna list (name, station, dish diameter)
///   - Field list (name, source ID)
///   - Spectral window list (name, reference frequency, number of channels)
///   - Unique scan numbers present in the main table
///   - Observation metadata (telescope name, observer, project)
///   - Per-subtable row counts for all subtables present on disk
///
///
/// The function reads from the MeasurementSet's subtables using the public
/// table accessor API; it does not use storage manager internals directly.
///
///
/// @par Example
/// Print a summary of an open MS to stdout:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   std::cout << ms_summary(ms);
/// @endcode
///
///
/// @par Example
/// Capture the summary string and search it for a keyword:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   std::string summary = ms_summary(ms);
///   bool has_vla = summary.find("VLA") != std::string::npos;
/// @endcode
///

/// Generate a text summary of a MeasurementSet.
///
/// Includes: row count, antenna list, field list, spectral window list,
/// scan numbers, observation info, and subtable row counts.
///
/// @param ms  The MeasurementSet to summarize.
/// @return  Multi-line summary string.
[[nodiscard]] std::string ms_summary(MeasurementSet& ms);

/// @}
} // namespace casacore_mini
