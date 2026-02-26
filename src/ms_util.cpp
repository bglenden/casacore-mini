#include "casacore_mini/ms_util.hpp"

#include "casacore_mini/table.hpp"
#include "casacore_mini/table_desc_writer.hpp"
#include "casacore_mini/velocity_machine.hpp"

#include <chrono>
#include <complex>
#include <fstream>
#include <stdexcept>

namespace casacore_mini {

// ===========================================================================
// StokesConverter
// ===========================================================================

StokesConverter::StokesConverter(std::vector<std::int32_t> in_types,
                                 std::vector<std::int32_t> out_types)
    : in_types_(std::move(in_types)), out_types_(std::move(out_types)) {}

std::vector<std::complex<float>>
StokesConverter::convert(const std::vector<std::complex<float>>& in_data) const {
    if (in_data.size() != in_types_.size()) {
        throw std::invalid_argument("StokesConverter: input size mismatch");
    }

    // Build a lookup: in_type -> index.
    auto find_in = [&](std::int32_t code) -> std::int32_t {
        for (std::size_t i = 0; i < in_types_.size(); ++i) {
            if (in_types_[i] == code) {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    };

    std::vector<std::complex<float>> result(out_types_.size(), {0.0F, 0.0F});

    for (std::size_t oi = 0; oi < out_types_.size(); ++oi) {
        const auto out_code = out_types_[oi];

        // Direct pass-through if the output type exists in input.
        auto idx = find_in(out_code);
        if (idx >= 0) {
            result[oi] = in_data[static_cast<std::size_t>(idx)];
            continue;
        }

        // Stokes I,Q,U,V from circular (RR=5, RL=6, LR=7, LL=8):
        // I = (RR + LL) / 2
        // Q = (RL + LR) / 2
        // U = (RL - LR) / (2i) = i*(LR - RL) / 2
        // V = (RR - LL) / 2
        auto rr = find_in(5);
        auto rl = find_in(6);
        auto lr = find_in(7);
        auto ll = find_in(8);

        if (out_code == 1 && rr >= 0 && ll >= 0) { // I from circular
            result[oi] =
                (in_data[static_cast<std::size_t>(rr)] + in_data[static_cast<std::size_t>(ll)]) *
                0.5F;
            continue;
        }
        if (out_code == 2 && rl >= 0 && lr >= 0) { // Q from circular
            result[oi] =
                (in_data[static_cast<std::size_t>(rl)] + in_data[static_cast<std::size_t>(lr)]) *
                0.5F;
            continue;
        }
        if (out_code == 3 && rl >= 0 && lr >= 0) { // U from circular
            result[oi] =
                std::complex<float>(0.0F, 1.0F) *
                (in_data[static_cast<std::size_t>(lr)] - in_data[static_cast<std::size_t>(rl)]) *
                0.5F;
            continue;
        }
        if (out_code == 4 && rr >= 0 && ll >= 0) { // V from circular
            result[oi] =
                (in_data[static_cast<std::size_t>(rr)] - in_data[static_cast<std::size_t>(ll)]) *
                0.5F;
            continue;
        }

        // Stokes I,Q,U,V from linear (XX=9, XY=10, YX=11, YY=12):
        // I = (XX + YY) / 2
        // Q = (XX - YY) / 2
        // U = (XY + YX) / 2
        // V = (XY - YX) / (2i) = i*(YX - XY) / 2
        auto xx = find_in(9);
        auto xy = find_in(10);
        auto yx = find_in(11);
        auto yy = find_in(12);

        if (out_code == 1 && xx >= 0 && yy >= 0) { // I from linear
            result[oi] =
                (in_data[static_cast<std::size_t>(xx)] + in_data[static_cast<std::size_t>(yy)]) *
                0.5F;
            continue;
        }
        if (out_code == 2 && xx >= 0 && yy >= 0) { // Q from linear
            result[oi] =
                (in_data[static_cast<std::size_t>(xx)] - in_data[static_cast<std::size_t>(yy)]) *
                0.5F;
            continue;
        }
        if (out_code == 3 && xy >= 0 && yx >= 0) { // U from linear
            result[oi] =
                (in_data[static_cast<std::size_t>(xy)] + in_data[static_cast<std::size_t>(yx)]) *
                0.5F;
            continue;
        }
        if (out_code == 4 && xy >= 0 && yx >= 0) { // V from linear
            result[oi] =
                std::complex<float>(0.0F, 1.0F) *
                (in_data[static_cast<std::size_t>(yx)] - in_data[static_cast<std::size_t>(xy)]) *
                0.5F;
            continue;
        }

        // No conversion path found — leave as zero.
    }

    return result;
}

// ===========================================================================
// MsDopplerUtil
// ===========================================================================

double MsDopplerUtil::velocity_to_frequency(double velocity_mps, double rest_freq_hz) {
    return radio_velocity_to_freq(velocity_mps, rest_freq_hz);
}

double MsDopplerUtil::frequency_to_velocity(double freq_hz, double rest_freq_hz) {
    return freq_ratio_to_radio_velocity(freq_hz, rest_freq_hz);
}

// ===========================================================================
// MsHistoryHandler
// ===========================================================================

MsHistoryHandler::MsHistoryHandler(MeasurementSet& ms) : ms_(ms) {}

void MsHistoryHandler::add_entry(const std::string& message, const std::string& priority,
                                 const std::string& origin, double time_s) {
    HistoryEntry entry;
    if (time_s == 0.0) {
        // Current time as MJD seconds.
        // Unix epoch = MJD 40587. MJD seconds = (unix_seconds / 86400 + 40587) * 86400.
        auto now = std::chrono::system_clock::now();
        auto unix_s = static_cast<double>(
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());
        entry.time = (unix_s / 86400.0 + 40587.0) * 86400.0;
    } else {
        entry.time = time_s;
    }
    entry.message = message;
    entry.priority = priority;
    entry.origin = origin;
    entry.application = "casacore_mini";
    entries_.push_back(std::move(entry));
}

void MsHistoryHandler::flush() {
    if (entries_.empty()) {
        return;
    }

    const auto hist_path = ms_.path() / "HISTORY";
    const auto& st = ms_.subtable("HISTORY");
    const auto nrows = static_cast<std::uint64_t>(entries_.size());

    // Build column specs from existing schema.
    std::vector<TableColumnSpec> specs;
    for (const auto& col : st.columns()) {
        TableColumnSpec spec;
        spec.name = col.name;
        spec.data_type = col.data_type;
        spec.kind = col.kind;
        spec.shape.assign(col.shape.begin(), col.shape.end());
        spec.comment = col.comment;
        specs.push_back(std::move(spec));
    }

    // Remove old table files and create fresh.
    std::filesystem::remove_all(hist_path);
    auto table = Table::create(hist_path, specs, nrows);

    for (std::uint64_t r = 0; r < nrows; ++r) {
        const auto& e = entries_[r];
        for (const auto& col : st.columns()) {
            if (col.kind == ColumnKind::array) {
                continue;
            }
            CellValue val;
            if (col.name == "TIME") {
                val = e.time;
            } else if (col.name == "OBSERVATION_ID") {
                val = e.observation_id;
            } else if (col.name == "MESSAGE") {
                val = e.message;
            } else if (col.name == "PRIORITY") {
                val = e.priority;
            } else if (col.name == "ORIGIN") {
                val = e.origin;
            } else if (col.name == "OBJECT_ID") {
                val = e.object_id;
            } else if (col.name == "APPLICATION") {
                val = e.application;
            } else if (col.data_type == DataType::tp_double) {
                val = 0.0;
            } else if (col.data_type == DataType::tp_string) {
                val = std::string{};
            } else {
                val = std::int32_t{0};
            }
            table.write_scalar_cell(col.name, r, val);
        }
    }

    table.flush();

    // Write table.info to mark it as a subtable.
    write_table_info(hist_path, "", "");

    entries_.clear();
}

} // namespace casacore_mini
