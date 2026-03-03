// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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

///
/// Lazy-cached metadata queries for a MeasurementSet.
///
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS container whose subtables are queried
///
///
///
/// `MsMetaData` is a read-only façade over a `MeasurementSet`
/// that collects and caches commonly queried metadata.  Each logical group of
/// metadata (antennas, fields, spectral windows, observations, and main-table
/// aggregates) is loaded from the underlying subtable exactly once, on the
/// first access to any property in that group.  Subsequent accesses return the
/// in-memory cached values.
///
/// This design is useful when multiple callers query the same properties
/// repeatedly, because it avoids repeated row-by-row reads from the table
/// storage layer.
///
/// The five cache groups are:
///
///   - Antenna cache — names, station pads, dish diameters from ANTENNA
///   - Field cache   — field names from FIELD
///   - SPW cache     — names, reference frequencies, channel counts from
///                        SPECTRAL_WINDOW
///   - Observation cache — telescope names, observer names from OBSERVATION
///   - Main-table cache  — unique scan numbers, array IDs, observation IDs
///                            scanned from the full main table
///
///
/// Note that the main-table aggregate cache (scan_numbers, array_ids,
/// observation_ids) requires a full sequential scan of the main table.  For
/// large MSes this scan happens once and the results are stored in sorted
/// `std::set` containers.
///
///
/// @par Example
/// Print a summary of antenna and field counts for an open MS:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   MsMetaData meta(ms);
///
///   std::cout << "Antennas: " << meta.n_antennas() << "\n";
///   for (const auto& name : meta.antenna_names())
///       std::cout << "  " << name << "\n";
///
///   std::cout << "Fields: " << meta.n_fields() << "\n";
///   for (const auto& name : meta.field_names())
///       std::cout << "  " << name << "\n";
/// @endcode
///
///
/// @par Example
/// Enumerate unique scan numbers present in the main table:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   MsMetaData meta(ms);
///
///   for (std::int32_t s : meta.scan_numbers())
///       std::cout << "scan " << s << "\n";
/// @endcode
///
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
