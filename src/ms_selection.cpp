#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/taql.hpp"

#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace casacore_mini {

// ---------------------------------------------------------------------------
// Helpers: expression tokenization and parsing
// ---------------------------------------------------------------------------

namespace {

/// Trim leading/trailing whitespace from a string_view.
std::string_view trim(std::string_view sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t')) {
        sv.remove_suffix(1);
    }
    return sv;
}

/// Split a string by a delimiter character, returning trimmed tokens.
std::vector<std::string> split(std::string_view sv, char delim) {
    std::vector<std::string> tokens;
    std::string_view remaining = sv;
    while (true) {
        auto pos = remaining.find(delim);
        if (pos == std::string_view::npos) {
            auto tok = trim(remaining);
            if (!tok.empty()) {
                tokens.emplace_back(tok);
            }
            break;
        }
        auto tok = trim(remaining.substr(0, pos));
        if (!tok.empty()) {
            tokens.emplace_back(tok);
        }
        remaining = remaining.substr(pos + 1);
    }
    return tokens;
}

/// Try to parse a string as an int32. Returns nullopt on failure.
std::optional<std::int32_t> try_parse_int(std::string_view sv) {
    if (sv.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t pos = 0;
        auto val = std::stoi(std::string(sv), &pos);
        if (pos != sv.size()) {
            return std::nullopt;
        }
        return static_cast<std::int32_t>(val);
    } catch (...) {
        return std::nullopt;
    }
}

/// Try to parse a string as a double. Returns nullopt on failure.
std::optional<double> try_parse_double(std::string_view sv) {
    if (sv.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t pos = 0;
        auto val = std::stod(std::string(sv), &pos);
        if (pos != sv.size()) {
            return std::nullopt;
        }
        return val;
    } catch (...) {
        return std::nullopt;
    }
}

/// Parse an integer list or range expression like "1,3,5" or "1~5".
/// Returns the set of selected integers.
std::set<std::int32_t> parse_int_list_or_range(std::string_view expr, std::string_view category) {
    std::set<std::int32_t> result;
    auto tokens = split(expr, ',');
    for (const auto& tok : tokens) {
        // Check for range "a~b".
        auto tilde = tok.find('~');
        if (tilde != std::string::npos) {
            auto lo = try_parse_int(std::string_view(tok).substr(0, tilde));
            auto hi = try_parse_int(std::string_view(tok).substr(tilde + 1));
            if (!lo || !hi) {
                throw std::runtime_error(std::string(category) + ": invalid range '" + tok + "'");
            }
            for (std::int32_t i = *lo; i <= *hi; ++i) {
                result.insert(i);
            }
        } else {
            auto val = try_parse_int(tok);
            if (!val) {
                throw std::runtime_error(std::string(category) + ": invalid ID '" + tok + "'");
            }
            result.insert(*val);
        }
    }
    return result;
}

/// Simple glob match: supports '*' as wildcard for zero or more characters.
bool glob_match(std::string_view pattern, std::string_view text) {
    std::size_t pi = 0;
    std::size_t ti = 0;
    std::size_t star_p = std::string_view::npos;
    std::size_t star_t = 0;

    while (ti < text.size()) {
        if (pi < pattern.size() && (pattern[pi] == text[ti] || pattern[pi] == '?')) {
            ++pi;
            ++ti;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_p = pi;
            star_t = ti;
            ++pi;
        } else if (star_p != std::string_view::npos) {
            pi = star_p + 1;
            ++star_t;
            ti = star_t;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }
    return pi == pattern.size();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MsSelection setters
// ---------------------------------------------------------------------------

void MsSelection::set_antenna_expr(std::string_view expr) {
    antenna_expr_ = std::string(expr);
}

void MsSelection::set_field_expr(std::string_view expr) {
    field_expr_ = std::string(expr);
}

void MsSelection::set_spw_expr(std::string_view expr) {
    spw_expr_ = std::string(expr);
}

void MsSelection::set_scan_expr(std::string_view expr) {
    scan_expr_ = std::string(expr);
}

void MsSelection::set_time_expr(std::string_view expr) {
    time_expr_ = std::string(expr);
}

void MsSelection::set_uvdist_expr(std::string_view expr) {
    uvdist_expr_ = std::string(expr);
}

void MsSelection::set_corr_expr(std::string_view expr) {
    corr_expr_ = std::string(expr);
}

void MsSelection::set_state_expr(std::string_view expr) {
    state_expr_ = std::string(expr);
}

void MsSelection::set_observation_expr(std::string_view expr) {
    observation_expr_ = std::string(expr);
}

void MsSelection::set_array_expr(std::string_view expr) {
    array_expr_ = std::string(expr);
}

void MsSelection::set_feed_expr(std::string_view expr) {
    feed_expr_ = std::string(expr);
}

void MsSelection::set_taql_expr(std::string_view expr) {
    taql_expr_ = std::string(expr);
}

bool MsSelection::has_selection() const noexcept {
    return antenna_expr_ || field_expr_ || spw_expr_ || scan_expr_ || time_expr_ || uvdist_expr_ ||
           corr_expr_ || state_expr_ || observation_expr_ || array_expr_ || feed_expr_ ||
           taql_expr_;
}

void MsSelection::clear() noexcept {
    antenna_expr_.reset();
    field_expr_.reset();
    spw_expr_.reset();
    scan_expr_.reset();
    time_expr_.reset();
    uvdist_expr_.reset();
    corr_expr_.reset();
    state_expr_.reset();
    observation_expr_.reset();
    array_expr_.reset();
    feed_expr_.reset();
    taql_expr_.reset();
}

void MsSelection::reset() noexcept {
    clear();
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

MsSelectionResult MsSelection::evaluate(MeasurementSet& ms) const {
    MsSelectionResult result;
    const auto nrow = ms.row_count();

    // Start with all rows selected.
    std::vector<bool> selected(nrow, true);

    MsMainColumns cols(ms);

    // --- Antenna selection ---
    if (antenna_expr_) {
        auto expr = trim(*antenna_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Antenna: empty expression");
        }

        // Check for negation prefix "!".
        bool negate = false;
        if (expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }

        // Check for baseline expression "a&b".
        auto amp = expr.find('&');
        if (amp != std::string_view::npos) {
            auto lhs_sv = trim(expr.substr(0, amp));
            auto rhs_sv = trim(expr.substr(amp + 1));
            if (lhs_sv.empty()) {
                throw std::runtime_error("Antenna: incomplete baseline expression");
            }

            auto lhs = try_parse_int(lhs_sv);
            if (!lhs) {
                throw std::runtime_error("Antenna: invalid antenna ID '" + std::string(lhs_sv) +
                                         "'");
            }

            bool rhs_wildcard = (rhs_sv == "*");
            std::optional<std::int32_t> rhs;
            if (!rhs_wildcard) {
                if (rhs_sv.empty()) {
                    throw std::runtime_error("Antenna: incomplete baseline expression");
                }
                rhs = try_parse_int(rhs_sv);
                if (!rhs) {
                    throw std::runtime_error("Antenna: invalid antenna ID '" + std::string(rhs_sv) +
                                             "'");
                }
            }

            result.antennas.push_back(*lhs);
            if (rhs) {
                result.antennas.push_back(*rhs);
            }

            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto a1 = cols.antenna1(r);
                auto a2 = cols.antenna2(r);
                bool match = false;
                if (rhs_wildcard) {
                    match = (a1 == *lhs || a2 == *lhs);
                } else {
                    match = (a1 == *lhs && a2 == *rhs) || (a1 == *rhs && a2 == *lhs);
                }
                if (!match) {
                    selected[r] = false;
                }
            }
        } else {
            // ID list/range, possibly by name.
            std::set<std::int32_t> ant_ids;

            // Try numeric first.
            auto tokens = split(expr, ',');
            bool all_numeric = true;
            for (const auto& tok : tokens) {
                if (tok.find('~') != std::string::npos) {
                    // Range token.
                    auto tilde = tok.find('~');
                    auto lo = try_parse_int(std::string_view(tok).substr(0, tilde));
                    auto hi = try_parse_int(std::string_view(tok).substr(tilde + 1));
                    if (!lo || !hi) {
                        throw std::runtime_error("Antenna: invalid range '" + tok + "'");
                    }
                    for (std::int32_t i = *lo; i <= *hi; ++i) {
                        ant_ids.insert(i);
                    }
                } else {
                    auto val = try_parse_int(tok);
                    if (val) {
                        if (*val < 0) {
                            throw std::runtime_error("Antenna: negative antenna ID '" + tok + "'");
                        }
                        ant_ids.insert(*val);
                    } else {
                        all_numeric = false;
                        break;
                    }
                }
            }

            if (!all_numeric) {
                // Name-based resolution via ANTENNA subtable (supports glob patterns).
                ant_ids.clear();
                MsAntennaColumns ant_cols(ms);
                for (const auto& tok : tokens) {
                    bool has_wildcard =
                        tok.find('*') != std::string::npos || tok.find('?') != std::string::npos;
                    bool found = false;
                    for (std::uint64_t ai = 0; ai < ant_cols.row_count(); ++ai) {
                        bool match = has_wildcard ? glob_match(tok, ant_cols.name(ai))
                                                  : (ant_cols.name(ai) == tok);
                        if (match) {
                            ant_ids.insert(static_cast<std::int32_t>(ai));
                            found = true;
                        }
                    }
                    if (!found) {
                        throw std::runtime_error("Antenna: name '" + tok +
                                                 "' not found in ANTENNA subtable");
                    }
                }
            }

            result.antennas.assign(ant_ids.begin(), ant_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto a1 = cols.antenna1(r);
                auto a2 = cols.antenna2(r);
                bool match = ant_ids.count(a1) > 0 || ant_ids.count(a2) > 0;
                if (negate) {
                    match = !match;
                }
                if (!match) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Field selection ---
    if (field_expr_) {
        auto expr = trim(*field_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Field: empty expression");
        }

        std::set<std::int32_t> field_ids;
        auto tokens = split(expr, ',');

        // Try numeric first.
        bool all_numeric = true;
        for (const auto& tok : tokens) {
            auto val = try_parse_int(tok);
            if (val) {
                field_ids.insert(*val);
            } else {
                all_numeric = false;
                break;
            }
        }

        if (!all_numeric) {
            // Name/glob resolution via FIELD subtable.
            field_ids.clear();
            MsFieldColumns fld_cols(ms);
            for (const auto& tok : tokens) {
                bool has_wildcard =
                    tok.find('*') != std::string::npos || tok.find('?') != std::string::npos;
                bool found = false;
                for (std::uint64_t fi = 0; fi < fld_cols.row_count(); ++fi) {
                    auto fname = fld_cols.name(fi);
                    bool match = has_wildcard ? glob_match(tok, fname) : (fname == tok);
                    if (match) {
                        field_ids.insert(static_cast<std::int32_t>(fi));
                        found = true;
                    }
                }
                if (!found) {
                    throw std::runtime_error("Field: name '" + tok +
                                             "' not found in FIELD subtable");
                }
            }
        }

        result.fields.assign(field_ids.begin(), field_ids.end());

        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (field_ids.count(cols.field_id(r)) == 0) {
                selected[r] = false;
            }
        }
    }

    // --- SPW selection ---
    if (spw_expr_) {
        auto expr = trim(*spw_expr_);
        if (expr.empty()) {
            throw std::runtime_error("SPW: empty expression");
        }

        // Build DATA_DESC_ID → SPW_ID map.
        MsDataDescColumns dd_cols(ms);
        std::vector<std::int32_t> dd_to_spw;
        dd_to_spw.reserve(dd_cols.row_count());
        for (std::uint64_t di = 0; di < dd_cols.row_count(); ++di) {
            dd_to_spw.push_back(dd_cols.spectral_window_id(di));
        }

        std::set<std::int32_t> spw_ids;
        auto tokens = split(expr, ',');

        for (const auto& tok : tokens) {
            // Check for SPW:channel syntax.
            auto colon = tok.find(':');
            std::string_view spw_part;
            if (colon != std::string::npos) {
                spw_part = std::string_view(tok).substr(0, colon);
                auto chan_part = std::string_view(tok).substr(colon + 1);
                if (chan_part.empty()) {
                    throw std::runtime_error("SPW: missing channel specification after ':' in '" +
                                             tok + "'");
                }
                // Parse channel ranges (store in result but don't filter rows by channel).
                auto chan_ranges = split(chan_part, ';');
                auto spw_val = try_parse_int(spw_part);
                if (!spw_val) {
                    throw std::runtime_error("SPW: invalid SPW ID '" + std::string(spw_part) + "'");
                }
                for (const auto& cr : chan_ranges) {
                    auto tilde = cr.find('~');
                    if (tilde != std::string::npos) {
                        auto lo = try_parse_int(std::string_view(cr).substr(0, tilde));
                        auto hi = try_parse_int(std::string_view(cr).substr(tilde + 1));
                        if (!lo || !hi) {
                            throw std::runtime_error("SPW: invalid channel range '" + cr + "'");
                        }
                        result.channel_ranges.push_back(*spw_val);
                        result.channel_ranges.push_back(*lo);
                        result.channel_ranges.push_back(*hi);
                    } else {
                        auto ch = try_parse_int(cr);
                        if (!ch) {
                            throw std::runtime_error("SPW: invalid channel '" + cr + "'");
                        }
                        result.channel_ranges.push_back(*spw_val);
                        result.channel_ranges.push_back(*ch);
                        result.channel_ranges.push_back(*ch);
                    }
                }
                spw_ids.insert(*spw_val);
            } else if (tok == "*") {
                // All SPWs.
                MsSpWindowColumns spw_cols(ms);
                for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si) {
                    spw_ids.insert(static_cast<std::int32_t>(si));
                }
            } else {
                auto val = try_parse_int(tok);
                if (!val) {
                    throw std::runtime_error("SPW: invalid SPW ID '" + tok + "'");
                }
                spw_ids.insert(*val);
            }
        }

        result.spws.assign(spw_ids.begin(), spw_ids.end());

        // Build set of DATA_DESC_IDs that map to selected SPWs.
        std::set<std::int32_t> selected_dds;
        for (std::size_t di = 0; di < dd_to_spw.size(); ++di) {
            if (spw_ids.count(dd_to_spw[di]) > 0) {
                selected_dds.insert(static_cast<std::int32_t>(di));
            }
        }

        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (selected_dds.count(cols.data_desc_id(r)) == 0) {
                selected[r] = false;
            }
        }
    }

    // --- Scan selection ---
    if (scan_expr_) {
        auto expr = trim(*scan_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Scan: empty expression");
        }

        bool negate = false;
        if (expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }

        // Check for bound operators < >
        if (!negate && (expr.front() == '<' || expr.front() == '>')) {
            char op = expr.front();
            auto val = try_parse_int(expr.substr(1));
            if (!val) {
                throw std::runtime_error("Scan: invalid bound value '" +
                                         std::string(expr.substr(1)) + "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto s = cols.scan_number(r);
                bool match = (op == '<') ? (s < *val) : (s > *val);
                if (!match) selected[r] = false;
                else result.scans.push_back(s);
            }
            // Deduplicate scans
            std::set<std::int32_t> uniq(result.scans.begin(), result.scans.end());
            result.scans.assign(uniq.begin(), uniq.end());
        } else {
            auto scan_ids = parse_int_list_or_range(expr, "Scan");
            result.scans.assign(scan_ids.begin(), scan_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                bool match = scan_ids.count(cols.scan_number(r)) > 0;
                if (negate) {
                    match = !match;
                }
                if (!match) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Time selection ---
    if (time_expr_) {
        auto expr = trim(*time_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Time: empty expression");
        }

        // Comparison operators: ">value", "<value", or range "lo~hi".
        double lo_mjd = -1e30;
        double hi_mjd = 1e30;

        if (expr.front() == '>') {
            auto val = try_parse_double(expr.substr(1));
            if (!val) {
                throw std::runtime_error("Time: invalid value '" + std::string(expr.substr(1)) +
                                         "'");
            }
            lo_mjd = *val;
        } else if (expr.front() == '<') {
            auto val = try_parse_double(expr.substr(1));
            if (!val) {
                throw std::runtime_error("Time: invalid value '" + std::string(expr.substr(1)) +
                                         "'");
            }
            hi_mjd = *val;
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo_val = try_parse_double(expr.substr(0, tilde));
                auto hi_val = try_parse_double(expr.substr(tilde + 1));
                if (!lo_val || !hi_val) {
                    throw std::runtime_error("Time: invalid range '" + std::string(expr) + "'");
                }
                lo_mjd = *lo_val;
                hi_mjd = *hi_val;
            } else {
                throw std::runtime_error("Time: expression must use >, <, or lo~hi range: '" +
                                         std::string(expr) + "'");
            }
        }

        for (std::uint64_t r = 0; r < nrow; ++r) {
            double t = cols.time(r);
            if (t <= lo_mjd || t >= hi_mjd) {
                // For '>' we want t > lo, for '<' we want t < hi,
                // for range we want lo <= t <= hi.
                bool in_range = false;
                if (expr.front() == '>') {
                    in_range = t > lo_mjd;
                } else if (expr.front() == '<') {
                    in_range = t < hi_mjd;
                } else {
                    in_range = t >= lo_mjd && t <= hi_mjd;
                }
                if (!in_range) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- UV-distance selection ---
    if (uvdist_expr_) {
        auto expr = trim(*uvdist_expr_);
        if (expr.empty()) {
            throw std::runtime_error("UVdist: empty expression");
        }

        double lo_uv = 0.0;
        double hi_uv = 1e30;

        if (expr.front() == '>') {
            auto val = try_parse_double(expr.substr(1));
            if (!val) {
                throw std::runtime_error("UVdist: invalid value '" + std::string(expr.substr(1)) +
                                         "'");
            }
            lo_uv = *val;
        } else if (expr.front() == '<') {
            auto val = try_parse_double(expr.substr(1));
            if (!val) {
                throw std::runtime_error("UVdist: invalid value '" + std::string(expr.substr(1)) +
                                         "'");
            }
            hi_uv = *val;
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo_val = try_parse_double(expr.substr(0, tilde));
                auto hi_val = try_parse_double(expr.substr(tilde + 1));
                if (!lo_val || !hi_val) {
                    throw std::runtime_error("UVdist: invalid range '" + std::string(expr) + "'");
                }
                lo_uv = *lo_val;
                hi_uv = *hi_val;
            } else {
                throw std::runtime_error("UVdist: expression must use >, <, or lo~hi range: '" +
                                         std::string(expr) + "'");
            }
        }

        for (std::uint64_t r = 0; r < nrow; ++r) {
            auto uvw_val = cols.uvw(r);
            double uvdist = 0.0;
            if (uvw_val.size() >= 2) {
                uvdist = std::sqrt(uvw_val[0] * uvw_val[0] + uvw_val[1] * uvw_val[1]);
            }

            bool in_range = false;
            if (expr.front() == '>') {
                in_range = uvdist > lo_uv;
            } else if (expr.front() == '<') {
                in_range = uvdist < hi_uv;
            } else {
                in_range = uvdist >= lo_uv && uvdist <= hi_uv;
            }
            if (!in_range) {
                selected[r] = false;
            }
        }
    }

    // --- Correlation selection ---
    if (corr_expr_) {
        auto expr = trim(*corr_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Correlation: empty expression");
        }
        auto tokens = split(expr, ',');
        result.correlations = tokens;
        // Correlation selection narrows the correlation axis, not rows.
        // No row filtering needed here.
    }

    // --- State selection ---
    if (state_expr_) {
        auto expr = trim(*state_expr_);
        if (expr.empty()) {
            throw std::runtime_error("State: empty expression");
        }

        std::set<std::int32_t> state_ids;

        // Check if it's a pattern match (contains '*' or '?').
        bool is_pattern =
            expr.find('*') != std::string_view::npos || expr.find('?') != std::string_view::npos;

        if (is_pattern) {
            // Match against OBS_MODE in STATE subtable.
            auto& state_table = ms.subtable("STATE");
            for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
                auto obs_mode_val = state_table.read_scalar_cell("OBS_MODE", si);
                if (auto* sp = std::get_if<std::string>(&obs_mode_val)) {
                    if (glob_match(std::string(expr), *sp)) {
                        state_ids.insert(static_cast<std::int32_t>(si));
                    }
                }
            }
        } else {
            // Numeric IDs.
            state_ids = parse_int_list_or_range(expr, "State");
        }

        result.states.assign(state_ids.begin(), state_ids.end());

        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (state_ids.count(cols.state_id(r)) == 0) {
                selected[r] = false;
            }
        }
    }

    // --- Observation selection ---
    if (observation_expr_) {
        auto expr = trim(*observation_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Observation: empty expression");
        }

        // Bound operators < >
        if (expr.front() == '<' || expr.front() == '>') {
            char op = expr.front();
            auto val = try_parse_int(expr.substr(1));
            if (!val) {
                throw std::runtime_error("Observation: invalid bound value '" +
                                         std::string(expr.substr(1)) + "'");
            }
            std::set<std::int32_t> matched;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto oid = cols.observation_id(r);
                bool match = (op == '<') ? (oid < *val) : (oid > *val);
                if (!match) { // NOLINT(bugprone-branch-clone)
                    selected[r] = false;
                } else {
                    matched.insert(oid);
                }
            }
            result.observations.assign(matched.begin(), matched.end());
        } else {
            auto obs_ids = parse_int_list_or_range(expr, "Observation");
            result.observations.assign(obs_ids.begin(), obs_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (obs_ids.count(cols.observation_id(r)) == 0) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Array (subarray) selection ---
    if (array_expr_) {
        auto expr = trim(*array_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Array: empty expression");
        }

        // Bound operators < >
        if (expr.front() == '<' || expr.front() == '>') {
            char op = expr.front();
            auto val = try_parse_int(expr.substr(1));
            if (!val) {
                throw std::runtime_error("Array: invalid bound value '" +
                                         std::string(expr.substr(1)) + "'");
            }
            std::set<std::int32_t> matched;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto aid = cols.array_id(r);
                bool match = (op == '<') ? (aid < *val) : (aid > *val);
                if (!match) { // NOLINT(bugprone-branch-clone)
                    selected[r] = false;
                } else {
                    matched.insert(aid);
                }
            }
            result.arrays.assign(matched.begin(), matched.end());
        } else {
            auto arr_ids = parse_int_list_or_range(expr, "Array");
            result.arrays.assign(arr_ids.begin(), arr_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (arr_ids.count(cols.array_id(r)) == 0) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Feed selection ---
    if (feed_expr_) {
        auto expr = trim(*feed_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Feed: empty expression");
        }

        // Check for negation prefix "!".
        bool negate = false;
        if (expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }

        // Check for feed pair expression "f1&f2", "f1&&f2", "f1&&&".
        auto amp = expr.find('&');
        if (amp != std::string_view::npos) {
            auto lhs_sv = trim(expr.substr(0, amp));
            auto rest = expr.substr(amp + 1);

            // Detect &&& (auto-only), && (with auto), & (cross-only)
            bool auto_only = false;
            bool with_auto = false;
            if (rest.size() >= 2 && rest[0] == '&' && rest[1] == '&') {
                auto_only = true;
                rest = trim(rest.substr(2));
            } else if (!rest.empty() && rest[0] == '&') {
                with_auto = true;
                rest = trim(rest.substr(1));
            }

            auto lhs = try_parse_int(lhs_sv);
            if (!lhs) {
                throw std::runtime_error("Feed: invalid feed ID '" + std::string(lhs_sv) + "'");
            }

            result.feeds.push_back(*lhs);

            if (auto_only) {
                // Self-feeds only: f1 == f2 == lhs
                for (std::uint64_t r = 0; r < nrow; ++r) {
                    bool match = (cols.feed1(r) == *lhs && cols.feed2(r) == *lhs);
                    if (negate) match = !match;
                    if (!match) selected[r] = false;
                }
            } else if (rest.empty() || rest == "*") {
                // Wildcard: any row containing lhs as feed1 or feed2
                for (std::uint64_t r = 0; r < nrow; ++r) {
                    bool match = (cols.feed1(r) == *lhs || cols.feed2(r) == *lhs);
                    if (negate) match = !match;
                    if (!match) selected[r] = false;
                }
            } else {
                // Specific pair
                auto rhs = try_parse_int(rest);
                if (!rhs) {
                    throw std::runtime_error("Feed: invalid feed ID '" + std::string(rest) + "'");
                }
                result.feeds.push_back(*rhs);

                for (std::uint64_t r = 0; r < nrow; ++r) {
                    auto f1 = cols.feed1(r);
                    auto f2 = cols.feed2(r);
                    bool match = false;
                    if (with_auto) {
                        // Cross + auto: (f1,f2) pair plus self-correlations
                        match = (f1 == *lhs && f2 == *rhs) || (f1 == *rhs && f2 == *lhs) ||
                                (f1 == *lhs && f2 == *lhs) || (f1 == *rhs && f2 == *rhs);
                    } else {
                        // Cross-only
                        match = (f1 == *lhs && f2 == *rhs) || (f1 == *rhs && f2 == *lhs);
                    }
                    if (negate) match = !match;
                    if (!match) selected[r] = false;
                }
            }
        } else {
            // Simple feed ID list/range
            auto feed_ids = parse_int_list_or_range(expr, "Feed");
            result.feeds.assign(feed_ids.begin(), feed_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                bool match = feed_ids.count(cols.feed1(r)) > 0 || feed_ids.count(cols.feed2(r)) > 0;
                if (negate) match = !match;
                if (!match) selected[r] = false;
            }
        }
    }

    // --- TaQL expression injection ---
    if (taql_expr_) {
        auto expr_str = trim(*taql_expr_);
        if (expr_str.empty()) {
            throw std::runtime_error("TaQL: empty expression");
        }

        // Build a SELECT WHERE query and execute it via taql_execute.
        auto query = "SELECT FROM t WHERE " + std::string(expr_str);
        auto& main_table = ms.main_table();
        auto taql_result = taql_execute(query, main_table);

        // Convert the TaQL result rows to a set for fast lookup.
        std::set<std::uint64_t> taql_rows(taql_result.rows.begin(), taql_result.rows.end());

        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (taql_rows.count(r) == 0) {
                selected[r] = false;
            }
        }
    }

    // --- Collect selected row indices ---
    for (std::uint64_t r = 0; r < nrow; ++r) {
        if (selected[r]) {
            result.rows.push_back(r);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// to_taql_where: convert selection to TaQL WHERE clause
// ---------------------------------------------------------------------------

namespace {

/// Convert an int list/range expression to a TaQL IN clause.
/// e.g., "1,3,5" -> "COL IN [1,3,5]", "1~5" -> "COL IN [1,2,3,4,5]"
std::string int_expr_to_taql(std::string_view expr, std::string_view column,
                             std::string_view category, bool negate = false) {
    auto ids = parse_int_list_or_range(expr, category);
    if (ids.empty()) return {};
    std::string clause = std::string(column) + " IN [";
    bool first = true;
    for (auto id : ids) {
        if (!first) clause += ", ";
        clause += std::to_string(id);
        first = false;
    }
    clause += "]";
    if (negate) clause = "NOT (" + clause + ")";
    return clause;
}

/// Convert a bound expression (">N" or "<N") to a TaQL comparison.
std::string bound_to_taql(std::string_view expr, std::string_view column,
                          std::string_view category) {
    char op = expr.front();
    auto val = try_parse_int(expr.substr(1));
    if (!val) {
        throw std::runtime_error(std::string(category) + ": invalid bound value '" +
                                 std::string(expr.substr(1)) + "'");
    }
    return std::string(column) + (op == '<' ? " < " : " > ") + std::to_string(*val);
}

/// Convert a double bound/range to a TaQL clause.
std::string double_range_to_taql(std::string_view expr, std::string_view column,
                                 std::string_view category) {
    if (expr.front() == '>') {
        auto val = try_parse_double(expr.substr(1));
        if (!val) throw std::runtime_error(std::string(category) + ": invalid value");
        return std::string(column) + " > " + std::to_string(*val);
    }
    if (expr.front() == '<') {
        auto val = try_parse_double(expr.substr(1));
        if (!val) throw std::runtime_error(std::string(category) + ": invalid value");
        return std::string(column) + " < " + std::to_string(*val);
    }
    auto tilde = expr.find('~');
    if (tilde != std::string_view::npos) {
        auto lo = try_parse_double(expr.substr(0, tilde));
        auto hi = try_parse_double(expr.substr(tilde + 1));
        if (!lo || !hi) throw std::runtime_error(std::string(category) + ": invalid range");
        return std::string(column) + " >= " + std::to_string(*lo) + " AND " +
               std::string(column) + " <= " + std::to_string(*hi);
    }
    throw std::runtime_error(std::string(category) + ": unsupported expression for TaQL conversion");
}

} // anonymous namespace

std::string MsSelection::to_taql_where(MeasurementSet& ms) const {
    std::vector<std::string> clauses;

    // Antenna
    if (antenna_expr_) {
        auto expr = trim(*antenna_expr_);
        bool negate = false;
        if (!expr.empty() && expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }
        auto amp = expr.find('&');
        if (amp != std::string_view::npos) {
            auto lhs_sv = trim(expr.substr(0, amp));
            auto rhs_sv = trim(expr.substr(amp + 1));
            auto lhs = try_parse_int(lhs_sv);
            if (!lhs) throw std::runtime_error("Antenna: invalid antenna ID in TaQL conversion");
            if (rhs_sv == "*") {
                auto c = "(ANTENNA1 = " + std::to_string(*lhs) + " OR ANTENNA2 = " +
                         std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else {
                auto rhs = try_parse_int(rhs_sv);
                if (!rhs) throw std::runtime_error("Antenna: invalid antenna ID in TaQL conversion");
                auto c = "((ANTENNA1 = " + std::to_string(*lhs) + " AND ANTENNA2 = " +
                         std::to_string(*rhs) + ") OR (ANTENNA1 = " + std::to_string(*rhs) +
                         " AND ANTENNA2 = " + std::to_string(*lhs) + "))";
                clauses.push_back(negate ? ("NOT " + c) : c);
            }
        } else {
            // ID list/range — resolve names if needed
            bool all_numeric = true;
            auto tokens = split(expr, ',');
            for (const auto& tok : tokens) {
                if (tok.find('~') == std::string::npos && !try_parse_int(tok)) {
                    all_numeric = false;
                    break;
                }
            }
            if (all_numeric) {
                auto ids = parse_int_list_or_range(expr, "Antenna");
                std::string in_list;
                bool first = true;
                for (auto id : ids) {
                    if (!first) in_list += ", ";
                    in_list += std::to_string(id);
                    first = false;
                }
                auto c = "(ANTENNA1 IN [" + in_list + "] OR ANTENNA2 IN [" + in_list + "])";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else {
                // Name-based: resolve via subtable then use IDs
                MsAntennaColumns ant_cols(ms);
                std::set<std::int32_t> ant_ids;
                for (const auto& tok : tokens) {
                    for (std::uint64_t ai = 0; ai < ant_cols.row_count(); ++ai) {
                        bool has_wc = tok.find('*') != std::string::npos || tok.find('?') != std::string::npos;
                        bool match = has_wc ? glob_match(tok, ant_cols.name(ai)) : (ant_cols.name(ai) == tok);
                        if (match) ant_ids.insert(static_cast<std::int32_t>(ai));
                    }
                }
                std::string in_list;
                bool first = true;
                for (auto id : ant_ids) {
                    if (!first) in_list += ", ";
                    in_list += std::to_string(id);
                    first = false;
                }
                auto c = "(ANTENNA1 IN [" + in_list + "] OR ANTENNA2 IN [" + in_list + "])";
                clauses.push_back(negate ? ("NOT " + c) : c);
            }
        }
    }

    // Field
    if (field_expr_) {
        auto expr = trim(*field_expr_);
        auto tokens = split(expr, ',');
        bool all_numeric = true;
        for (const auto& tok : tokens) {
            if (!try_parse_int(tok)) { all_numeric = false; break; }
        }
        if (all_numeric) {
            clauses.push_back(int_expr_to_taql(expr, "FIELD_ID", "Field"));
        } else {
            MsFieldColumns fld_cols(ms);
            std::set<std::int32_t> fld_ids;
            for (const auto& tok : tokens) {
                bool has_wc = tok.find('*') != std::string::npos || tok.find('?') != std::string::npos;
                for (std::uint64_t fi = 0; fi < fld_cols.row_count(); ++fi) {
                    bool match = has_wc ? glob_match(tok, fld_cols.name(fi)) : (fld_cols.name(fi) == tok);
                    if (match) fld_ids.insert(static_cast<std::int32_t>(fi));
                }
            }
            std::string in_list;
            bool first = true;
            for (auto id : fld_ids) {
                if (!first) in_list += ", ";
                in_list += std::to_string(id);
                first = false;
            }
            clauses.push_back("FIELD_ID IN [" + in_list + "]");
        }
    }

    // SPW (via DATA_DESC_ID)
    if (spw_expr_) {
        auto expr = trim(*spw_expr_);
        MsDataDescColumns dd_cols(ms);
        std::set<std::int32_t> selected_dds;

        auto tokens = split(expr, ',');
        std::set<std::int32_t> spw_ids;
        for (const auto& tok : tokens) {
            auto colon = tok.find(':');
            std::string_view spw_part = (colon != std::string::npos)
                                            ? std::string_view(tok).substr(0, colon)
                                            : std::string_view(tok);
            if (spw_part == "*") {
                MsSpWindowColumns spw_cols(ms);
                for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si)
                    spw_ids.insert(static_cast<std::int32_t>(si));
            } else {
                auto val = try_parse_int(spw_part);
                if (val) spw_ids.insert(*val);
            }
        }
        for (std::uint64_t di = 0; di < dd_cols.row_count(); ++di) {
            if (spw_ids.count(dd_cols.spectral_window_id(di)) > 0)
                selected_dds.insert(static_cast<std::int32_t>(di));
        }
        std::string in_list;
        bool first = true;
        for (auto dd : selected_dds) {
            if (!first) in_list += ", ";
            in_list += std::to_string(dd);
            first = false;
        }
        if (!in_list.empty()) clauses.push_back("DATA_DESC_ID IN [" + in_list + "]");
    }

    // Scan
    if (scan_expr_) {
        auto expr = trim(*scan_expr_);
        bool negate = false;
        if (!expr.empty() && expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }
        if (!negate && !expr.empty() && (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
            clauses.push_back(bound_to_taql(expr, "SCAN_NUMBER", "Scan"));
        } else {
            clauses.push_back(int_expr_to_taql(expr, "SCAN_NUMBER", "Scan", negate));
        }
    }

    // Time
    if (time_expr_) {
        auto expr = trim(*time_expr_);
        clauses.push_back(double_range_to_taql(expr, "TIME", "Time"));
    }

    // UVdist — requires sqrt(UVW[0]^2 + UVW[1]^2), can't express directly in TaQL easily
    // We use: sqrt(sumsqr(UVW[0:2])) as approximation
    if (uvdist_expr_) {
        auto expr = trim(*uvdist_expr_);
        // UV distance filtering is complex in TaQL; use SQRT(UVW[1]^2+UVW[2]^2)
        // For simplicity, we express it as a direct comparison.
        // This is a best-effort conversion.
        if (expr.front() == '>') {
            auto val = try_parse_double(expr.substr(1));
            if (val) clauses.push_back("sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2]) > " + std::to_string(*val));
        } else if (expr.front() == '<') {
            auto val = try_parse_double(expr.substr(1));
            if (val) clauses.push_back("sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2]) < " + std::to_string(*val));
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo = try_parse_double(expr.substr(0, tilde));
                auto hi = try_parse_double(expr.substr(tilde + 1));
                if (lo && hi) {
                    auto uv_expr = "sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2])";
                    clauses.push_back(std::string(uv_expr) + " >= " + std::to_string(*lo) +
                                      " AND " + std::string(uv_expr) + " <= " + std::to_string(*hi));
                }
            }
        }
    }

    // State
    if (state_expr_) {
        auto expr = trim(*state_expr_);
        bool is_pattern = expr.find('*') != std::string_view::npos || expr.find('?') != std::string_view::npos;
        if (is_pattern) {
            // Resolve pattern to IDs via STATE subtable
            auto& state_table = ms.subtable("STATE");
            std::set<std::int32_t> state_ids;
            for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
                auto obs_mode_val = state_table.read_scalar_cell("OBS_MODE", si);
                if (auto* sp = std::get_if<std::string>(&obs_mode_val)) {
                    if (glob_match(std::string(expr), *sp))
                        state_ids.insert(static_cast<std::int32_t>(si));
                }
            }
            std::string in_list;
            bool first = true;
            for (auto id : state_ids) {
                if (!first) in_list += ", ";
                in_list += std::to_string(id);
                first = false;
            }
            if (!in_list.empty()) clauses.push_back("STATE_ID IN [" + in_list + "]");
        } else {
            clauses.push_back(int_expr_to_taql(expr, "STATE_ID", "State"));
        }
    }

    // Observation
    if (observation_expr_) {
        auto expr = trim(*observation_expr_);
        if (!expr.empty() && (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
            clauses.push_back(bound_to_taql(expr, "OBSERVATION_ID", "Observation"));
        } else {
            clauses.push_back(int_expr_to_taql(expr, "OBSERVATION_ID", "Observation"));
        }
    }

    // Array
    if (array_expr_) {
        auto expr = trim(*array_expr_);
        if (!expr.empty() && (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
            clauses.push_back(bound_to_taql(expr, "ARRAY_ID", "Array"));
        } else {
            clauses.push_back(int_expr_to_taql(expr, "ARRAY_ID", "Array"));
        }
    }

    // Feed
    if (feed_expr_) {
        auto expr = trim(*feed_expr_);
        bool negate = false;
        if (!expr.empty() && expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }
        auto amp = expr.find('&');
        if (amp != std::string_view::npos) {
            auto lhs_sv = trim(expr.substr(0, amp));
            auto lhs = try_parse_int(lhs_sv);
            if (!lhs) throw std::runtime_error("Feed: invalid feed ID in TaQL conversion");
            auto rest = expr.substr(amp + 1);
            bool auto_only = false;
            if (rest.size() >= 2 && rest[0] == '&' && rest[1] == '&') {
                auto_only = true;
                rest = trim(rest.substr(2));
            } else if (!rest.empty() && rest[0] == '&') {
                rest = trim(rest.substr(1));
            }
            if (auto_only) {
                auto c = "(FEED1 = " + std::to_string(*lhs) + " AND FEED2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else if (rest.empty() || rest == "*") {
                auto c = "(FEED1 = " + std::to_string(*lhs) + " OR FEED2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else {
                auto rhs = try_parse_int(rest);
                if (!rhs) throw std::runtime_error("Feed: invalid feed ID in TaQL conversion");
                auto c = "((FEED1 = " + std::to_string(*lhs) + " AND FEED2 = " +
                         std::to_string(*rhs) + ") OR (FEED1 = " + std::to_string(*rhs) +
                         " AND FEED2 = " + std::to_string(*lhs) + "))";
                clauses.push_back(negate ? ("NOT " + c) : c);
            }
        } else {
            auto ids = parse_int_list_or_range(expr, "Feed");
            std::string in_list;
            bool first = true;
            for (auto id : ids) {
                if (!first) in_list += ", ";
                in_list += std::to_string(id);
                first = false;
            }
            auto c = "(FEED1 IN [" + in_list + "] OR FEED2 IN [" + in_list + "])";
            clauses.push_back(negate ? ("NOT " + c) : c);
        }
    }

    // Raw TaQL
    if (taql_expr_) {
        auto expr = trim(*taql_expr_);
        if (!expr.empty()) clauses.push_back("(" + std::string(expr) + ")");
    }

    // Combine with AND
    if (clauses.empty()) return {};
    std::string result;
    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) result += " AND ";
        result += clauses[i];
    }
    return result;
}

} // namespace casacore_mini
