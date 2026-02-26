#include "casacore_mini/ms_iter.hpp"

#include "casacore_mini/ms_columns.hpp"

#include <cmath>
#include <map>
#include <tuple>

namespace casacore_mini {

std::vector<MsIterChunk> ms_iter_chunks(MeasurementSet& ms) {
    const auto nrows = ms.row_count();
    if (nrows == 0) {
        return {};
    }

    MsMainColumns cols(ms);

    // Group key: (array_id, field_id, data_desc_id).
    using group_key_t = std::tuple<std::int32_t, std::int32_t, std::int32_t>;
    std::map<group_key_t, std::vector<std::uint64_t>> groups;

    for (std::uint64_t r = 0; r < nrows; ++r) {
        group_key_t key{cols.array_id(r), cols.field_id(r), cols.data_desc_id(r)};
        groups[key].push_back(r);
    }

    std::vector<MsIterChunk> chunks;
    chunks.reserve(groups.size());
    for (auto& [key, rows] : groups) {
        MsIterChunk chunk;
        chunk.array_id = std::get<0>(key);
        chunk.field_id = std::get<1>(key);
        chunk.data_desc_id = std::get<2>(key);
        chunk.rows = std::move(rows);
        chunks.push_back(std::move(chunk));
    }

    return chunks;
}

std::vector<MsTimeChunk> ms_time_chunks(MeasurementSet& ms, double interval_s) {
    const auto nrows = ms.row_count();
    if (nrows == 0) {
        return {};
    }

    MsMainColumns cols(ms);

    // Find min time.
    double min_time = cols.time(0);
    for (std::uint64_t r = 1; r < nrows; ++r) {
        double t = cols.time(r);
        if (t < min_time) {
            min_time = t;
        }
    }

    // Bin rows.
    std::map<std::int64_t, std::vector<std::uint64_t>> bins;
    for (std::uint64_t r = 0; r < nrows; ++r) {
        double t = cols.time(r);
        auto bin = static_cast<std::int64_t>(std::floor((t - min_time) / interval_s));
        bins[bin].push_back(r);
    }

    std::vector<MsTimeChunk> chunks;
    chunks.reserve(bins.size());
    for (auto& [bin, rows] : bins) {
        MsTimeChunk chunk;
        chunk.time_center = min_time + (static_cast<double>(bin) + 0.5) * interval_s;
        chunk.rows = std::move(rows);
        chunks.push_back(std::move(chunk));
    }

    return chunks;
}

} // namespace casacore_mini
