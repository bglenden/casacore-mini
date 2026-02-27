#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MS row selection via casacore-compatible expressions.
///
/// MsSelection supports 12 selection categories that can be combined (AND logic).
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
    /// Selected observation IDs.
    std::vector<std::int32_t> observations;
    /// Selected (sub)array IDs.
    std::vector<std::int32_t> arrays;
    /// Selected feed IDs.
    std::vector<std::int32_t> feeds;
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
    /// Syntax: "0", "0,1,2", "0~3" (range), "0&1" (baseline),
    /// "0&&1" (with auto), "0&&&" (auto-only), "!5" (negate).
    void set_antenna_expr(std::string_view expr);

    /// Set field selection expression.
    /// Syntax: "0", "0,1", "3C273" (by name), "3C*" (glob pattern).
    void set_field_expr(std::string_view expr);

    /// Set spectral window / channel selection expression.
    /// Syntax: "0", "0,1", "0:0~63" (spw:channels), "*" (all).
    void set_spw_expr(std::string_view expr);

    /// Set scan number selection expression.
    /// Syntax: "1", "1,3,5", "1~5" (range), "!3" (negate), "<5", ">3".
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

    /// Set state selection expression.
    /// Syntax: "0,1" (state IDs), "*REFERENCE*" (obs_mode pattern).
    void set_state_expr(std::string_view expr);

    /// Set observation ID selection expression.
    /// Syntax: "0", "0,1", "0~3" (range), "<5", ">2".
    void set_observation_expr(std::string_view expr);

    /// Set (sub)array ID selection expression.
    /// Syntax: "0", "0,1", "0~2" (range), "<3", ">0".
    void set_array_expr(std::string_view expr);

    /// Set feed selection expression.
    /// Syntax: "0", "0,1", "0&1" (feed pair cross), "0&&1" (with auto),
    /// "0&&&" (auto-only), "!0&1" (negate pair).
    void set_feed_expr(std::string_view expr);

    /// Set raw TaQL expression for direct injection.
    /// The expression is ANDed with other category selections during evaluation.
    void set_taql_expr(std::string_view expr);

    /// Evaluate all set expressions against an MS.
    /// Returns the selected row indices and per-category results.
    /// @throws std::runtime_error on malformed expressions.
    [[nodiscard]] MsSelectionResult evaluate(MeasurementSet& ms) const;

    /// Convert the current selection to an equivalent TaQL WHERE clause.
    /// This is the casacore-mini equivalent of MSSelection::toTableExprNode().
    /// The returned string can be used with taql_execute("SELECT FROM t WHERE " + expr, table).
    /// Returns an empty string if no selection is set.
    /// @param ms Reference to the MeasurementSet (needed for name→ID resolution).
    [[nodiscard]] std::string to_taql_where(MeasurementSet& ms) const;

    /// Check if any selection category has been set.
    [[nodiscard]] bool has_selection() const noexcept;

    /// Clear all selection expressions.
    void clear() noexcept;

    /// Reset to default state (same as clear).
    void reset() noexcept;

    // --- Selected-ID accessors (populated after evaluate()) ---

    /// Get the antenna expression string (if set).
    [[nodiscard]] const std::optional<std::string>& antenna_expr() const noexcept { return antenna_expr_; }
    /// Get the field expression string (if set).
    [[nodiscard]] const std::optional<std::string>& field_expr() const noexcept { return field_expr_; }
    /// Get the spw expression string (if set).
    [[nodiscard]] const std::optional<std::string>& spw_expr() const noexcept { return spw_expr_; }
    /// Get the scan expression string (if set).
    [[nodiscard]] const std::optional<std::string>& scan_expr() const noexcept { return scan_expr_; }
    /// Get the time expression string (if set).
    [[nodiscard]] const std::optional<std::string>& time_expr() const noexcept { return time_expr_; }
    /// Get the uvdist expression string (if set).
    [[nodiscard]] const std::optional<std::string>& uvdist_expr() const noexcept { return uvdist_expr_; }
    /// Get the corr expression string (if set).
    [[nodiscard]] const std::optional<std::string>& corr_expr() const noexcept { return corr_expr_; }
    /// Get the state expression string (if set).
    [[nodiscard]] const std::optional<std::string>& state_expr() const noexcept { return state_expr_; }
    /// Get the observation expression string (if set).
    [[nodiscard]] const std::optional<std::string>& observation_expr() const noexcept { return observation_expr_; }
    /// Get the array expression string (if set).
    [[nodiscard]] const std::optional<std::string>& array_expr() const noexcept { return array_expr_; }
    /// Get the feed expression string (if set).
    [[nodiscard]] const std::optional<std::string>& feed_expr() const noexcept { return feed_expr_; }
    /// Get the taql expression string (if set).
    [[nodiscard]] const std::optional<std::string>& taql_expr() const noexcept { return taql_expr_; }

  private:
    std::optional<std::string> antenna_expr_;
    std::optional<std::string> field_expr_;
    std::optional<std::string> spw_expr_;
    std::optional<std::string> scan_expr_;
    std::optional<std::string> time_expr_;
    std::optional<std::string> uvdist_expr_;
    std::optional<std::string> corr_expr_;
    std::optional<std::string> state_expr_;
    std::optional<std::string> observation_expr_;
    std::optional<std::string> array_expr_;
    std::optional<std::string> feed_expr_;
    std::optional<std::string> taql_expr_;
};

} // namespace casacore_mini
