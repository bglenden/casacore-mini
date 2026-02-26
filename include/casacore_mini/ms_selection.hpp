#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MS row selection via simplified casacore-compatible expressions.
///
/// MsSelection supports 8 selection categories that can be combined (AND logic).
/// Each category filters the row set by a different criterion. The final result
/// is the intersection of all active category selections.

/// Result of evaluating an MsSelection against a MeasurementSet.
struct MsSelectionResult {
    /// Selected main-table row indices (sorted, unique).
    std::vector<std::uint64_t> rows;
    /// Selected antenna IDs (from antenna expression).
    std::vector<std::int32_t> antennas;
    /// Selected field IDs (from field expression).
    std::vector<std::int32_t> fields;
    /// Selected spectral window IDs (from spw expression).
    std::vector<std::int32_t> spws;
    /// Selected channel ranges per SPW: {spw_id, chan_start, chan_end} triples.
    std::vector<std::int32_t> channel_ranges;
    /// Selected scan numbers.
    std::vector<std::int32_t> scans;
    /// Selected state IDs.
    std::vector<std::int32_t> states;
    /// Selected correlation names or indices.
    std::vector<std::string> correlations;
};

/// Row selection engine for MeasurementSet data.
///
/// Usage:
/// @code
///   MsSelection sel;
///   sel.set_antenna_expr("0,1,2");
///   sel.set_scan_expr("1~5");
///   auto result = sel.evaluate(ms);
///   // result.rows contains row indices matching both criteria
/// @endcode
class MsSelection {
  public:
    /// Set antenna selection expression.
    /// Syntax: "0", "0,1,2", "0~3" (range), "0&1" (baseline), "!5" (negate).
    void set_antenna_expr(std::string_view expr);

    /// Set field selection expression.
    /// Syntax: "0", "0,1", "3C273" (by name), "3C*" (glob pattern).
    void set_field_expr(std::string_view expr);

    /// Set spectral window / channel selection expression.
    /// Syntax: "0", "0,1", "0:0~63" (spw:channels), "*" (all).
    void set_spw_expr(std::string_view expr);

    /// Set scan number selection expression.
    /// Syntax: "1", "1,3,5", "1~5" (range), "!3" (negate).
    void set_scan_expr(std::string_view expr);

    /// Set time selection expression.
    /// Syntax: ">59000.0" (MJD), "59000.0~59001.0" (MJD range).
    void set_time_expr(std::string_view expr);

    /// Set UV-distance selection expression.
    /// Syntax: ">100", "100~500", "<50" (meters).
    void set_uvdist_expr(std::string_view expr);

    /// Set correlation / polarization selection expression.
    /// Syntax: "XX,YY", "RR,LL", "I,Q,U,V", "0,3" (indices).
    void set_corr_expr(std::string_view expr);

    /// Set state/observation/array selection expression.
    /// Syntax: "0,1" (state IDs), "*REFERENCE*" (obs_mode pattern).
    void set_state_expr(std::string_view expr);

    /// Evaluate all set expressions against an MS.
    /// Returns the selected row indices and per-category results.
    /// @throws std::runtime_error on malformed expressions.
    [[nodiscard]] MsSelectionResult evaluate(MeasurementSet& ms) const;

    /// Check if any selection category has been set.
    [[nodiscard]] bool has_selection() const noexcept;

  private:
    std::optional<std::string> antenna_expr_;
    std::optional<std::string> field_expr_;
    std::optional<std::string> spw_expr_;
    std::optional<std::string> scan_expr_;
    std::optional<std::string> time_expr_;
    std::optional<std::string> uvdist_expr_;
    std::optional<std::string> corr_expr_;
    std::optional<std::string> state_expr_;
};

} // namespace casacore_mini
