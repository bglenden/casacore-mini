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

/// <summary>
/// Generate a human-readable text summary of a MeasurementSet.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> MeasurementSet — the MS container to be summarised
///   <li> MsMetaData — cached metadata queries used internally
/// </prerequisite>
///
/// <synopsis>
/// <src>ms_summary()</src> produces a multi-line string that describes the
/// key properties of a MeasurementSet, analogous to the output of
/// casacore's <src>msoverview</src> tool or CASA's <src>listobs</src> task.
///
/// The summary includes:
/// <ul>
///   <li> Total main-table row count
///   <li> Antenna list (name, station, dish diameter)
///   <li> Field list (name, source ID)
///   <li> Spectral window list (name, reference frequency, number of channels)
///   <li> Unique scan numbers present in the main table
///   <li> Observation metadata (telescope name, observer, project)
///   <li> Per-subtable row counts for all subtables present on disk
/// </ul>
///
/// The function reads from the MeasurementSet's subtables using the public
/// table accessor API; it does not use storage manager internals directly.
/// </synopsis>
///
/// <example>
/// Print a summary of an open MS to stdout:
/// <srcblock>
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   std::cout << ms_summary(ms);
/// </srcblock>
/// </example>
///
/// <example>
/// Capture the summary string and search it for a keyword:
/// <srcblock>
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   std::string summary = ms_summary(ms);
///   bool has_vla = summary.find("VLA") != std::string::npos;
/// </srcblock>
/// </example>

/// Generate a text summary of a MeasurementSet.
///
/// Includes: row count, antenna list, field list, spectral window list,
/// scan numbers, observation info, and subtable row counts.
///
/// @param ms  The MeasurementSet to summarize.
/// @return  Multi-line summary string.
[[nodiscard]] std::string ms_summary(MeasurementSet& ms);

} // namespace casacore_mini
