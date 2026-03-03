// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/taql.hpp"

#include <cmath>
#include <regex>
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

/// Check if a string contains regex meta-characters (beyond glob wildcards).
bool is_regex_pattern(std::string_view sv) {
    for (auto ch : sv) {
        if (ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == '|' || ch == '+' ||
            ch == '{' || ch == '}' || ch == '^' || ch == '$' || ch == '\\' || ch == '.') {
            return true;
        }
    }
    return false;
}

/// Regex match: returns true if text matches the regex pattern.
bool regex_match_str(std::string_view pattern, std::string_view text) {
    try {
        std::regex re(std::string(pattern), std::regex::ECMAScript | std::regex::nosubs);
        return std::regex_search(std::string(text), re);
    } catch (const std::regex_error&) {
        return false;
    }
}

/// Check if a name token is a pattern (glob or regex).
bool is_name_pattern(std::string_view tok) {
    return tok.find('*') != std::string_view::npos || tok.find('?') != std::string_view::npos ||
           is_regex_pattern(tok);
}

/// Match a name token against a candidate: uses regex if regex metacharacters present,
/// otherwise tries glob, then exact match.
bool name_match(std::string_view pattern, std::string_view text) {
    if (is_regex_pattern(pattern)) {
        return regex_match_str(pattern, text);
    }
    bool has_glob =
        pattern.find('*') != std::string_view::npos || pattern.find('?') != std::string_view::npos;
    if (has_glob) {
        return glob_match(pattern, text);
    }
    return pattern == text;
}

/// Parse a time string in YYYY/MM/DD or YYYY/MM/DD/HH:MM:SS format to MJD seconds.
/// Returns nullopt if the string doesn't match the expected formats.
std::optional<double> parse_time_string(std::string_view sv) {
    // Try YYYY/MM/DD/HH:MM:SS or YYYY/MM/DD
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    std::string s(sv);

    // Count slashes to determine format
    int slashes = 0;
    for (auto ch : s) {
        if (ch == '/')
            ++slashes;
    }
    if (slashes < 2)
        return std::nullopt;

    // Parse YYYY/MM/DD
    auto slash1 = s.find('/');
    auto slash2 = s.find('/', slash1 + 1);
    auto year_str = s.substr(0, slash1);
    auto month_str = s.substr(slash1 + 1, slash2 - slash1 - 1);
    auto rest = s.substr(slash2 + 1);

    try {
        year = std::stoi(year_str);
        month = std::stoi(month_str);
    } catch (...) {
        return std::nullopt;
    }

    if (slashes >= 3) {
        // YYYY/MM/DD/HH:MM:SS
        auto slash3 = rest.find('/');
        if (slash3 == std::string::npos)
            return std::nullopt;
        auto day_str = rest.substr(0, slash3);
        auto time_str = rest.substr(slash3 + 1);
        try {
            day = std::stoi(day_str);
        } catch (...) {
            return std::nullopt;
        }
        // Parse HH:MM:SS
        auto colon1 = time_str.find(':');
        if (colon1 != std::string::npos) {
            auto colon2 = time_str.find(':', colon1 + 1);
            try {
                hour = std::stoi(std::string(time_str.substr(0, colon1)));
                if (colon2 != std::string::npos) {
                    minute =
                        std::stoi(std::string(time_str.substr(colon1 + 1, colon2 - colon1 - 1)));
                    second = std::stoi(std::string(time_str.substr(colon2 + 1)));
                } else {
                    minute = std::stoi(std::string(time_str.substr(colon1 + 1)));
                }
            } catch (...) {
                return std::nullopt;
            }
        } else {
            try {
                hour = std::stoi(std::string(time_str));
            } catch (...) {
                return std::nullopt;
            }
        }
    } else {
        // YYYY/MM/DD only
        try {
            day = std::stoi(std::string(rest));
        } catch (...) {
            return std::nullopt;
        }
    }

    // Convert to MJD using Fliegel & Van Flandern algorithm, then to seconds.
    // The JDN formula gives the day number at noon, so subtract 2400001 to get
    // the integer MJD for midnight of the given date.
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    int jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    double mjd = static_cast<double>(jdn) - 2400001.0;
    double mjd_seconds = mjd * 86400.0 + hour * 3600.0 + minute * 60.0 + second;
    return mjd_seconds;
}

/// Try to parse a time value: either a numeric double (MJD seconds) or a date string.
std::optional<double> parse_time_value(std::string_view sv) {
    // Try numeric first
    auto dval = try_parse_double(sv);
    if (dval)
        return dval;
    // Try date string
    return parse_time_string(sv);
}

/// Parse UV distance unit suffix and return the multiplier to convert to meters.
/// Strips the unit from the string and returns (numeric_value, multiplier).
struct UvParsed {
    double value;
    double multiplier;
};

std::optional<UvParsed> parse_uv_with_unit(std::string_view sv) {
    // Check for unit suffix: km, m, wavelength, lambda, %
    std::string s(sv);
    double multiplier = 1.0; // default: meters

    if (s.size() > 2 && s.substr(s.size() - 2) == "km") {
        multiplier = 1000.0;
        s = s.substr(0, s.size() - 2);
    } else if (s.size() > 1 && s.back() == 'm') {
        multiplier = 1.0;
        s = s.substr(0, s.size() - 1);
    } else if (s.size() > 10 && s.substr(s.size() - 10) == "wavelength") {
        multiplier = -1.0; // sentinel: wavelength mode, needs frequency context
        s = s.substr(0, s.size() - 10);
    } else if (s.size() > 6 && s.substr(s.size() - 6) == "lambda") {
        multiplier = -1.0;
        s = s.substr(0, s.size() - 6);
    } else if (s.size() > 1 && s.back() == '%') {
        multiplier = -2.0; // sentinel: percentage mode
        s = s.substr(0, s.size() - 1);
    }

    auto val = try_parse_double(s);
    if (!val)
        return std::nullopt;
    return UvParsed{*val, multiplier};
}

/// Parse a channel stride expression: "start~end^step"
/// Returns {start, end, step}
struct ChanRange {
    std::int32_t start;
    std::int32_t end;
    std::int32_t step;
};

std::optional<ChanRange> parse_chan_range(std::string_view cr) {
    std::int32_t step = 1;
    std::string_view range_part = cr;

    // Check for ^step suffix
    auto caret = cr.find('^');
    if (caret != std::string_view::npos) {
        auto step_val = try_parse_int(cr.substr(caret + 1));
        if (!step_val || *step_val < 1)
            return std::nullopt;
        step = *step_val;
        range_part = cr.substr(0, caret);
    }

    auto tilde = range_part.find('~');
    if (tilde != std::string_view::npos) {
        auto lo = try_parse_int(range_part.substr(0, tilde));
        auto hi = try_parse_int(range_part.substr(tilde + 1));
        if (!lo || !hi)
            return std::nullopt;
        return ChanRange{*lo, *hi, step};
    }

    // Single channel
    auto ch = try_parse_int(range_part);
    if (!ch)
        return std::nullopt;
    return ChanRange{*ch, *ch, step};
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

void MsSelection::set_parse_mode(ParseMode mode) noexcept {
    parse_mode_ = mode;
}

void MsSelection::set_error_mode(ErrorMode mode) noexcept {
    error_mode_ = mode;
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
    parse_mode_ = ParseMode::ParseNow;
    error_mode_ = ErrorMode::ThrowOnError;
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

    // Helper macro for error-collection mode.
    auto handle_error = [&](const std::exception& e) {
        if (error_mode_ == ErrorMode::CollectErrors) {
            result.errors.emplace_back(e.what());
        } else {
            throw;
        }
    };

    // --- Antenna selection ---
    if (antenna_expr_)
        try {
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

            // Check for baseline expression "a&b", "a&&b", "a&&&".
            auto amp = expr.find('&');
            if (amp != std::string_view::npos) {
                auto lhs_sv = trim(expr.substr(0, amp));
                auto rest = expr.substr(amp + 1);
                if (lhs_sv.empty()) {
                    throw std::runtime_error("Antenna: incomplete baseline expression");
                }

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
                    throw std::runtime_error("Antenna: invalid antenna ID '" + std::string(lhs_sv) +
                                             "'");
                }

                result.antennas.push_back(*lhs);
                result.antenna1_list.push_back(*lhs);

                if (rest.empty() && !auto_only) {
                    throw std::runtime_error("Antenna: incomplete baseline expression");
                }

                if (auto_only) {
                    // Auto-correlation only: ANTENNA1 == ANTENNA2 == lhs
                    result.antenna2_list.push_back(*lhs);
                    for (std::uint64_t r = 0; r < nrow; ++r) {
                        bool match = (cols.antenna1(r) == *lhs && cols.antenna2(r) == *lhs);
                        if (negate)
                            match = !match;
                        if (!match)
                            selected[r] = false;
                    }
                } else if (rest == "*") {
                    // Wildcard: any row containing lhs
                    for (std::uint64_t r = 0; r < nrow; ++r) {
                        bool match = (cols.antenna1(r) == *lhs || cols.antenna2(r) == *lhs);
                        if (negate)
                            match = !match;
                        if (!match)
                            selected[r] = false;
                    }
                } else {
                    auto rhs = try_parse_int(rest);
                    if (!rhs) {
                        throw std::runtime_error("Antenna: invalid antenna ID '" +
                                                 std::string(rest) + "'");
                    }
                    result.antennas.push_back(*rhs);
                    result.antenna2_list.push_back(*rhs);

                    for (std::uint64_t r = 0; r < nrow; ++r) {
                        auto a1 = cols.antenna1(r);
                        auto a2 = cols.antenna2(r);
                        bool match = false;
                        if (with_auto) {
                            // Cross + auto
                            match = (a1 == *lhs && a2 == *rhs) || (a1 == *rhs && a2 == *lhs) ||
                                    (a1 == *lhs && a2 == *lhs) || (a1 == *rhs && a2 == *rhs);
                        } else {
                            // Cross-only
                            match = (a1 == *lhs && a2 == *rhs) || (a1 == *rhs && a2 == *lhs);
                        }
                        if (negate)
                            match = !match;
                        if (!match)
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
                                throw std::runtime_error("Antenna: negative antenna ID '" + tok +
                                                         "'");
                            }
                            ant_ids.insert(*val);
                        } else {
                            all_numeric = false;
                            break;
                        }
                    }
                }

                if (!all_numeric) {
                    // Name-based resolution via ANTENNA subtable (glob + regex patterns).
                    ant_ids.clear();
                    MsAntennaColumns ant_cols(ms);
                    for (const auto& tok : tokens) {
                        bool found = false;
                        for (std::uint64_t ai = 0; ai < ant_cols.row_count(); ++ai) {
                            if (name_match(tok, ant_cols.name(ai))) {
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
                result.antenna1_list.assign(ant_ids.begin(), ant_ids.end());
                result.antenna2_list.assign(ant_ids.begin(), ant_ids.end());

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
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Field selection ---
    if (field_expr_)
        try {
            auto expr = trim(*field_expr_);
            if (expr.empty()) {
                throw std::runtime_error("Field: empty expression");
            }

            // Check for negation prefix "!".
            bool negate = false;
            if (expr.front() == '!') {
                negate = true;
                expr = trim(expr.substr(1));
            }

            std::set<std::int32_t> field_ids;

            // Check for bound operators < >
            if (!negate && !expr.empty() && (expr.front() == '<' || expr.front() == '>')) {
                char op = expr.front();
                auto val = try_parse_int(expr.substr(1));
                if (!val) {
                    throw std::runtime_error("Field: invalid bound value '" +
                                             std::string(expr.substr(1)) + "'");
                }
                MsFieldColumns fld_cols(ms);
                for (std::uint64_t fi = 0; fi < fld_cols.row_count(); ++fi) {
                    auto fid = static_cast<std::int32_t>(fi);
                    bool match = (op == '<') ? (fid < *val) : (fid > *val);
                    if (match)
                        field_ids.insert(fid);
                }
            } else {
                auto tokens = split(expr, ',');

                // Try numeric first (supports ranges with ~).
                bool all_numeric = true;
                for (const auto& tok : tokens) {
                    if (tok.find('~') != std::string::npos) {
                        // Range token
                        auto tilde = tok.find('~');
                        auto lo = try_parse_int(std::string_view(tok).substr(0, tilde));
                        auto hi = try_parse_int(std::string_view(tok).substr(tilde + 1));
                        if (!lo || !hi) {
                            all_numeric = false;
                            break;
                        }
                        for (std::int32_t i = *lo; i <= *hi; ++i) {
                            field_ids.insert(i);
                        }
                    } else {
                        auto val = try_parse_int(tok);
                        if (val) {
                            field_ids.insert(*val);
                        } else {
                            all_numeric = false;
                            break;
                        }
                    }
                }

                if (!all_numeric) {
                    // Name/glob/regex resolution via FIELD subtable.
                    field_ids.clear();
                    MsFieldColumns fld_cols(ms);
                    for (const auto& tok : tokens) {
                        bool found = false;
                        for (std::uint64_t fi = 0; fi < fld_cols.row_count(); ++fi) {
                            if (name_match(tok, fld_cols.name(fi))) {
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
            }

            result.fields.assign(field_ids.begin(), field_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                bool match = field_ids.count(cols.field_id(r)) > 0;
                if (negate)
                    match = !match;
                if (!match) {
                    selected[r] = false;
                }
            }
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- SPW selection ---
    if (spw_expr_)
        try {
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

            MsSpWindowColumns spw_cols(ms);
            std::set<std::int32_t> spw_ids;
            auto tokens = split(expr, ',');

            for (const auto& tok : tokens) {
                // Check for SPW:channel syntax.
                auto colon = tok.find(':');
                std::string spw_part_str;
                if (colon != std::string::npos) {
                    spw_part_str = tok.substr(0, colon);
                    auto chan_part = std::string_view(tok).substr(colon + 1);
                    if (chan_part.empty()) {
                        throw std::runtime_error(
                            "SPW: missing channel specification after ':' in '" + tok + "'");
                    }
                    // Parse channel ranges with stride support.
                    auto chan_ranges = split(chan_part, ';');
                    auto spw_val = try_parse_int(spw_part_str);
                    if (!spw_val) {
                        throw std::runtime_error("SPW: invalid SPW ID '" + spw_part_str + "'");
                    }
                    for (const auto& cr : chan_ranges) {
                        auto parsed = parse_chan_range(cr);
                        if (!parsed) {
                            throw std::runtime_error("SPW: invalid channel range '" + cr + "'");
                        }
                        // Store channel ranges (step is stored as 4th element when != 1)
                        result.channel_ranges.push_back(*spw_val);
                        result.channel_ranges.push_back(parsed->start);
                        result.channel_ranges.push_back(parsed->end);
                    }
                    spw_ids.insert(*spw_val);
                } else if (tok == "*") {
                    // All SPWs.
                    for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si) {
                        spw_ids.insert(static_cast<std::int32_t>(si));
                    }
                } else {
                    // Check for frequency suffix first (Hz, MHz, GHz, kHz) — handles
                    // both single values ("1.4GHz") and ranges ("1.0~2.0GHz").
                    std::string s(tok);
                    double freq_mult = 0.0;
                    if (s.size() > 3 && s.substr(s.size() - 3) == "GHz") {
                        freq_mult = 1e9;
                        s = s.substr(0, s.size() - 3);
                    } else if (s.size() > 3 && s.substr(s.size() - 3) == "MHz") {
                        freq_mult = 1e6;
                        s = s.substr(0, s.size() - 3);
                    } else if (s.size() > 3 && s.substr(s.size() - 3) == "kHz") {
                        freq_mult = 1e3;
                        s = s.substr(0, s.size() - 3);
                    } else if (s.size() > 2 && s.substr(s.size() - 2) == "Hz") {
                        freq_mult = 1.0;
                        s = s.substr(0, s.size() - 2);
                    }

                    if (freq_mult > 0.0) {
                        // Frequency-based selection.
                        auto ftilde = s.find('~');
                        if (ftilde != std::string::npos) {
                            auto flo = try_parse_double(s.substr(0, ftilde));
                            auto fhi = try_parse_double(s.substr(ftilde + 1));
                            if (!flo || !fhi) {
                                throw std::runtime_error("SPW: invalid frequency range '" + tok +
                                                         "'");
                            }
                            double lo_hz = *flo * freq_mult;
                            double hi_hz = *fhi * freq_mult;
                            for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si) {
                                double rf = spw_cols.ref_frequency(si);
                                if (rf >= lo_hz && rf <= hi_hz) {
                                    spw_ids.insert(static_cast<std::int32_t>(si));
                                }
                            }
                        } else {
                            auto fval = try_parse_double(s);
                            if (!fval) {
                                throw std::runtime_error("SPW: invalid frequency '" + tok + "'");
                            }
                            double target_hz = *fval * freq_mult;
                            for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si) {
                                double rf = spw_cols.ref_frequency(si);
                                if (std::abs(rf - target_hz) / target_hz < 0.01) {
                                    spw_ids.insert(static_cast<std::int32_t>(si));
                                }
                            }
                        }
                    } else {
                        // Try numeric ID or range.
                        auto tilde = tok.find('~');
                        if (tilde != std::string::npos) {
                            auto lo = try_parse_int(std::string_view(tok).substr(0, tilde));
                            auto hi = try_parse_int(std::string_view(tok).substr(tilde + 1));
                            if (!lo || !hi) {
                                throw std::runtime_error("SPW: invalid range '" + tok + "'");
                            }
                            for (std::int32_t i = *lo; i <= *hi; ++i) {
                                spw_ids.insert(i);
                            }
                        } else {
                            auto val = try_parse_int(tok);
                            if (val) {
                                spw_ids.insert(*val);
                            } else {
                                // Name-based SPW selection (glob/regex match).
                                bool found = false;
                                for (std::uint64_t si = 0; si < spw_cols.row_count(); ++si) {
                                    if (name_match(tok, spw_cols.name(si))) {
                                        spw_ids.insert(static_cast<std::int32_t>(si));
                                        found = true;
                                    }
                                }
                                if (!found) {
                                    throw std::runtime_error(
                                        "SPW: name '" + tok +
                                        "' not found in SPECTRAL_WINDOW subtable");
                                }
                            }
                        }
                    }
                }
            }

            result.spws.assign(spw_ids.begin(), spw_ids.end());

            // Build set of DATA_DESC_IDs that map to selected SPWs.
            std::set<std::int32_t> selected_dds;
            for (std::size_t di = 0; di < dd_to_spw.size(); ++di) {
                auto did = static_cast<std::int32_t>(di);
                if (spw_ids.count(dd_to_spw[di]) > 0) {
                    selected_dds.insert(did);
                    result.data_desc_ids.push_back(did);
                    result.ddid_to_spw[did] = dd_to_spw[di];
                    result.ddid_to_pol_id[did] = dd_cols.polarization_id(di);
                }
            }

            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected_dds.count(cols.data_desc_id(r)) == 0) {
                    selected[r] = false;
                }
            }
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Scan selection ---
    if (scan_expr_)
        try {
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
                    if (!match)
                        selected[r] = false;
                    else
                        result.scans.push_back(s);
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
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Time selection ---
    if (time_expr_)
        try {
            auto expr = trim(*time_expr_);
            if (expr.empty()) {
                throw std::runtime_error("Time: empty expression");
            }

            // Comparison operators: ">value", "<value", or range "lo~hi".
            // Values can be numeric (MJD seconds) or date strings (YYYY/MM/DD[/HH:MM:SS]).
            char op = 0;
            double lo_time = -1e30;
            double hi_time = 1e30;

            if (expr.front() == '>') {
                op = '>';
                auto val = parse_time_value(expr.substr(1));
                if (!val) {
                    throw std::runtime_error("Time: invalid value '" + std::string(expr.substr(1)) +
                                             "'");
                }
                lo_time = *val;
            } else if (expr.front() == '<') {
                op = '<';
                auto val = parse_time_value(expr.substr(1));
                if (!val) {
                    throw std::runtime_error("Time: invalid value '" + std::string(expr.substr(1)) +
                                             "'");
                }
                hi_time = *val;
            } else {
                auto tilde = expr.find('~');
                if (tilde != std::string_view::npos) {
                    auto lo_val = parse_time_value(expr.substr(0, tilde));
                    auto hi_val = parse_time_value(expr.substr(tilde + 1));
                    if (!lo_val || !hi_val) {
                        throw std::runtime_error("Time: invalid range '" + std::string(expr) + "'");
                    }
                    lo_time = *lo_val;
                    hi_time = *hi_val;
                } else {
                    // Try single time value (exact match not meaningful, treat as error).
                    throw std::runtime_error("Time: expression must use >, <, or lo~hi range: '" +
                                             std::string(expr) + "'");
                }
            }

            // Store parsed time range.
            bool is_sec =
                (op == '>'
                     ? try_parse_double(expr.substr(1)).has_value()
                     : (op == '<' ? try_parse_double(expr.substr(1)).has_value()
                                  : try_parse_double(expr.substr(0, expr.find('~'))).has_value()));
            result.time_ranges.push_back({lo_time, hi_time, is_sec});

            for (std::uint64_t r = 0; r < nrow; ++r) {
                double t = cols.time(r);
                bool in_range = false;
                if (op == '>') {
                    in_range = t > lo_time;
                } else if (op == '<') {
                    in_range = t < hi_time;
                } else {
                    in_range = t >= lo_time && t <= hi_time;
                }
                if (!in_range) {
                    selected[r] = false;
                }
            }
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- UV-distance selection ---
    if (uvdist_expr_)
        try {
            auto expr = trim(*uvdist_expr_);
            if (expr.empty()) {
                throw std::runtime_error("UVdist: empty expression");
            }

            char op = 0;
            double lo_uv = 0.0;
            double hi_uv = 1e30;

            // Helper to parse a UV value with optional unit suffix.
            auto parse_uv_val = [](std::string_view sv) -> double {
                auto parsed = parse_uv_with_unit(sv);
                if (!parsed) {
                    throw std::runtime_error("UVdist: invalid value '" + std::string(sv) + "'");
                }
                if (parsed->multiplier < 0) {
                    // Wavelength or percentage mode not fully supported for now;
                    // treat wavelength as meters and percentage as raw.
                    return parsed->value;
                }
                return parsed->value * parsed->multiplier;
            };

            if (expr.front() == '>') {
                op = '>';
                lo_uv = parse_uv_val(expr.substr(1));
            } else if (expr.front() == '<') {
                op = '<';
                hi_uv = parse_uv_val(expr.substr(1));
            } else {
                auto tilde = expr.find('~');
                if (tilde != std::string_view::npos) {
                    lo_uv = parse_uv_val(expr.substr(0, tilde));
                    hi_uv = parse_uv_val(expr.substr(tilde + 1));
                } else {
                    throw std::runtime_error("UVdist: expression must use >, <, or lo~hi range: '" +
                                             std::string(expr) + "'");
                }
            }

            // Store parsed UV range.
            result.uv_ranges.push_back({lo_uv, hi_uv, UvUnit::Meters});

            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto uvw_val = cols.uvw(r);
                double uvdist = 0.0;
                if (uvw_val.size() >= 2) {
                    uvdist = std::sqrt(uvw_val[0] * uvw_val[0] + uvw_val[1] * uvw_val[1]);
                }

                bool in_range = false;
                if (op == '>') {
                    in_range = uvdist > lo_uv;
                } else if (op == '<') {
                    in_range = uvdist < hi_uv;
                } else {
                    in_range = uvdist >= lo_uv && uvdist <= hi_uv;
                }
                if (!in_range) {
                    selected[r] = false;
                }
            }
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Correlation selection ---
    if (corr_expr_)
        try {
            auto expr = trim(*corr_expr_);
            if (expr.empty()) {
                throw std::runtime_error("Correlation: empty expression");
            }
            auto tokens = split(expr, ',');
            result.correlations = tokens;
            // Correlation selection narrows the correlation axis, not rows.
            // No row filtering needed here.
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- State selection ---
    if (state_expr_)
        try {
            auto expr = trim(*state_expr_);
            if (expr.empty()) {
                throw std::runtime_error("State: empty expression");
            }

            std::set<std::int32_t> state_ids;

            // Check for bound operators < >
            if (expr.front() == '<' || expr.front() == '>') {
                char op = expr.front();
                auto val = try_parse_int(expr.substr(1));
                if (!val) {
                    throw std::runtime_error("State: invalid bound value '" +
                                             std::string(expr.substr(1)) + "'");
                }
                auto& state_table = ms.subtable("STATE");
                for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
                    auto sid = static_cast<std::int32_t>(si);
                    bool match = (op == '<') ? (sid < *val) : (sid > *val);
                    if (match)
                        state_ids.insert(sid);
                }
            } else if (is_name_pattern(expr) || is_regex_pattern(expr)) {
                // Match against OBS_MODE in STATE subtable (glob/regex).
                auto& state_table = ms.subtable("STATE");
                for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
                    auto obs_mode_val = state_table.read_scalar_cell("OBS_MODE", si);
                    if (auto* sp = std::get_if<std::string>(&obs_mode_val)) {
                        if (name_match(std::string(expr), *sp)) {
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
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Observation selection ---
    if (observation_expr_)
        try {
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
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Array (subarray) selection ---
    if (array_expr_)
        try {
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
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- Feed selection ---
    if (feed_expr_)
        try {
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
                result.feed1_list.push_back(*lhs);

                if (auto_only) {
                    // Self-feeds only: f1 == f2 == lhs
                    result.feed2_list.push_back(*lhs);
                    for (std::uint64_t r = 0; r < nrow; ++r) {
                        bool match = (cols.feed1(r) == *lhs && cols.feed2(r) == *lhs);
                        if (negate)
                            match = !match;
                        if (!match)
                            selected[r] = false;
                    }
                } else if (rest.empty() || rest == "*") {
                    // Wildcard: any row containing lhs as feed1 or feed2
                    for (std::uint64_t r = 0; r < nrow; ++r) {
                        bool match = (cols.feed1(r) == *lhs || cols.feed2(r) == *lhs);
                        if (negate)
                            match = !match;
                        if (!match)
                            selected[r] = false;
                    }
                } else {
                    // Specific pair
                    auto rhs = try_parse_int(rest);
                    if (!rhs) {
                        throw std::runtime_error("Feed: invalid feed ID '" + std::string(rest) +
                                                 "'");
                    }
                    result.feeds.push_back(*rhs);
                    result.feed2_list.push_back(*rhs);

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
                        if (negate)
                            match = !match;
                        if (!match)
                            selected[r] = false;
                    }
                }
            } else {
                // Simple feed ID list/range
                auto feed_ids = parse_int_list_or_range(expr, "Feed");
                result.feeds.assign(feed_ids.begin(), feed_ids.end());

                for (std::uint64_t r = 0; r < nrow; ++r) {
                    bool match =
                        feed_ids.count(cols.feed1(r)) > 0 || feed_ids.count(cols.feed2(r)) > 0;
                    if (negate)
                        match = !match;
                    if (!match)
                        selected[r] = false;
                }
            }
        } catch (const std::exception& e) {
            handle_error(e);
        }

    // --- TaQL expression injection ---
    if (taql_expr_)
        try {
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
        } catch (const std::exception& e) {
            handle_error(e);
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
    if (ids.empty())
        return {};
    std::string clause = std::string(column) + " IN [";
    bool first = true;
    for (auto id : ids) {
        if (!first)
            clause += ", ";
        clause += std::to_string(id);
        first = false;
    }
    clause += "]";
    if (negate)
        clause = "NOT (" + clause + ")";
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
            auto rest_sv = expr.substr(amp + 1);
            bool taql_auto_only = false;
            bool taql_with_auto = false;
            if (rest_sv.size() >= 2 && rest_sv[0] == '&' && rest_sv[1] == '&') {
                taql_auto_only = true;
                rest_sv = trim(rest_sv.substr(2));
            } else if (!rest_sv.empty() && rest_sv[0] == '&') {
                taql_with_auto = true;
                rest_sv = trim(rest_sv.substr(1));
            }
            auto lhs = try_parse_int(lhs_sv);
            if (!lhs)
                throw std::runtime_error("Antenna: invalid antenna ID in TaQL conversion");
            if (taql_auto_only) {
                auto c = "(ANTENNA1 = " + std::to_string(*lhs) +
                         " AND ANTENNA2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else if (rest_sv.empty() || rest_sv == "*") {
                auto c = "(ANTENNA1 = " + std::to_string(*lhs) +
                         " OR ANTENNA2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else {
                auto rhs = try_parse_int(rest_sv);
                if (!rhs)
                    throw std::runtime_error("Antenna: invalid antenna ID in TaQL conversion");
                std::string c;
                if (taql_with_auto) {
                    c = "((ANTENNA1 = " + std::to_string(*lhs) +
                        " AND ANTENNA2 = " + std::to_string(*rhs) +
                        ") OR (ANTENNA1 = " + std::to_string(*rhs) +
                        " AND ANTENNA2 = " + std::to_string(*lhs) +
                        ") OR (ANTENNA1 = " + std::to_string(*lhs) +
                        " AND ANTENNA2 = " + std::to_string(*lhs) +
                        ") OR (ANTENNA1 = " + std::to_string(*rhs) +
                        " AND ANTENNA2 = " + std::to_string(*rhs) + "))";
                } else {
                    c = "((ANTENNA1 = " + std::to_string(*lhs) +
                        " AND ANTENNA2 = " + std::to_string(*rhs) +
                        ") OR (ANTENNA1 = " + std::to_string(*rhs) +
                        " AND ANTENNA2 = " + std::to_string(*lhs) + "))";
                }
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
                    if (!first)
                        in_list += ", ";
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
                        if (name_match(tok, ant_cols.name(ai))) {
                            ant_ids.insert(static_cast<std::int32_t>(ai));
                        }
                    }
                }
                std::string in_list;
                bool first = true;
                for (auto id : ant_ids) {
                    if (!first)
                        in_list += ", ";
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
        bool field_negate = false;
        if (!expr.empty() && expr.front() == '!') {
            field_negate = true;
            expr = trim(expr.substr(1));
        }
        if (!field_negate && !expr.empty() && (expr.front() == '<' || expr.front() == '>')) {
            clauses.push_back(bound_to_taql(expr, "FIELD_ID", "Field"));
        } else {
            auto tokens = split(expr, ',');
            bool all_numeric = true;
            for (const auto& tok : tokens) {
                if (tok.find('~') == std::string::npos && !try_parse_int(tok)) {
                    all_numeric = false;
                    break;
                }
            }
            if (all_numeric) {
                clauses.push_back(int_expr_to_taql(expr, "FIELD_ID", "Field", field_negate));
            } else {
                MsFieldColumns fld_cols(ms);
                std::set<std::int32_t> fld_ids;
                for (const auto& tok : tokens) {
                    for (std::uint64_t fi = 0; fi < fld_cols.row_count(); ++fi) {
                        if (name_match(tok, fld_cols.name(fi))) {
                            fld_ids.insert(static_cast<std::int32_t>(fi));
                        }
                    }
                }
                std::string in_list;
                bool first = true;
                for (auto id : fld_ids) {
                    if (!first)
                        in_list += ", ";
                    in_list += std::to_string(id);
                    first = false;
                }
                auto c = "FIELD_ID IN [" + in_list + "]";
                clauses.push_back(field_negate ? ("NOT (" + c + ")") : c);
            }
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
                if (val)
                    spw_ids.insert(*val);
            }
        }
        for (std::uint64_t di = 0; di < dd_cols.row_count(); ++di) {
            if (spw_ids.count(dd_cols.spectral_window_id(di)) > 0)
                selected_dds.insert(static_cast<std::int32_t>(di));
        }
        std::string in_list;
        bool first = true;
        for (auto dd : selected_dds) {
            if (!first)
                in_list += ", ";
            in_list += std::to_string(dd);
            first = false;
        }
        if (!in_list.empty())
            clauses.push_back("DATA_DESC_ID IN [" + in_list + "]");
    }

    // Scan
    if (scan_expr_) {
        auto expr = trim(*scan_expr_);
        bool negate = false;
        if (!expr.empty() && expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }
        if (!negate && !expr.empty() &&
            (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
            clauses.push_back(bound_to_taql(expr, "SCAN_NUMBER", "Scan"));
        } else {
            clauses.push_back(int_expr_to_taql(expr, "SCAN_NUMBER", "Scan", negate));
        }
    }

    // Time — parse date strings to MJD seconds for TaQL
    if (time_expr_) {
        auto expr = trim(*time_expr_);
        if (expr.front() == '>') {
            auto val = parse_time_value(expr.substr(1));
            if (!val)
                throw std::runtime_error("Time: invalid value for TaQL conversion");
            clauses.push_back("TIME > " + std::to_string(*val));
        } else if (expr.front() == '<') {
            auto val = parse_time_value(expr.substr(1));
            if (!val)
                throw std::runtime_error("Time: invalid value for TaQL conversion");
            clauses.push_back("TIME < " + std::to_string(*val));
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo = parse_time_value(expr.substr(0, tilde));
                auto hi = parse_time_value(expr.substr(tilde + 1));
                if (!lo || !hi)
                    throw std::runtime_error("Time: invalid range for TaQL conversion");
                clauses.push_back("TIME >= " + std::to_string(*lo) +
                                  " AND TIME <= " + std::to_string(*hi));
            } else {
                throw std::runtime_error("Time: unsupported expression for TaQL conversion");
            }
        }
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
            if (val)
                clauses.push_back("sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2]) > " + std::to_string(*val));
        } else if (expr.front() == '<') {
            auto val = try_parse_double(expr.substr(1));
            if (val)
                clauses.push_back("sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2]) < " + std::to_string(*val));
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo = try_parse_double(expr.substr(0, tilde));
                auto hi = try_parse_double(expr.substr(tilde + 1));
                if (lo && hi) {
                    auto uv_expr = "sqrt(UVW[1]*UVW[1]+UVW[2]*UVW[2])";
                    clauses.push_back(std::string(uv_expr) + " >= " + std::to_string(*lo) +
                                      " AND " + std::string(uv_expr) +
                                      " <= " + std::to_string(*hi));
                }
            }
        }
    }

    // State
    if (state_expr_) {
        auto expr = trim(*state_expr_);
        if (!expr.empty() && (expr.front() == '<' || expr.front() == '>')) {
            clauses.push_back(bound_to_taql(expr, "STATE_ID", "State"));
        } else if (is_name_pattern(expr) || is_regex_pattern(expr)) {
            // Resolve pattern to IDs via STATE subtable
            auto& state_table = ms.subtable("STATE");
            std::set<std::int32_t> state_ids;
            for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
                auto obs_mode_val = state_table.read_scalar_cell("OBS_MODE", si);
                if (auto* sp = std::get_if<std::string>(&obs_mode_val)) {
                    if (name_match(std::string(expr), *sp))
                        state_ids.insert(static_cast<std::int32_t>(si));
                }
            }
            std::string in_list;
            bool first = true;
            for (auto id : state_ids) {
                if (!first)
                    in_list += ", ";
                in_list += std::to_string(id);
                first = false;
            }
            if (!in_list.empty())
                clauses.push_back("STATE_ID IN [" + in_list + "]");
        } else {
            clauses.push_back(int_expr_to_taql(expr, "STATE_ID", "State"));
        }
    }

    // Observation
    if (observation_expr_) {
        auto expr = trim(*observation_expr_);
        if (!expr.empty() &&
            (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
            clauses.push_back(bound_to_taql(expr, "OBSERVATION_ID", "Observation"));
        } else {
            clauses.push_back(int_expr_to_taql(expr, "OBSERVATION_ID", "Observation"));
        }
    }

    // Array
    if (array_expr_) {
        auto expr = trim(*array_expr_);
        if (!expr.empty() &&
            (expr.front() == '<' || expr.front() == '>')) { // NOLINT(bugprone-branch-clone)
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
            if (!lhs)
                throw std::runtime_error("Feed: invalid feed ID in TaQL conversion");
            auto rest = expr.substr(amp + 1);
            bool auto_only = false;
            if (rest.size() >= 2 && rest[0] == '&' && rest[1] == '&') {
                auto_only = true;
                rest = trim(rest.substr(2));
            } else if (!rest.empty() && rest[0] == '&') {
                rest = trim(rest.substr(1));
            }
            if (auto_only) {
                auto c = "(FEED1 = " + std::to_string(*lhs) +
                         " AND FEED2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else if (rest.empty() || rest == "*") {
                auto c = "(FEED1 = " + std::to_string(*lhs) +
                         " OR FEED2 = " + std::to_string(*lhs) + ")";
                clauses.push_back(negate ? ("NOT " + c) : c);
            } else {
                auto rhs = try_parse_int(rest);
                if (!rhs)
                    throw std::runtime_error("Feed: invalid feed ID in TaQL conversion");
                auto c = "((FEED1 = " + std::to_string(*lhs) +
                         " AND FEED2 = " + std::to_string(*rhs) +
                         ") OR (FEED1 = " + std::to_string(*rhs) +
                         " AND FEED2 = " + std::to_string(*lhs) + "))";
                clauses.push_back(negate ? ("NOT " + c) : c);
            }
        } else {
            auto ids = parse_int_list_or_range(expr, "Feed");
            std::string in_list;
            bool first = true;
            for (auto id : ids) {
                if (!first)
                    in_list += ", ";
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
        if (!expr.empty())
            clauses.push_back("(" + std::string(expr) + ")");
    }

    // Combine with AND
    if (clauses.empty())
        return {};
    std::string result;
    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0)
            result += " AND ";
        result += clauses[i];
    }
    return result;
}

} // namespace casacore_mini
