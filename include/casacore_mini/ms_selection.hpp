#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
    /// Selected antenna1 IDs (from antenna expression).
    std::vector<std::int32_t> antenna1_list;
    /// Selected antenna2 IDs (from antenna expression).
    std::vector<std::int32_t> antenna2_list;
    /// Selected baselines as (ant1, ant2) pairs.
    std::vector<std::pair<std::int32_t, std::int32_t>> baseline_list;
    /// Selected antenna IDs (union of antenna1 and antenna2; backward compat).
    std::vector<std::int32_t> antennas;
    /// Selected field IDs (from field expression).
    std::vector<std::int32_t> fields;
    /// Selected spectral window IDs (from spw expression).
    std::vector<std::int32_t> spws;
    /// Selected channel ranges per SPW: {spw_id, chan_start, chan_end} triples.
    std::vector<std::int32_t> channel_ranges;
    /// Selected channel slices as (start, width) pairs.
    std::vector<std::pair<std::int32_t, std::int32_t>> chan_slices;
    /// Selected scan numbers.
    std::vector<std::int32_t> scans;
    /// Selected state IDs.
    std::vector<std::int32_t> states;
    /// Selected observation IDs.
    std::vector<std::int32_t> observation_ids;
    /// Backward-compatible alias for observation_ids.
    std::vector<std::int32_t> observations;
    /// Selected sub-array IDs.
    std::vector<std::int32_t> subarray_ids;
    /// Backward-compatible alias for subarray_ids.
    std::vector<std::int32_t> arrays;
    /// Selected feed IDs.
    std::vector<std::int32_t> feeds;
    /// Selected DATA_DESC_IDs.
    std::vector<std::int32_t> dd_ids;
    /// Selected time ranges as MJD (start, end) pairs, flattened.
    std::vector<double> time_list;
    /// Selected UV ranges as doubles, flattened.
    std::vector<double> uv_list;
    /// Selected correlation slices as (start, width) pairs.
    std::vector<std::pair<std::int32_t, std::int32_t>> corr_slices;
    /// Selected correlation names or indices.
    std::vector<std::string> correlations;
};

/// Simple error collection class for MSSelection error handling.
class MsSelectionErrorHandler {
  public:
    /// Record an error message.
    void handleError(const std::string& msg);

    /// Get all collected error messages.
    [[nodiscard]] const std::vector<std::string>& getMessages() const noexcept;

    /// Check if any errors have been recorded.
    [[nodiscard]] bool hasErrors() const noexcept;

    /// Clear all collected error messages.
    void clear() noexcept;

  private:
    std::vector<std::string> messages_;
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
    /// Syntax: "0", "0,1,2", "0~3" (range), "0&1" (baseline), "!5" (negate),
    ///         "/pattern/" (regex on antenna names).
    void set_antenna_expr(std::string_view expr);

    /// Set field selection expression.
    /// Syntax: "0", "0,1", "3C273" (by name), "3C*" (glob pattern).
    void set_field_expr(std::string_view expr);

    /// Set spectral window / channel selection expression.
    /// Syntax: "0", "0,1", "0:0~63" (spw:channels), "*" (all),
    ///         "0:10~20MHz" (frequency range).
    void set_spw_expr(std::string_view expr);

    /// Set scan number selection expression.
    /// Syntax: "1", "1,3,5", "1~5" (range), "!3" (negate).
    void set_scan_expr(std::string_view expr);

    /// Set time selection expression.
    /// Syntax: ">59000.0" (MJD), "59000.0~59001.0" (MJD range),
    ///         "2024/01/15/00:00:00~2024/01/16/00:00:00" (date/time range).
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

    /// Set observation selection expression.
    /// Syntax: "0", "0,1", "0~3" (range).
    void set_observation_expr(std::string_view expr);

    /// Set feed selection expression (placeholder for extended categories).
    void set_feed_expr(std::string_view expr);

    /// Set array/sub-array selection expression.
    /// Syntax: "0", "0,1" (array IDs).
    void set_array_expr(std::string_view expr);

    /// Set intent selection expression.
    /// Syntax: "*TARGET*" (glob pattern on STATE.OBS_MODE).
    void set_intent_expr(std::string_view expr);

    /// Set polarization selection expression (alias for corr_expr).
    void set_poln_expr(std::string_view expr);

    /// Set raw TaQL expression for injection into the WHERE clause.
    void set_taql_expr(std::string_view expr);

    /// Clear all selection expressions, resetting the object to its initial state.
    void clear() noexcept;

    /// Reset all selection expressions (alias for clear).
    void reset() noexcept;

    /// Evaluate all set expressions against an MS.
    /// Returns the selected row indices and per-category results.
    /// @throws std::runtime_error on malformed expressions.
    [[nodiscard]] MsSelectionResult evaluate(MeasurementSet& ms) const;

    /// Generate a TaQL WHERE clause string from the current selection.
    /// Returns an empty string if no selection is set.
    [[nodiscard]] std::string to_taql_where(MeasurementSet& ms) const;

    /// Check if any selection category has been set.
    [[nodiscard]] bool has_selection() const noexcept;

    // --- Expression accessor methods ---

    /// Get the antenna expression, if set.
    [[nodiscard]] const std::optional<std::string>& antenna_expr() const noexcept;
    /// Get the field expression, if set.
    [[nodiscard]] const std::optional<std::string>& field_expr() const noexcept;
    /// Get the spw expression, if set.
    [[nodiscard]] const std::optional<std::string>& spw_expr() const noexcept;
    /// Get the scan expression, if set.
    [[nodiscard]] const std::optional<std::string>& scan_expr() const noexcept;
    /// Get the time expression, if set.
    [[nodiscard]] const std::optional<std::string>& time_expr() const noexcept;
    /// Get the uvdist expression, if set.
    [[nodiscard]] const std::optional<std::string>& uvdist_expr() const noexcept;
    /// Get the correlation expression, if set.
    [[nodiscard]] const std::optional<std::string>& corr_expr() const noexcept;
    /// Get the state expression, if set.
    [[nodiscard]] const std::optional<std::string>& state_expr() const noexcept;
    /// Get the observation expression, if set.
    [[nodiscard]] const std::optional<std::string>& observation_expr() const noexcept;
    /// Get the feed expression, if set.
    [[nodiscard]] const std::optional<std::string>& feed_expr() const noexcept;
    /// Get the array expression, if set.
    [[nodiscard]] const std::optional<std::string>& array_expr() const noexcept;
    /// Get the intent expression, if set.
    [[nodiscard]] const std::optional<std::string>& intent_expr() const noexcept;
    /// Get the poln expression, if set.
    [[nodiscard]] const std::optional<std::string>& poln_expr() const noexcept;
    /// Get the TaQL expression, if set.
    [[nodiscard]] const std::optional<std::string>& taql_expr() const noexcept;

    // --- Accessor methods for parsed selection data ---

    /// Get antenna1 IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getAntenna1List() const;
    /// Get antenna2 IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getAntenna2List() const;
    /// Get baselines as (ant1, ant2) pairs from the last evaluation result.
    [[nodiscard]] std::vector<std::pair<int, int>> getBaselineList() const;
    /// Get field IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getFieldList() const;
    /// Get SPW IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getSpwList() const;
    /// Get channel slices as (start, width) pairs from the last evaluation result.
    [[nodiscard]] std::vector<std::pair<int, int>> getChanSlices() const;
    /// Get scan numbers from the last evaluation result.
    [[nodiscard]] std::vector<int> getScanList() const;
    /// Get state IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getStateList() const;
    /// Get observation IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getObservationList() const;
    /// Get sub-array IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getSubArrayList() const;
    /// Get DATA_DESC_IDs from the last evaluation result.
    [[nodiscard]] std::vector<int> getDDIDList() const;
    /// Get time ranges as MJD pairs (start, end) from the last evaluation result.
    [[nodiscard]] std::vector<double> getTimeList() const;
    /// Get correlation slices as (start, width) pairs.
    [[nodiscard]] std::vector<std::pair<int, int>> getCorrSlices() const;
    /// Get UV distance ranges from the last evaluation result.
    [[nodiscard]] std::vector<double> getUVList() const;

    // --- PARSE_LATE mode ---

    /// Set PARSE_LATE mode. When true, expressions are stored but not parsed
    /// until evaluate() is called.
    void set_parse_late(bool late) noexcept;
    /// Check if PARSE_LATE mode is enabled.
    [[nodiscard]] bool parse_late() const noexcept;

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
    std::optional<std::string> feed_expr_;
    std::optional<std::string> array_expr_;
    std::optional<std::string> intent_expr_;
    std::optional<std::string> poln_expr_;
    std::optional<std::string> taql_expr_;

    bool parse_late_ = false;

    /// Cached result from the last evaluate() call, used by accessor methods.
    mutable MsSelectionResult last_result_;
};

} // namespace casacore_mini
