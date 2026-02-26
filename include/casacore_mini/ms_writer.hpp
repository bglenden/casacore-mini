#pragma once

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/table_desc.hpp"

#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Batch writer for populating a MeasurementSet with data.
///
/// MsWriter collects rows in memory and writes them all at once when flush()
/// is called. This matches the SsmWriter/TiledStManWriter model which requires
/// a known row count at setup time.

/// A single main-table row's scalar + array data.
struct MsMainRow {
    std::int32_t antenna1 = 0;
    std::int32_t antenna2 = 0;
    std::int32_t array_id = 0;
    std::int32_t data_desc_id = 0;
    double exposure = 0.0;
    std::int32_t feed1 = 0;
    std::int32_t feed2 = 0;
    std::int32_t field_id = 0;
    bool flag_row = false;
    double interval = 0.0;
    std::int32_t observation_id = 0;
    std::int32_t processor_id = 0;
    std::int32_t scan_number = 0;
    std::int32_t state_id = 0;
    double time = 0.0;
    double time_centroid = 0.0;
    /// UVW coordinates (must have 3 elements).
    std::vector<double> uvw;
    /// Sigma per correlation.
    std::vector<float> sigma;
    /// Weight per correlation.
    std::vector<float> weight;
    /// Complex visibility data [ncorr * nchan] (row-major). Empty if no DATA.
    std::vector<std::complex<float>> data;
    /// Flag per channel per correlation [ncorr * nchan]. Empty if no FLAG.
    std::vector<bool> flag;
};

/// A row for the ANTENNA subtable.
struct MsAntennaRow {
    std::string name;
    std::string station;
    std::string type = "GROUND-BASED";
    std::string mount = "ALT-AZ";
    std::vector<double> position; ///< ITRF XYZ [3].
    std::vector<double> offset;   ///< Axes offset [3].
    double dish_diameter = 0.0;
    bool flag_row = false;
};

/// A row for the SPECTRAL_WINDOW subtable.
struct MsSpWindowRow {
    std::int32_t num_chan = 0;
    std::string name;
    double ref_frequency = 0.0;
    std::vector<double> chan_freq;
    std::vector<double> chan_width;
    std::vector<double> effective_bw;
    std::vector<double> resolution;
    std::int32_t meas_freq_ref = 0;
    double total_bandwidth = 0.0;
    std::int32_t net_sideband = 0;
    std::int32_t if_conv_chain = 0;
    std::int32_t freq_group = 0;
    std::string freq_group_name;
    bool flag_row = false;
};

/// A row for the FIELD subtable.
struct MsFieldRow {
    std::string name;
    std::string code;
    double time = 0.0;
    std::int32_t num_poly = 0;
    std::int32_t source_id = -1;
    bool flag_row = false;
};

/// A row for the DATA_DESCRIPTION subtable.
struct MsDataDescRow {
    std::int32_t spectral_window_id = 0;
    std::int32_t polarization_id = 0;
    bool flag_row = false;
};

/// A row for the POLARIZATION subtable.
struct MsPolarizationRow {
    std::int32_t num_corr = 0;
    std::vector<std::int32_t> corr_type;
    bool flag_row = false;
};

/// A row for the OBSERVATION subtable.
struct MsObservationRow {
    std::string telescope_name;
    std::string observer;
    std::string project;
    double release_date = 0.0;
    bool flag_row = false;
};

/// A row for the STATE subtable.
struct MsStateRow {
    bool sig = true;
    bool ref = false;
    double cal = 0.0;
    double load = 0.0;
    std::int32_t sub_scan = 0;
    std::string obs_mode;
    bool flag_row = false;
};

/// Batch writer for populating a MeasurementSet.
///
/// Usage:
/// @code
///   auto ms = MeasurementSet::create("my.ms");
///   MsWriter writer(ms);
///   writer.add_antenna({.name="ANT0", .station="PAD0", .dish_diameter=25.0});
///   writer.add_spectral_window({.num_chan=64, ...});
///   writer.add_data_description({.spectral_window_id=0, .polarization_id=0});
///   writer.add_row({.antenna1=0, .antenna2=1, .time=4.8e9, ...});
///   writer.flush();  // writes all data to disk
/// @endcode
class MsWriter {
  public:
    explicit MsWriter(MeasurementSet& ms);

    /// Add a main-table row.
    void add_row(MsMainRow row);

    /// Add an ANTENNA subtable row. Returns the row index.
    std::uint64_t add_antenna(MsAntennaRow row);
    /// Add a SPECTRAL_WINDOW subtable row. Returns the row index.
    std::uint64_t add_spectral_window(MsSpWindowRow row);
    /// Add a FIELD subtable row. Returns the row index.
    std::uint64_t add_field(MsFieldRow row);
    /// Add a DATA_DESCRIPTION subtable row. Returns the row index.
    std::uint64_t add_data_description(MsDataDescRow row);
    /// Add a POLARIZATION subtable row. Returns the row index.
    std::uint64_t add_polarization(MsPolarizationRow row);
    /// Add an OBSERVATION subtable row. Returns the row index.
    std::uint64_t add_observation(MsObservationRow row);
    /// Add a STATE subtable row. Returns the row index.
    std::uint64_t add_state(MsStateRow row);

    /// Validate foreign-key references in pending main-table rows.
    /// Checks ANTENNA1, ANTENNA2, FIELD_ID, DATA_DESC_ID, OBSERVATION_ID,
    /// PROCESSOR_ID, STATE_ID against subtable row counts.
    /// @throws std::runtime_error listing all invalid references.
    void validate_foreign_keys() const;

    /// Write all pending rows to disk and update the MS.
    /// Calls validate_foreign_keys() before writing.
    void flush();

  private:
    void flush_main_table();
    void
    flush_subtable(std::string_view name, const std::vector<ColumnDesc>& columns,
                   std::uint64_t nrows,
                   const std::function<CellValue(std::size_t col, std::uint64_t row)>& get_cell);

    MeasurementSet& ms_;
    std::vector<MsMainRow> main_rows_;
    std::vector<MsAntennaRow> antenna_rows_;
    std::vector<MsSpWindowRow> spw_rows_;
    std::vector<MsFieldRow> field_rows_;
    std::vector<MsDataDescRow> dd_rows_;
    std::vector<MsPolarizationRow> pol_rows_;
    std::vector<MsObservationRow> obs_rows_;
    std::vector<MsStateRow> state_rows_;
};

} // namespace casacore_mini
