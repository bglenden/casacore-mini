#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Cached metadata queries for a MeasurementSet.
///
/// MsMetaData provides efficient access to commonly queried MS properties
/// such as antenna names, field names, spectral window frequencies, scan
/// numbers, etc. Results are computed on first access and cached.

class MsMetaData {
  public:
    explicit MsMetaData(MeasurementSet& ms);

    // --- Antenna queries ---
    [[nodiscard]] std::uint64_t n_antennas() const;
    [[nodiscard]] const std::vector<std::string>& antenna_names() const;
    [[nodiscard]] const std::vector<std::string>& antenna_stations() const;
    [[nodiscard]] const std::vector<double>& antenna_diameters() const;

    // --- Field queries ---
    [[nodiscard]] std::uint64_t n_fields() const;
    [[nodiscard]] const std::vector<std::string>& field_names() const;

    // --- Spectral window queries ---
    [[nodiscard]] std::uint64_t n_spws() const;
    [[nodiscard]] const std::vector<std::string>& spw_names() const;
    [[nodiscard]] const std::vector<double>& spw_ref_frequencies() const;
    [[nodiscard]] const std::vector<std::int32_t>& spw_num_channels() const;

    // --- Observation queries ---
    [[nodiscard]] std::uint64_t n_observations() const;
    [[nodiscard]] const std::vector<std::string>& telescope_names() const;
    [[nodiscard]] const std::vector<std::string>& observers() const;

    // --- Main-table aggregate queries ---
    [[nodiscard]] std::uint64_t n_rows() const;
    [[nodiscard]] const std::set<std::int32_t>& scan_numbers() const;
    [[nodiscard]] const std::set<std::int32_t>& array_ids() const;
    [[nodiscard]] const std::set<std::int32_t>& observation_ids() const;

  private:
    void ensure_antenna_cache() const;
    void ensure_field_cache() const;
    void ensure_spw_cache() const;
    void ensure_obs_cache() const;
    void ensure_main_cache() const;

    MeasurementSet& ms_;

    mutable bool antenna_loaded_ = false;
    mutable std::vector<std::string> antenna_names_;
    mutable std::vector<std::string> antenna_stations_;
    mutable std::vector<double> antenna_diameters_;

    mutable bool field_loaded_ = false;
    mutable std::vector<std::string> field_names_;

    mutable bool spw_loaded_ = false;
    mutable std::vector<std::string> spw_names_;
    mutable std::vector<double> spw_ref_freqs_;
    mutable std::vector<std::int32_t> spw_num_chans_;

    mutable bool obs_loaded_ = false;
    mutable std::vector<std::string> telescope_names_;
    mutable std::vector<std::string> observers_;

    mutable bool main_loaded_ = false;
    mutable std::set<std::int32_t> scan_numbers_;
    mutable std::set<std::int32_t> array_ids_;
    mutable std::set<std::int32_t> observation_ids_;
};

} // namespace casacore_mini
