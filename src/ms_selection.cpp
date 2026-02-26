#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_columns.hpp"

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

bool MsSelection::has_selection() const noexcept {
    return antenna_expr_ || field_expr_ || spw_expr_ || scan_expr_ || time_expr_ || uvdist_expr_ ||
           corr_expr_ || state_expr_;
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
                // Name-based resolution via ANTENNA subtable.
                ant_ids.clear();
                MsAntennaColumns ant_cols(ms);
                for (const auto& tok : tokens) {
                    bool found = false;
                    for (std::uint64_t ai = 0; ai < ant_cols.row_count(); ++ai) {
                        if (ant_cols.name(ai) == tok) {
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

    // --- Collect selected row indices ---
    for (std::uint64_t r = 0; r < nrow; ++r) {
        if (selected[r]) {
            result.rows.push_back(r);
        }
    }

    return result;
}

} // namespace casacore_mini
