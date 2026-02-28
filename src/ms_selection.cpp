#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/taql.hpp"

#include <algorithm>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>
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

/// Check if a string looks like a regex pattern: /pattern/.
bool is_regex_pattern(std::string_view sv) {
    return sv.size() >= 3 && sv.front() == '/' && sv.back() == '/';
}

/// Extract the regex pattern from /pattern/.
std::string extract_regex_pattern(std::string_view sv) {
    return std::string(sv.substr(1, sv.size() - 2));
}

/// Try to parse a date/time string "YYYY/MM/DD/HH:MM:SS" to MJD seconds.
/// Returns nullopt if the string doesn't match the expected format.
std::optional<double> try_parse_datetime_to_mjd(std::string_view sv) {
    // Expected format: YYYY/MM/DD/HH:MM:SS or YYYY/MM/DD
    // MJD calculation: days since 1858-11-17 00:00:00 UTC, then * 86400 for seconds.
    std::string s(sv);

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    // Try YYYY/MM/DD/HH:MM:SS
    if (std::sscanf(s.c_str(), "%d/%d/%d/%d:%d:%d", &year, &month, &day, &hour, &minute, &second) >=
        3) {
        // Convert to MJD using a simplified algorithm.
        // From Meeus, Astronomical Algorithms (Chapter 7).
        int a = (14 - month) / 12;
        int y = year + 4800 - a;
        int m = month + 12 * a - 3;
        // Julian Day Number
        int jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
        // JD at noon
        double jd = static_cast<double>(jdn) + (static_cast<double>(hour) - 12.0) / 24.0 +
                     static_cast<double>(minute) / 1440.0 + static_cast<double>(second) / 86400.0;
        // MJD = JD - 2400000.5
        double mjd_days = jd - 2400000.5;
        // Convert to seconds (casacore convention: TIME column is in MJD seconds).
        return mjd_days * 86400.0;
    }
    return std::nullopt;
}

/// Parse a frequency value with unit suffix (e.g., "1400MHz", "1.4GHz").
/// Returns the frequency in Hz, or nullopt if not a frequency specification.
std::optional<double> try_parse_frequency(std::string_view sv) {
    std::string s(sv);
    double value = 0.0;
    std::size_t pos = 0;

    try {
        value = std::stod(s, &pos);
    } catch (...) {
        return std::nullopt;
    }

    if (pos >= s.size()) {
        return std::nullopt; // No unit suffix
    }

    std::string unit = s.substr(pos);
    // Case-insensitive compare
    std::string lower_unit;
    lower_unit.reserve(unit.size());
    for (char c : unit) {
        lower_unit += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower_unit == "hz") {
        return value;
    }
    if (lower_unit == "khz") {
        return value * 1.0e3;
    }
    if (lower_unit == "mhz") {
        return value * 1.0e6;
    }
    if (lower_unit == "ghz") {
        return value * 1.0e9;
    }
    if (lower_unit == "thz") {
        return value * 1.0e12;
    }
    return std::nullopt;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// MsSelectionErrorHandler
// ---------------------------------------------------------------------------

void MsSelectionErrorHandler::handleError(const std::string& msg) {
    messages_.push_back(msg);
}

const std::vector<std::string>& MsSelectionErrorHandler::getMessages() const noexcept {
    return messages_;
}

bool MsSelectionErrorHandler::hasErrors() const noexcept {
    return !messages_.empty();
}

void MsSelectionErrorHandler::clear() noexcept {
    messages_.clear();
}

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

void MsSelection::set_feed_expr(std::string_view expr) {
    feed_expr_ = std::string(expr);
}

void MsSelection::set_array_expr(std::string_view expr) {
    array_expr_ = std::string(expr);
}

void MsSelection::set_intent_expr(std::string_view expr) {
    intent_expr_ = std::string(expr);
}

void MsSelection::set_poln_expr(std::string_view expr) {
    poln_expr_ = std::string(expr);
}

void MsSelection::set_taql_expr(std::string_view expr) {
    taql_expr_ = std::string(expr);
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
    feed_expr_.reset();
    array_expr_.reset();
    intent_expr_.reset();
    poln_expr_.reset();
    taql_expr_.reset();
    parse_late_ = false;
    last_result_ = MsSelectionResult{};
}

void MsSelection::reset() noexcept {
    clear();
}

bool MsSelection::has_selection() const noexcept {
    return antenna_expr_ || field_expr_ || spw_expr_ || scan_expr_ || time_expr_ || uvdist_expr_ ||
           corr_expr_ || state_expr_ || observation_expr_ || feed_expr_ || array_expr_ ||
           intent_expr_ || poln_expr_ || taql_expr_;
}

void MsSelection::set_parse_late(bool late) noexcept {
    parse_late_ = late;
}

bool MsSelection::parse_late() const noexcept {
    return parse_late_;
}

// ---------------------------------------------------------------------------
// Accessor methods
// ---------------------------------------------------------------------------

std::vector<int> MsSelection::getAntenna1List() const {
    return {last_result_.antenna1_list.begin(), last_result_.antenna1_list.end()};
}

std::vector<int> MsSelection::getAntenna2List() const {
    return {last_result_.antenna2_list.begin(), last_result_.antenna2_list.end()};
}

std::vector<std::pair<int, int>> MsSelection::getBaselineList() const {
    std::vector<std::pair<int, int>> result;
    result.reserve(last_result_.baseline_list.size());
    for (const auto& [a1, a2] : last_result_.baseline_list) {
        result.emplace_back(static_cast<int>(a1), static_cast<int>(a2));
    }
    return result;
}

std::vector<int> MsSelection::getFieldList() const {
    return {last_result_.fields.begin(), last_result_.fields.end()};
}

std::vector<int> MsSelection::getSpwList() const {
    return {last_result_.spws.begin(), last_result_.spws.end()};
}

std::vector<std::pair<int, int>> MsSelection::getChanSlices() const {
    std::vector<std::pair<int, int>> result;
    result.reserve(last_result_.chan_slices.size());
    for (const auto& [s, w] : last_result_.chan_slices) {
        result.emplace_back(static_cast<int>(s), static_cast<int>(w));
    }
    return result;
}

std::vector<int> MsSelection::getScanList() const {
    return {last_result_.scans.begin(), last_result_.scans.end()};
}

std::vector<int> MsSelection::getStateList() const {
    return {last_result_.states.begin(), last_result_.states.end()};
}

std::vector<int> MsSelection::getObservationList() const {
    return {last_result_.observation_ids.begin(), last_result_.observation_ids.end()};
}

std::vector<int> MsSelection::getSubArrayList() const {
    return {last_result_.subarray_ids.begin(), last_result_.subarray_ids.end()};
}

std::vector<int> MsSelection::getDDIDList() const {
    return {last_result_.dd_ids.begin(), last_result_.dd_ids.end()};
}

std::vector<double> MsSelection::getTimeList() const {
    return last_result_.time_list;
}

std::vector<std::pair<int, int>> MsSelection::getCorrSlices() const {
    std::vector<std::pair<int, int>> result;
    result.reserve(last_result_.corr_slices.size());
    for (const auto& [s, w] : last_result_.corr_slices) {
        result.emplace_back(static_cast<int>(s), static_cast<int>(w));
    }
    return result;
}

std::vector<double> MsSelection::getUVList() const {
    return last_result_.uv_list;
}

// ---------------------------------------------------------------------------
// Expression accessors
// ---------------------------------------------------------------------------

const std::optional<std::string>& MsSelection::antenna_expr() const noexcept {
    return antenna_expr_;
}

const std::optional<std::string>& MsSelection::field_expr() const noexcept {
    return field_expr_;
}

const std::optional<std::string>& MsSelection::spw_expr() const noexcept {
    return spw_expr_;
}

const std::optional<std::string>& MsSelection::scan_expr() const noexcept {
    return scan_expr_;
}

const std::optional<std::string>& MsSelection::time_expr() const noexcept {
    return time_expr_;
}

const std::optional<std::string>& MsSelection::uvdist_expr() const noexcept {
    return uvdist_expr_;
}

const std::optional<std::string>& MsSelection::corr_expr() const noexcept {
    return corr_expr_;
}

const std::optional<std::string>& MsSelection::state_expr() const noexcept {
    return state_expr_;
}

const std::optional<std::string>& MsSelection::observation_expr() const noexcept {
    return observation_expr_;
}

const std::optional<std::string>& MsSelection::feed_expr() const noexcept {
    return feed_expr_;
}

const std::optional<std::string>& MsSelection::array_expr() const noexcept {
    return array_expr_;
}

const std::optional<std::string>& MsSelection::intent_expr() const noexcept {
    return intent_expr_;
}

const std::optional<std::string>& MsSelection::poln_expr() const noexcept {
    return poln_expr_;
}

const std::optional<std::string>& MsSelection::taql_expr() const noexcept {
    return taql_expr_;
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

        // Check for regex pattern /pattern/.
        if (is_regex_pattern(expr)) {
            auto pattern_str = extract_regex_pattern(expr);
            std::regex re(pattern_str, std::regex::ECMAScript);

            std::set<std::int32_t> ant_ids;
            MsAntennaColumns ant_cols(ms);
            for (std::uint64_t ai = 0; ai < ant_cols.row_count(); ++ai) {
                auto aname = ant_cols.name(ai);
                if (std::regex_search(aname, re)) {
                    ant_ids.insert(static_cast<std::int32_t>(ai));
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
        // Check for baseline expression "a&b".
        else if (auto amp = expr.find('&'); amp != std::string_view::npos) {
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
            result.antenna1_list.push_back(*lhs);
            if (rhs) {
                result.antennas.push_back(*rhs);
                result.antenna2_list.push_back(*rhs);
                result.baseline_list.emplace_back(*lhs, *rhs);
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

        // Build DATA_DESC_ID -> SPW_ID map.
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

                auto spw_val = try_parse_int(spw_part);
                if (!spw_val) {
                    throw std::runtime_error("SPW: invalid SPW ID '" + std::string(spw_part) + "'");
                }

                // Check if the channel part contains a frequency range (e.g., "10~20MHz").
                auto tilde_pos = chan_part.find('~');
                if (tilde_pos != std::string_view::npos) {
                    auto lo_str = chan_part.substr(0, tilde_pos);
                    auto hi_str = chan_part.substr(tilde_pos + 1);

                    auto lo_freq = try_parse_frequency(lo_str);
                    auto hi_freq = try_parse_frequency(hi_str);

                    if (lo_freq && hi_freq) {
                        // Frequency range: convert to channel indices.
                        // Use the SPW's ref_frequency and num_chan to compute indices.
                        MsSpWindowColumns spw_cols(ms);
                        auto spw_idx = static_cast<std::uint64_t>(*spw_val);
                        if (spw_idx < spw_cols.row_count()) {
                            auto num_chan = spw_cols.num_chan(spw_idx);
                            auto ref_freq = spw_cols.ref_frequency(spw_idx);
                            auto total_bw = spw_cols.total_bandwidth(spw_idx);
                            if (total_bw <= 0.0) {
                                // Estimate total bandwidth from num_chan and ref_frequency.
                                total_bw = ref_freq * 0.1; // 10% fractional bandwidth guess
                            }
                            double chan_width = total_bw / static_cast<double>(num_chan);
                            double freq_start = ref_freq - total_bw / 2.0;

                            // Calculate channel indices for the frequency range.
                            auto lo_chan = static_cast<std::int32_t>(
                                std::max(0.0, std::floor((*lo_freq - freq_start) / chan_width)));
                            auto hi_chan = static_cast<std::int32_t>(
                                std::min(static_cast<double>(num_chan - 1),
                                         std::floor((*hi_freq - freq_start) / chan_width)));

                            if (lo_chan < 0) {
                                lo_chan = 0;
                            }
                            if (hi_chan >= num_chan) {
                                hi_chan = num_chan - 1;
                            }

                            result.channel_ranges.push_back(*spw_val);
                            result.channel_ranges.push_back(lo_chan);
                            result.channel_ranges.push_back(hi_chan);
                            result.chan_slices.emplace_back(lo_chan, hi_chan - lo_chan + 1);
                        }
                        spw_ids.insert(*spw_val);
                        continue;
                    }
                }

                // Standard channel range parsing.
                auto chan_ranges = split(chan_part, ';');
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
                        result.chan_slices.emplace_back(*lo, *hi - *lo + 1);
                    } else {
                        auto ch = try_parse_int(cr);
                        if (!ch) {
                            throw std::runtime_error("SPW: invalid channel '" + cr + "'");
                        }
                        result.channel_ranges.push_back(*spw_val);
                        result.channel_ranges.push_back(*ch);
                        result.channel_ranges.push_back(*ch);
                        result.chan_slices.emplace_back(*ch, 1);
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
        result.dd_ids.assign(selected_dds.begin(), selected_dds.end());

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

        // Handle comparison operators > and <.
        bool is_gt = false, is_lt = false;
        if (!negate && expr.front() == '>') {
            is_gt = true;
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Scan: empty bound after '>'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Scan: invalid bound value '" + std::string(val_str) + "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.scan_number(r) <= *bound) {
                    selected[r] = false;
                }
            }
            // Record matched scans for the result.
            std::set<std::int32_t> scan_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    scan_set.insert(cols.scan_number(r));
                }
            }
            result.scans.assign(scan_set.begin(), scan_set.end());
        } else if (!negate && expr.front() == '<') {
            is_lt = true;
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Scan: empty bound after '<'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Scan: invalid bound value '" + std::string(val_str) + "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.scan_number(r) >= *bound) {
                    selected[r] = false;
                }
            }
            std::set<std::int32_t> scan_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    scan_set.insert(cols.scan_number(r));
                }
            }
            result.scans.assign(scan_set.begin(), scan_set.end());
        }

        if (!is_gt && !is_lt) {
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
        bool is_range = false;

        if (expr.front() == '>') {
            auto sub = expr.substr(1);
            // Try date/time first, then double.
            auto dt_val = try_parse_datetime_to_mjd(sub);
            if (dt_val) {
                lo_mjd = *dt_val;
            } else {
                auto val = try_parse_double(sub);
                if (!val) {
                    throw std::runtime_error("Time: invalid value '" + std::string(sub) + "'");
                }
                lo_mjd = *val;
            }
        } else if (expr.front() == '<') {
            auto sub = expr.substr(1);
            auto dt_val = try_parse_datetime_to_mjd(sub);
            if (dt_val) {
                hi_mjd = *dt_val;
            } else {
                auto val = try_parse_double(sub);
                if (!val) {
                    throw std::runtime_error("Time: invalid value '" + std::string(sub) + "'");
                }
                hi_mjd = *val;
            }
        } else {
            auto tilde = expr.find('~');
            if (tilde != std::string_view::npos) {
                auto lo_str = expr.substr(0, tilde);
                auto hi_str = expr.substr(tilde + 1);

                // Try date/time first.
                auto lo_dt = try_parse_datetime_to_mjd(lo_str);
                auto hi_dt = try_parse_datetime_to_mjd(hi_str);

                if (lo_dt && hi_dt) {
                    lo_mjd = *lo_dt;
                    hi_mjd = *hi_dt;
                } else {
                    auto lo_val = try_parse_double(lo_str);
                    auto hi_val = try_parse_double(hi_str);
                    if (!lo_val || !hi_val) {
                        throw std::runtime_error("Time: invalid range '" + std::string(expr) + "'");
                    }
                    lo_mjd = *lo_val;
                    hi_mjd = *hi_val;
                }
                is_range = true;
            } else {
                // Try a single date/time value as a point-in-time (range of +-0.5s).
                auto dt_val = try_parse_datetime_to_mjd(expr);
                if (dt_val) {
                    lo_mjd = *dt_val - 0.5;
                    hi_mjd = *dt_val + 0.5;
                    is_range = true;
                } else {
                    throw std::runtime_error(
                        "Time: expression must use >, <, or lo~hi range: '" + std::string(expr) +
                        "'");
                }
            }
        }

        // Record time range.
        result.time_list.push_back(lo_mjd);
        result.time_list.push_back(hi_mjd);

        for (std::uint64_t r = 0; r < nrow; ++r) {
            double t = cols.time(r);
            bool in_range = false;
            if (expr.front() == '>') {
                in_range = t > lo_mjd;
            } else if (expr.front() == '<') {
                in_range = t < hi_mjd;
            } else if (is_range) {
                in_range = t >= lo_mjd && t <= hi_mjd;
            }
            if (!in_range) {
                selected[r] = false;
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

        // Record UV range.
        result.uv_list.push_back(lo_uv);
        result.uv_list.push_back(hi_uv);

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
    if (corr_expr_ || poln_expr_) {
        auto expr_str = corr_expr_ ? *corr_expr_ : *poln_expr_;
        auto expr = trim(expr_str);
        if (expr.empty()) {
            throw std::runtime_error("Correlation: empty expression");
        }
        auto tokens = split(expr, ',');
        result.correlations = tokens;

        // Build correlation slices: each correlation token is (index, 1).
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            result.corr_slices.emplace_back(static_cast<std::int32_t>(i), 1);
        }
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

        if (expr.front() == '>') {
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Observation: empty bound after '>'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Observation: invalid bound value '" +
                                         std::string(val_str) + "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.observation_id(r) <= *bound) {
                    selected[r] = false;
                }
            }
            std::set<std::int32_t> obs_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    obs_set.insert(cols.observation_id(r));
                }
            }
            result.observation_ids.assign(obs_set.begin(), obs_set.end());
            result.observations = result.observation_ids;
        } else if (expr.front() == '<') {
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Observation: empty bound after '<'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Observation: invalid bound value '" +
                                         std::string(val_str) + "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.observation_id(r) >= *bound) {
                    selected[r] = false;
                }
            }
            std::set<std::int32_t> obs_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    obs_set.insert(cols.observation_id(r));
                }
            }
            result.observation_ids.assign(obs_set.begin(), obs_set.end());
            result.observations = result.observation_ids;
        } else {
            auto obs_ids = parse_int_list_or_range(expr, "Observation");
            result.observation_ids.assign(obs_ids.begin(), obs_ids.end());
            result.observations = result.observation_ids;

            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (obs_ids.count(cols.observation_id(r)) == 0) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Array/sub-array selection ---
    if (array_expr_) {
        auto expr = trim(*array_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Array: empty expression");
        }

        if (expr.front() == '>') {
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Array: empty bound after '>'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Array: invalid bound value '" + std::string(val_str) +
                                         "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.array_id(r) <= *bound) {
                    selected[r] = false;
                }
            }
            std::set<std::int32_t> arr_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    arr_set.insert(cols.array_id(r));
                }
            }
            result.subarray_ids.assign(arr_set.begin(), arr_set.end());
            result.arrays = result.subarray_ids;
        } else if (expr.front() == '<') {
            auto val_str = trim(expr.substr(1));
            if (val_str.empty()) {
                throw std::runtime_error("Array: empty bound after '<'");
            }
            auto bound = try_parse_int(val_str);
            if (!bound) {
                throw std::runtime_error("Array: invalid bound value '" + std::string(val_str) +
                                         "'");
            }
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (cols.array_id(r) >= *bound) {
                    selected[r] = false;
                }
            }
            std::set<std::int32_t> arr_set;
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (selected[r]) {
                    arr_set.insert(cols.array_id(r));
                }
            }
            result.subarray_ids.assign(arr_set.begin(), arr_set.end());
            result.arrays = result.subarray_ids;
        } else {
            auto arr_ids = parse_int_list_or_range(expr, "Array");
            result.subarray_ids.assign(arr_ids.begin(), arr_ids.end());
            result.arrays = result.subarray_ids;

            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (arr_ids.count(cols.array_id(r)) == 0) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- Intent selection (glob on STATE.OBS_MODE) ---
    if (intent_expr_) {
        auto expr = trim(*intent_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Intent: empty expression");
        }

        std::set<std::int32_t> state_ids;
        auto& state_table = ms.subtable("STATE");
        for (std::uint64_t si = 0; si < state_table.nrow(); ++si) {
            auto obs_mode_val = state_table.read_scalar_cell("OBS_MODE", si);
            if (auto* sp = std::get_if<std::string>(&obs_mode_val)) {
                if (glob_match(std::string(expr), *sp)) {
                    state_ids.insert(static_cast<std::int32_t>(si));
                }
            }
        }

        // Merge with any existing state selections.
        for (auto id : state_ids) {
            if (std::find(result.states.begin(), result.states.end(), id) == result.states.end()) {
                result.states.push_back(id);
            }
        }
        std::sort(result.states.begin(), result.states.end());

        for (std::uint64_t r = 0; r < nrow; ++r) {
            if (state_ids.count(cols.state_id(r)) == 0) {
                selected[r] = false;
            }
        }
    }

    // --- Feed selection ---
    if (feed_expr_) {
        auto expr = trim(*feed_expr_);
        if (expr.empty()) {
            throw std::runtime_error("Feed: empty expression");
        }

        bool negate = false;
        if (expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }

        // Check for auto-only "a&&&" syntax.
        auto triple_amp = std::string(expr).find("&&&");
        if (!negate && triple_amp != std::string::npos) {
            auto lhs_sv = trim(expr.substr(0, triple_amp));
            if (lhs_sv.empty()) {
                throw std::runtime_error("Feed: missing LHS in auto-only expression");
            }
            auto lhs = try_parse_int(lhs_sv);
            if (!lhs) {
                throw std::runtime_error("Feed: non-numeric LHS '" + std::string(lhs_sv) + "'");
            }
            result.feeds.push_back(*lhs);
            for (std::uint64_t r = 0; r < nrow; ++r) {
                if (!(cols.feed1(r) == *lhs && cols.feed2(r) == *lhs)) {
                    selected[r] = false;
                }
            }
        }
        // Check for cross+auto "a&&b" syntax.
        else if (!negate && std::string(expr).find("&&") != std::string::npos) {
            auto damp = std::string(expr).find("&&");
            auto lhs_sv = trim(expr.substr(0, damp));
            auto rhs_sv = trim(expr.substr(damp + 2));
            if (lhs_sv.empty()) {
                throw std::runtime_error("Feed: missing LHS in pair expression");
            }
            if (rhs_sv.empty()) {
                throw std::runtime_error("Feed: missing RHS in pair expression");
            }
            auto lhs = try_parse_int(lhs_sv);
            auto rhs = try_parse_int(rhs_sv);
            if (!lhs) {
                throw std::runtime_error("Feed: non-numeric LHS '" + std::string(lhs_sv) + "'");
            }
            if (!rhs) {
                throw std::runtime_error("Feed: non-numeric RHS '" + std::string(rhs_sv) + "'");
            }
            result.feeds.push_back(*lhs);
            result.feeds.push_back(*rhs);
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto f1 = cols.feed1(r);
                auto f2 = cols.feed2(r);
                // Cross: (f1=lhs,f2=rhs) or (f1=rhs,f2=lhs)
                // Auto: (f1=lhs,f2=lhs) or (f1=rhs,f2=rhs)
                bool cross = (f1 == *lhs && f2 == *rhs) || (f1 == *rhs && f2 == *lhs);
                bool auto_match = (f1 == *lhs && f2 == *lhs) || (f1 == *rhs && f2 == *rhs);
                if (!cross && !auto_match) {
                    selected[r] = false;
                }
            }
        }
        // Check for cross-only "a&b" syntax.
        else if (!negate && std::string(expr).find('&') != std::string::npos) {
            auto amp = std::string(expr).find('&');
            auto lhs_sv = trim(expr.substr(0, amp));
            auto rhs_sv = trim(expr.substr(amp + 1));
            if (lhs_sv.empty()) {
                throw std::runtime_error("Feed: missing LHS in pair expression");
            }
            if (rhs_sv.empty()) {
                throw std::runtime_error("Feed: missing RHS in pair expression");
            }
            auto lhs = try_parse_int(lhs_sv);
            auto rhs = try_parse_int(rhs_sv);
            if (!lhs) {
                throw std::runtime_error("Feed: non-numeric LHS '" + std::string(lhs_sv) + "'");
            }
            if (!rhs) {
                throw std::runtime_error("Feed: non-numeric RHS '" + std::string(rhs_sv) + "'");
            }
            result.feeds.push_back(*lhs);
            result.feeds.push_back(*rhs);
            for (std::uint64_t r = 0; r < nrow; ++r) {
                auto f1 = cols.feed1(r);
                auto f2 = cols.feed2(r);
                bool cross = (f1 == *lhs && f2 == *rhs) || (f1 == *rhs && f2 == *lhs);
                if (!cross) {
                    selected[r] = false;
                }
            }
        } else {
            // Single feed ID or comma-separated list.
            std::set<std::int32_t> feed_ids;
            auto tokens = split(expr, ',');
            for (const auto& tok : tokens) {
                auto val = try_parse_int(tok);
                if (!val) {
                    throw std::runtime_error("Feed: non-numeric feed ID '" + tok + "'");
                }
                feed_ids.insert(*val);
            }
            result.feeds.assign(feed_ids.begin(), feed_ids.end());

            for (std::uint64_t r = 0; r < nrow; ++r) {
                bool match = feed_ids.count(cols.feed1(r)) > 0 || feed_ids.count(cols.feed2(r)) > 0;
                if (negate) {
                    match = !match;
                }
                if (!match) {
                    selected[r] = false;
                }
            }
        }
    }

    // --- TaQL expression ---
    if (taql_expr_) {
        auto expr = trim(*taql_expr_);
        if (expr.empty()) {
            throw std::runtime_error("TaQL: empty expression");
        }

        // Evaluate the TaQL WHERE clause against the main table.
        auto& main = ms.main_table();
        auto taql_result = taql_execute("SELECT FROM t WHERE " + std::string(expr), main);
        // Build a set of matched row indices.
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

    // Sync backward-compatible alias fields.
    result.observations = result.observation_ids;
    result.arrays = result.subarray_ids;

    // Cache the result for accessor methods.
    last_result_ = result;

    return result;
}

// ---------------------------------------------------------------------------
// to_taql_where: generate a TaQL WHERE clause from the current selection
// ---------------------------------------------------------------------------

std::string MsSelection::to_taql_where(MeasurementSet& ms) const {
    std::vector<std::string> clauses;

    if (antenna_expr_) {
        // Evaluate to get the antenna IDs, then build a clause.
        // We create a temporary result to get the resolved antenna IDs.
        MsSelection temp;
        temp.set_antenna_expr(*antenna_expr_);
        auto r = temp.evaluate(ms);
        if (!r.antennas.empty()) {
            std::ostringstream oss;
            oss << "(ANTENNA1 IN [";
            for (std::size_t i = 0; i < r.antennas.size(); ++i) {
                if (i > 0) oss << ",";
                oss << r.antennas[i];
            }
            oss << "] OR ANTENNA2 IN [";
            for (std::size_t i = 0; i < r.antennas.size(); ++i) {
                if (i > 0) oss << ",";
                oss << r.antennas[i];
            }
            oss << "])";
            clauses.push_back(oss.str());
        }
    }

    if (field_expr_) {
        MsSelection temp;
        temp.set_field_expr(*field_expr_);
        auto r = temp.evaluate(ms);
        if (!r.fields.empty()) {
            std::ostringstream oss;
            oss << "FIELD_ID IN [";
            for (std::size_t i = 0; i < r.fields.size(); ++i) {
                if (i > 0) oss << ",";
                oss << r.fields[i];
            }
            oss << "]";
            clauses.push_back(oss.str());
        }
    }

    if (spw_expr_) {
        MsSelection temp;
        temp.set_spw_expr(*spw_expr_);
        auto r = temp.evaluate(ms);
        if (!r.dd_ids.empty()) {
            std::ostringstream oss;
            oss << "DATA_DESC_ID IN [";
            for (std::size_t i = 0; i < r.dd_ids.size(); ++i) {
                if (i > 0) oss << ",";
                oss << r.dd_ids[i];
            }
            oss << "]";
            clauses.push_back(oss.str());
        }
    }

    if (scan_expr_) {
        auto expr = trim(*scan_expr_);
        bool negate = false;
        if (!expr.empty() && expr.front() == '!') {
            negate = true;
            expr = trim(expr.substr(1));
        }
        if (!expr.empty() && expr.front() == '>') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("SCAN_NUMBER > " + std::to_string(*bound));
            }
        } else if (!expr.empty() && expr.front() == '<') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("SCAN_NUMBER < " + std::to_string(*bound));
            }
        } else {
            MsSelection temp;
            temp.set_scan_expr(*scan_expr_);
            auto r = temp.evaluate(ms);
            if (!r.scans.empty()) {
                std::ostringstream oss;
                if (negate) {
                    oss << "NOT (";
                }
                oss << "SCAN_NUMBER IN [";
                for (std::size_t i = 0; i < r.scans.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << r.scans[i];
                }
                oss << "]";
                if (negate) {
                    oss << ")";
                }
                clauses.push_back(oss.str());
            }
        }
    }

    if (state_expr_) {
        MsSelection temp;
        temp.set_state_expr(*state_expr_);
        auto r = temp.evaluate(ms);
        if (!r.states.empty()) {
            std::ostringstream oss;
            oss << "STATE_ID IN [";
            for (std::size_t i = 0; i < r.states.size(); ++i) {
                if (i > 0) oss << ",";
                oss << r.states[i];
            }
            oss << "]";
            clauses.push_back(oss.str());
        }
    }

    if (observation_expr_) {
        auto expr = trim(*observation_expr_);
        if (!expr.empty() && expr.front() == '>') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("OBSERVATION_ID > " + std::to_string(*bound));
            }
        } else if (!expr.empty() && expr.front() == '<') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("OBSERVATION_ID < " + std::to_string(*bound));
            }
        } else {
            MsSelection temp;
            temp.set_observation_expr(*observation_expr_);
            auto r = temp.evaluate(ms);
            if (!r.observation_ids.empty()) {
                std::ostringstream oss;
                oss << "OBSERVATION_ID IN [";
                for (std::size_t i = 0; i < r.observation_ids.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << r.observation_ids[i];
                }
                oss << "]";
                clauses.push_back(oss.str());
            }
        }
    }

    if (array_expr_) {
        auto expr = trim(*array_expr_);
        if (!expr.empty() && expr.front() == '>') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("ARRAY_ID > " + std::to_string(*bound));
            }
        } else if (!expr.empty() && expr.front() == '<') {
            auto val_str = trim(expr.substr(1));
            auto bound = try_parse_int(val_str);
            if (bound) {
                clauses.push_back("ARRAY_ID < " + std::to_string(*bound));
            }
        } else {
            MsSelection temp;
            temp.set_array_expr(*array_expr_);
            auto r = temp.evaluate(ms);
            if (!r.subarray_ids.empty()) {
                std::ostringstream oss;
                oss << "ARRAY_ID IN [";
                for (std::size_t i = 0; i < r.subarray_ids.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << r.subarray_ids[i];
                }
                oss << "]";
                clauses.push_back(oss.str());
            }
        }
    }

    if (taql_expr_) {
        auto expr = trim(*taql_expr_);
        if (!expr.empty()) {
            clauses.push_back("(" + std::string(expr) + ")");
        }
    }

    if (clauses.empty()) {
        return {};
    }

    std::ostringstream result;
    for (std::size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) {
            result << " AND ";
        }
        result << clauses[i];
    }
    return result.str();
}

} // namespace casacore_mini
