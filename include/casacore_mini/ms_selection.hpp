// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <map>
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
/// @ingroup ms
/// @addtogroup ms
/// @{

///
/// Row selection engine for MeasurementSet data using casacore-compatible
/// expression strings.
///
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS container evaluated against the selection
///   - MsMetaData — used internally for name-to-ID resolution
///
///
///
/// `MsSelection` mirrors the public API of casacore's
/// `MSSelection` class.  It accepts human-readable expression strings
/// for up to 12 independent selection categories, evaluates them against a
/// `MeasurementSet`, and returns a populated
/// `MsSelectionResult` containing the matching row indices and the
/// resolved IDs for each category.
///
/// The 12 categories and their expression syntaxes are:
///
///   - antenna  — "0,1,2", "0~3", "0&1" (baseline), "!5" (negate)
///   - field    — "0,1", "3C273", "3C*" (glob), ">0", "<2"
///   - spw      — "0", "0:0~63" (channels), "1.4GHz" (freq), "*"
///   - scan     — "1,3,5", "1~5", "!3"
///   - time     — ">59000.0" (MJD seconds), "2020/01/01~2020/01/02"
///   - uvdist   — ">100", ">0.5km", "<50m"
///   - corr     — "XX,YY", "RR,LL", "I,Q,U,V", "0,3"
///   - state    — "0,1", "*REFERENCE*"
///   - observation — "0,1", "0~3"
///   - array    — "0", "0,1"
///   - feed     — "0", "0&1" (feed pair)
///   - taql     — raw TaQL WHERE clause fragment
///
///
/// All active categories are combined with AND logic: only rows that pass
/// every set category are included in `MsSelectionResult::rows`.
///
/// The `to_taql_where()` method returns an equivalent TaQL WHERE
/// clause string that can be used with `taql_execute()` for deferred
/// or server-side evaluation.
///
/// Error handling is controlled by `ErrorMode`: in
/// `ThrowOnError` mode a malformed expression raises
/// `std::runtime_error`; in `CollectErrors` mode errors are
/// appended to `MsSelectionResult::errors` and evaluation continues.
///
///
/// @par Example
/// Select baselines between antennas 0–2 from scans 1 through 5:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   MsSelection sel;
///   sel.set_antenna_expr("0,1,2");
///   sel.set_scan_expr("1~5");
///   auto result = sel.evaluate(ms);
///
///   std::cout << result.rows.size() << " rows selected\n";
/// @endcode
///
///
/// @par Example
/// Select a specific field by name and a frequency range:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   MsSelection sel;
///   sel.set_field_expr("3C286");
///   sel.set_spw_expr("0:0~63");   // SPW 0, channels 0–63
///   auto result = sel.evaluate(ms);
///
///   // Convert to a TaQL WHERE clause for use elsewhere
///   std::string where = sel.to_taql_where(ms);
/// @endcode
///
///
/// @par Example
/// Use CollectErrors mode to accumulate non-fatal parsing warnings:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   MsSelection sel;
///   sel.set_error_mode(ErrorMode::CollectErrors);
///   sel.set_antenna_expr("99");   // antenna 99 does not exist
///   auto result = sel.evaluate(ms);
///
///   for (const auto& err : result.errors)
///       std::cerr << "warn: " << err << "\n";
/// @endcode
///
///
/// @par Motivation
/// Providing an MSSelection-compatible API lets existing casacore workflows
/// be ported to casacore-mini with minimal expression-string changes.  The
/// parsed result struct exposes the resolved ID lists so that downstream code
/// does not need to re-parse the expression strings.
///

/// Parse mode: controls when expression validation occurs.
enum class ParseMode {
    ParseNow,  ///< Validate at evaluate() time (default).
    ParseLate, ///< Defer validation until evaluate() or to_taql_where() is called.
};

/// Error handling mode during evaluation.
enum class ErrorMode {
    ThrowOnError,  ///< Throw std::runtime_error on malformed expressions (default).
    CollectErrors, ///< Collect errors into MsSelectionResult::errors without throwing.
};

/// UV distance unit.
enum class UvUnit { Meters, Kilometers, Wavelengths, Percent };

/// Parsed time range from a time expression.
struct TimeRange {
    double lo;       ///< Lower bound in MJD seconds (-1e30 = open).
    double hi;       ///< Upper bound in MJD seconds (+1e30 = open).
    bool is_seconds; ///< True if parsed from numeric MJD seconds, false from date string.
};

/// Parsed UV distance range.
struct UvRange {
    double lo;   ///< Lower bound in declared unit.
    double hi;   ///< Upper bound in declared unit.
    UvUnit unit; ///< Unit of the declared values.
};

/// Result of evaluating an MsSelection against a MeasurementSet.
struct MsSelectionResult {
    /// Selected main-table row indices (sorted, unique).
    std::vector<std::uint64_t> rows;
    /// Selected antenna IDs (from antenna expression, union of both sides).
    std::vector<std::int32_t> antennas;
    /// ANTENNA1-side of baseline pairs (parallel with antenna2_list).
    std::vector<std::int32_t> antenna1_list;
    /// ANTENNA2-side of baseline pairs (parallel with antenna1_list).
    std::vector<std::int32_t> antenna2_list;
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
    /// Selected feed IDs (union of both sides).
    std::vector<std::int32_t> feeds;
    /// FEED1-side of feed pairs (parallel with feed2_list).
    std::vector<std::int32_t> feed1_list;
    /// FEED2-side of feed pairs (parallel with feed1_list).
    std::vector<std::int32_t> feed2_list;
    /// Parsed time selection ranges.
    std::vector<TimeRange> time_ranges;
    /// Parsed UV distance selection ranges.
    std::vector<UvRange> uv_ranges;
    /// Selected DATA_DESC_IDs (from SPW selection).
    std::vector<std::int32_t> data_desc_ids;
    /// DDID → SPW_ID map.
    std::map<std::int32_t, std::int32_t> ddid_to_spw;
    /// DDID → POLARIZATION_ID map.
    std::map<std::int32_t, std::int32_t> ddid_to_pol_id;
    /// Errors collected in CollectErrors mode.
    std::vector<std::string> errors;
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
    /// Syntax: "0", "0,1", "3C273" (by name), "3C*" (glob), "!0" (negate),
    /// "0~1" (range), ">0" or "<2" (bounds).
    void set_field_expr(std::string_view expr);

    /// Set spectral window / channel selection expression.
    /// Syntax: "0", "0,1", "0:0~63" (spw:channels), "0:0~63^2" (stride),
    /// "SPW0" (by name), "1.4GHz" (by frequency), "*" (all).
    void set_spw_expr(std::string_view expr);

    /// Set scan number selection expression.
    /// Syntax: "1", "1,3,5", "1~5" (range), "!3" (negate), "<5", ">3".
    void set_scan_expr(std::string_view expr);

    /// Set time selection expression.
    /// Syntax: ">59000.0" (MJD seconds), "59000.0~59001.0" (range),
    /// ">2020/01/01" (date string), "2020/01/01~2020/01/02/12:00:00".
    void set_time_expr(std::string_view expr);

    /// Set UV-distance selection expression.
    /// Syntax: ">100", ">0.5km", "<50m", "100~500" (meters).
    void set_uvdist_expr(std::string_view expr);

    /// Set correlation / polarization selection expression.
    /// Syntax: "XX,YY", "RR,LL", "I,Q,U,V", "0,3" (indices).
    void set_corr_expr(std::string_view expr);

    /// Set state selection expression.
    /// Syntax: "0,1", "*REFERENCE*", ">0", "<2".
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

    /// Set the parse mode (ParseNow or ParseLate).
    void set_parse_mode(ParseMode mode) noexcept;

    /// Set the error handling mode.
    void set_error_mode(ErrorMode mode) noexcept;

    /// Get the current parse mode.
    [[nodiscard]] ParseMode parse_mode() const noexcept {
        return parse_mode_;
    }

    /// Get the current error mode.
    [[nodiscard]] ErrorMode error_mode() const noexcept {
        return error_mode_;
    }

    /// Evaluate all set expressions against an MS.
    /// In ThrowOnError mode, throws std::runtime_error on malformed expressions.
    /// In CollectErrors mode, populates result.errors and continues.
    [[nodiscard]] MsSelectionResult evaluate(MeasurementSet& ms) const;

    /// Convert the current selection to an equivalent TaQL WHERE clause.
    /// This is the casacore-mini equivalent of MSSelection::toTableExprNode().
    /// The returned string can be used with taql_execute("SELECT FROM t WHERE " + expr, table).
    /// Returns an empty string if no selection is set.
    /// @param ms Reference to the MeasurementSet (needed for name→ID resolution).
    [[nodiscard]] std::string to_taql_where(MeasurementSet& ms) const;

    /// Check if any selection category has been set.
    [[nodiscard]] bool has_selection() const noexcept;

    /// Clear all selection expressions and reset modes to defaults.
    void clear() noexcept;

    /// Reset to default state (same as clear).
    void reset() noexcept;

    // --- Selected-ID accessors (populated after evaluate()) ---

    /// Get the antenna expression string (if set).
    [[nodiscard]] const std::optional<std::string>& antenna_expr() const noexcept {
        return antenna_expr_;
    }
    /// Get the field expression string (if set).
    [[nodiscard]] const std::optional<std::string>& field_expr() const noexcept {
        return field_expr_;
    }
    /// Get the spw expression string (if set).
    [[nodiscard]] const std::optional<std::string>& spw_expr() const noexcept {
        return spw_expr_;
    }
    /// Get the scan expression string (if set).
    [[nodiscard]] const std::optional<std::string>& scan_expr() const noexcept {
        return scan_expr_;
    }
    /// Get the time expression string (if set).
    [[nodiscard]] const std::optional<std::string>& time_expr() const noexcept {
        return time_expr_;
    }
    /// Get the uvdist expression string (if set).
    [[nodiscard]] const std::optional<std::string>& uvdist_expr() const noexcept {
        return uvdist_expr_;
    }
    /// Get the corr expression string (if set).
    [[nodiscard]] const std::optional<std::string>& corr_expr() const noexcept {
        return corr_expr_;
    }
    /// Get the state expression string (if set).
    [[nodiscard]] const std::optional<std::string>& state_expr() const noexcept {
        return state_expr_;
    }
    /// Get the observation expression string (if set).
    [[nodiscard]] const std::optional<std::string>& observation_expr() const noexcept {
        return observation_expr_;
    }
    /// Get the array expression string (if set).
    [[nodiscard]] const std::optional<std::string>& array_expr() const noexcept {
        return array_expr_;
    }
    /// Get the feed expression string (if set).
    [[nodiscard]] const std::optional<std::string>& feed_expr() const noexcept {
        return feed_expr_;
    }
    /// Get the taql expression string (if set).
    [[nodiscard]] const std::optional<std::string>& taql_expr() const noexcept {
        return taql_expr_;
    }

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
    ParseMode parse_mode_ = ParseMode::ParseNow;
    ErrorMode error_mode_ = ErrorMode::ThrowOnError;
};

/// @}
} // namespace casacore_mini
