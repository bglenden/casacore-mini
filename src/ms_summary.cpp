#include "casacore_mini/ms_summary.hpp"
#include "casacore_mini/ms_metadata.hpp"

#include <sstream>

namespace casacore_mini {

std::string ms_summary(MeasurementSet& ms) {
    MsMetaData md(ms);
    std::ostringstream oss;

    oss << "MeasurementSet: " << ms.path().filename().string() << "\n";
    oss << "  Rows: " << md.n_rows() << "\n";

    // Observation info.
    oss << "\n  Observation:\n";
    for (std::uint64_t i = 0; i < md.n_observations(); ++i) {
        oss << "    [" << i << "] Telescope: " << md.telescope_names()[i]
            << "  Observer: " << md.observers()[i] << "\n";
    }

    // Antennas.
    oss << "\n  Antennas: " << md.n_antennas() << "\n";
    for (std::uint64_t i = 0; i < md.n_antennas(); ++i) {
        oss << "    [" << i << "] " << md.antenna_names()[i] << " (" << md.antenna_stations()[i]
            << ")  diam=" << md.antenna_diameters()[i] << "m\n";
    }

    // Fields.
    oss << "\n  Fields: " << md.n_fields() << "\n";
    for (std::uint64_t i = 0; i < md.n_fields(); ++i) {
        oss << "    [" << i << "] " << md.field_names()[i] << "\n";
    }

    // Spectral windows.
    oss << "\n  Spectral Windows: " << md.n_spws() << "\n";
    for (std::uint64_t i = 0; i < md.n_spws(); ++i) {
        oss << "    [" << i << "] " << md.spw_names()[i]
            << "  ref_freq=" << md.spw_ref_frequencies()[i] / 1e6 << " MHz"
            << "  nchan=" << md.spw_num_channels()[i] << "\n";
    }

    // Scans.
    const auto& scans = md.scan_numbers();
    oss << "\n  Scans: " << scans.size() << " [";
    bool first = true;
    for (auto s : scans) {
        if (!first) {
            oss << ", ";
        }
        oss << s;
        first = false;
    }
    oss << "]\n";

    // Subtable row counts.
    oss << "\n  Subtables:\n";
    for (const auto& name : ms.subtable_names()) {
        const auto& st = ms.subtable(name);
        oss << "    " << name << ": " << st.nrow() << " rows\n";
    }

    return oss.str();
}

} // namespace casacore_mini
