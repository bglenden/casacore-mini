#include "casacore_mini/ms_metadata.hpp"
#include "casacore_mini/ms_columns.hpp"

namespace casacore_mini {

MsMetaData::MsMetaData(MeasurementSet& ms) : ms_(ms) {}

// ---------------------------------------------------------------------------
// Antenna
// ---------------------------------------------------------------------------

void MsMetaData::ensure_antenna_cache() const {
    if (antenna_loaded_) {
        return;
    }
    MsAntennaColumns cols(ms_);
    auto n = cols.row_count();
    antenna_names_.reserve(n);
    antenna_stations_.reserve(n);
    antenna_diameters_.reserve(n);
    for (std::uint64_t r = 0; r < n; ++r) {
        antenna_names_.push_back(cols.name(r));
        antenna_stations_.push_back(cols.station(r));
        antenna_diameters_.push_back(cols.dish_diameter(r));
    }
    antenna_loaded_ = true;
}

std::uint64_t MsMetaData::n_antennas() const {
    ensure_antenna_cache();
    return antenna_names_.size();
}

const std::vector<std::string>& MsMetaData::antenna_names() const {
    ensure_antenna_cache();
    return antenna_names_;
}

const std::vector<std::string>& MsMetaData::antenna_stations() const {
    ensure_antenna_cache();
    return antenna_stations_;
}

const std::vector<double>& MsMetaData::antenna_diameters() const {
    ensure_antenna_cache();
    return antenna_diameters_;
}

// ---------------------------------------------------------------------------
// Field
// ---------------------------------------------------------------------------

void MsMetaData::ensure_field_cache() const {
    if (field_loaded_) {
        return;
    }
    MsFieldColumns cols(ms_);
    auto n = cols.row_count();
    field_names_.reserve(n);
    for (std::uint64_t r = 0; r < n; ++r) {
        field_names_.push_back(cols.name(r));
    }
    field_loaded_ = true;
}

std::uint64_t MsMetaData::n_fields() const {
    ensure_field_cache();
    return field_names_.size();
}

const std::vector<std::string>& MsMetaData::field_names() const {
    ensure_field_cache();
    return field_names_;
}

// ---------------------------------------------------------------------------
// Spectral window
// ---------------------------------------------------------------------------

void MsMetaData::ensure_spw_cache() const {
    if (spw_loaded_) {
        return;
    }
    MsSpWindowColumns cols(ms_);
    auto n = cols.row_count();
    spw_names_.reserve(n);
    spw_ref_freqs_.reserve(n);
    spw_num_chans_.reserve(n);
    for (std::uint64_t r = 0; r < n; ++r) {
        spw_names_.push_back(cols.name(r));
        spw_ref_freqs_.push_back(cols.ref_frequency(r));
        spw_num_chans_.push_back(cols.num_chan(r));
    }
    spw_loaded_ = true;
}

std::uint64_t MsMetaData::n_spws() const {
    ensure_spw_cache();
    return spw_names_.size();
}

const std::vector<std::string>& MsMetaData::spw_names() const {
    ensure_spw_cache();
    return spw_names_;
}

const std::vector<double>& MsMetaData::spw_ref_frequencies() const {
    ensure_spw_cache();
    return spw_ref_freqs_;
}

const std::vector<std::int32_t>& MsMetaData::spw_num_channels() const {
    ensure_spw_cache();
    return spw_num_chans_;
}

// ---------------------------------------------------------------------------
// Observation
// ---------------------------------------------------------------------------

void MsMetaData::ensure_obs_cache() const {
    if (obs_loaded_) {
        return;
    }
    MsObservationColumns cols(ms_);
    auto n = cols.row_count();
    telescope_names_.reserve(n);
    observers_.reserve(n);
    for (std::uint64_t r = 0; r < n; ++r) {
        telescope_names_.push_back(cols.telescope_name(r));
        observers_.push_back(cols.observer(r));
    }
    obs_loaded_ = true;
}

std::uint64_t MsMetaData::n_observations() const {
    ensure_obs_cache();
    return telescope_names_.size();
}

const std::vector<std::string>& MsMetaData::telescope_names() const {
    ensure_obs_cache();
    return telescope_names_;
}

const std::vector<std::string>& MsMetaData::observers() const {
    ensure_obs_cache();
    return observers_;
}

// ---------------------------------------------------------------------------
// Main table aggregates
// ---------------------------------------------------------------------------

void MsMetaData::ensure_main_cache() const {
    if (main_loaded_) {
        return;
    }
    auto nrow = ms_.row_count();
    if (nrow > 0) {
        MsMainColumns cols(ms_);
        for (std::uint64_t r = 0; r < nrow; ++r) {
            scan_numbers_.insert(cols.scan_number(r));
            array_ids_.insert(cols.array_id(r));
            observation_ids_.insert(cols.observation_id(r));
        }
    }
    main_loaded_ = true;
}

std::uint64_t MsMetaData::n_rows() const {
    return ms_.row_count();
}

const std::set<std::int32_t>& MsMetaData::scan_numbers() const {
    ensure_main_cache();
    return scan_numbers_;
}

const std::set<std::int32_t>& MsMetaData::array_ids() const {
    ensure_main_cache();
    return array_ids_;
}

const std::set<std::int32_t>& MsMetaData::observation_ids() const {
    ensure_main_cache();
    return observation_ids_;
}

} // namespace casacore_mini
