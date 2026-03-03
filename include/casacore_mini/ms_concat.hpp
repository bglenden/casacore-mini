// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace casacore_mini {

/// @file
/// @brief Concatenate two MeasurementSets into one.
/// @ingroup ms
/// @addtogroup ms
/// @{

///
/// Concatenate two MeasurementSets into a new output MeasurementSet,
/// deduplicating shared subtable rows and remapping ID columns.
///
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS containers being concatenated
///   - MsWriter — used internally to write the output MS
///
///
///
/// `ms_concat()` produces a new MeasurementSet at
/// `output_path` whose main table contains all rows from both
/// `ms1` and `ms2`, in that order.
///
/// Subtable rows from `ms2` are appended to the corresponding
/// subtables from `ms1`.  Where subtable rows from both MSes share
/// the same identifying name (antenna NAME in ANTENNA; field NAME in FIELD;
/// etc.) they are deduplicated: the `ms2` row is not appended, and
/// the matching `ms1` row index is used instead.
///
/// All ID columns in the `ms2` main-table rows (ANTENNA1, ANTENNA2,
/// FIELD_ID, DATA_DESC_ID, OBSERVATION_ID, etc.) are remapped to the new
/// indices in the merged subtables.  The full old→new mapping is returned in
/// `MsConcatResult::id_remaps`, keyed by subtable name.
///
/// An exception is thrown if the two MSes have incompatible schemas (e.g.
/// one has a DATA column and the other does not).
///
///
/// @par Example
/// Concatenate two observations of the same field into one MS:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms1 = MeasurementSet::open("obs1.ms");
///   auto ms2 = MeasurementSet::open("obs2.ms");
///
///   auto result = ms_concat(ms1, ms2, "combined.ms");
///   std::cout << "output rows: " << result.row_count << "\n";
///
///   // Inspect the ANTENNA ID remapping for ms2
///   const auto& ant_remap = result.id_remaps.at("ANTENNA");
///   for (auto [old_id, new_id] : ant_remap)
///       std::cout << "ANT old=" << old_id << " -> new=" << new_id << "\n";
/// @endcode
///
///
/// @par Motivation
/// Concatenation with automatic ID remapping is a prerequisite for building
/// large MSes from many shorter observations, and for combining data from
/// different array configurations that share some antennas and fields but
/// introduce others.  Encapsulating the remapping logic here prevents
/// callers from having to manage the bookkeeping manually.
///

/// Result of an MS concatenation.
struct MsConcatResult {
    /// Number of rows in the output MS.
    std::uint64_t row_count = 0;
    /// Per-subtable: old→new ID mapping for the second MS.
    std::map<std::string, std::map<std::int32_t, std::int32_t>> id_remaps;
};

/// Concatenate two MeasurementSets into a new output MS.
///
/// The output MS contains all rows from both inputs. Subtable rows from
/// ms2 are appended to ms1's subtables, with ID columns in the main table
/// remapped accordingly. Duplicate subtable rows (same NAME for ANTENNA,
/// same NAME for FIELD, etc.) are deduplicated by name.
///
/// @param ms1  First (base) MeasurementSet.
/// @param ms2  Second MeasurementSet to append.
/// @param output_path  Path for the new concatenated MS.
/// @return  Concat result with row count and ID remapping info.
/// @throws std::runtime_error if schemas are incompatible.
[[nodiscard]] MsConcatResult ms_concat(MeasurementSet& ms1, MeasurementSet& ms2,
                                       const std::filesystem::path& output_path);

/// @}
} // namespace casacore_mini
