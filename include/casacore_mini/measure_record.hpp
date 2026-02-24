#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Measure serialization to/from Record in MeasureHolder format.

/// Convert a Measure to a Record matching casacore's MeasureHolder format.
///
/// Record layout:
/// - `type`: string (lowercase measure type name)
/// - `refer`: string (reference frame name)
/// - `m0`: sub-record `{value: double, unit: string}` (Quantity)
/// - `m1`: optional sub-record (2+ component measures)
/// - `m2`: optional sub-record (3-component measures)
/// - `offset`: optional sub-record (recursive Measure for offset)
[[nodiscard]] Record measure_to_record(const Measure& m);

/// Parse a Record in MeasureHolder format back to a Measure.
///
/// @throws std::invalid_argument on missing/invalid fields, unknown type/refer,
///         wrong value count, or corrupt nested offset.
[[nodiscard]] Measure measure_from_record(const Record& rec);

} // namespace casacore_mini
