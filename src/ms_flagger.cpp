#include "casacore_mini/ms_flagger.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/table.hpp"

#include <set>

namespace casacore_mini {

MsFlagStats ms_flag_stats(MeasurementSet& ms) {
    MsFlagStats stats;
    stats.total_rows = ms.row_count();

    if (stats.total_rows == 0) {
        return stats;
    }

    MsMainColumns cols(ms);
    for (std::uint64_t r = 0; r < stats.total_rows; ++r) {
        if (cols.flag_row(r)) {
            ++stats.flagged_rows;
        }
    }
    return stats;
}

namespace {

void modify_flag_rows(MeasurementSet& ms, const std::vector<std::uint64_t>& rows,
                      bool new_flag_value) {
    if (rows.empty() || ms.row_count() == 0) {
        return;
    }

    std::set<std::uint64_t> target_rows(rows.begin(), rows.end());

    auto table = Table::open_rw(ms.path());
    auto nrow = table.nrow();

    for (auto r : target_rows) {
        if (r < nrow) {
            table.write_scalar_cell("FLAG_ROW", r, CellValue(new_flag_value));
        }
    }

    table.flush();
}

} // anonymous namespace

void ms_flag_rows(MeasurementSet& ms, const std::vector<std::uint64_t>& rows) {
    modify_flag_rows(ms, rows, true);
}

void ms_unflag_rows(MeasurementSet& ms, const std::vector<std::uint64_t>& rows) {
    modify_flag_rows(ms, rows, false);
}

} // namespace casacore_mini
