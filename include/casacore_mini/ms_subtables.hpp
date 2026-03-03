// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table_desc.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Subtable column enums and schema factories for all 17 MS subtable types.

/// 
/// Column enumerations and schema factory functions for all 17 MeasurementSet
/// subtable types (12 required + 5 optional).
/// 
///
///
///
/// @par Prerequisites
///   - TableDesc — ColumnDesc type produced by the factory functions
///   - MeasurementSet — the MS container that owns the subtable directories
/// 
///
/// 
/// This header provides two levels of API for working with MS subtables:
///
/// 1. Per-subtable column enumerations.
///    A typed `enum class` is defined for each of the 17 subtable
///    types.  The enumerators correspond to the required columns of that
///    subtable as specified by the MS2 standard.  These enums serve as safe
///    indices into the column name lookup functions below.
///
/// 2. Schema factory functions.
///    A `make_*_desc()` function is provided for every subtable.
///    Each function returns a `std::vector<ColumnDesc>` that
///    describes the required columns for that subtable, ready to be passed to
///    `make_subtable_dat()` and written to disk.
///
/// The 12 required subtable schemas are:
///
///   - ANTENNA — 8 columns (NAME, STATION, TYPE, MOUNT, POSITION, OFFSET,
///                  DISH_DIAMETER, FLAG_ROW)
///   - DATA_DESCRIPTION — 3 columns
///   - FEED — 12 columns
///   - FIELD — 9 columns (including direction polynomial arrays)
///   - FLAG_CMD — 8 columns
///   - HISTORY — 9 columns
///   - OBSERVATION — 9 columns
///   - POINTING — 9 columns
///   - POLARIZATION — 4 columns (NUM_CORR, CORR_TYPE, CORR_PRODUCT,
///                        FLAG_ROW)
///   - PROCESSOR — 5 columns
///   - SPECTRAL_WINDOW — 14 columns (NUM_CHAN, CHAN_FREQ, CHAN_WIDTH, …)
///   - STATE — 7 columns
///
///
/// The 5 optional subtable schemas are:
///
///   - DOPPLER, FREQ_OFFSET, SOURCE, SYSCAL, WEATHER
///
///
/// Column name strings for each enum value are accessible through the
/// corresponding `ms_*_column_name()` helper.
/// 
///
/// @par Example
/// Create the ANTENNA subtable schema and write it to an MS directory:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto cols = make_antenna_desc();
///   auto dat  = make_subtable_dat(cols);
///   // dat is now ready to be written under "my.ms/ANTENNA/"
/// @endcode
/// 
///
/// @par Example
/// Resolve a subtable name at runtime and obtain its column schema:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto cols = make_subtable_desc_by_name("SPECTRAL_WINDOW");
///   // cols.empty() == false for any recognised subtable name
/// @endcode
/// 
///
/// @par Example
/// Look up the wire name of a column enum value:
/// @code{.cpp}
///   using namespace casacore_mini;
///   std::string_view n = ms_antenna_column_name(MsAntennaColumn::dish_diameter);
///   // n == "DISH_DIAMETER"
/// @endcode
/// 

// ---------------------------------------------------------------------------
// Per-subtable column enums
// ---------------------------------------------------------------------------

// NOLINTNEXTLINE(performance-enum-size)
enum class MsAntennaColumn : std::int32_t {
    name,
    station,
    type,
    mount,
    position,
    offset,
    dish_diameter,
    flag_row
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsDataDescColumn : std::int32_t { spectral_window_id, polarization_id, flag_row };

// NOLINTNEXTLINE(performance-enum-size)
enum class MsFeedColumn : std::int32_t {
    antenna_id,
    feed_id,
    spectral_window_id,
    time,
    interval,
    num_receptors,
    beam_id,
    beam_offset,
    polarization_type,
    pol_response,
    position,
    receptor_angle
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsFieldColumn : std::int32_t {
    name,
    code,
    time,
    num_poly,
    delay_dir,
    phase_dir,
    reference_dir,
    source_id,
    flag_row
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsFlagCmdColumn : std::int32_t {
    time,
    interval,
    type,
    reason,
    level,
    severity,
    applied,
    command
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsHistoryColumn : std::int32_t {
    time,
    observation_id,
    message,
    priority,
    origin,
    object_id,
    application,
    cli_command,
    app_params
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsObservationColumn : std::int32_t {
    telescope_name,
    time_range,
    observer,
    log,
    schedule_type,
    schedule,
    project,
    release_date,
    flag_row
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsPointingColumn : std::int32_t {
    antenna_id,
    time,
    interval,
    name,
    num_poly,
    time_origin,
    direction,
    target,
    tracking
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsPolarizationColumn : std::int32_t { num_corr, corr_type, corr_product, flag_row };

// NOLINTNEXTLINE(performance-enum-size)
enum class MsProcessorColumn : std::int32_t { type, sub_type, type_id, mode_id, flag_row };

// NOLINTNEXTLINE(performance-enum-size)
enum class MsSpWindowColumn : std::int32_t {
    num_chan,
    name,
    ref_frequency,
    chan_freq,
    chan_width,
    effective_bw,
    resolution,
    meas_freq_ref,
    total_bandwidth,
    net_sideband,
    if_conv_chain,
    freq_group,
    freq_group_name,
    flag_row
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsStateColumn : std::int32_t { sig, ref, cal, load, sub_scan, obs_mode, flag_row };

// --- Optional subtable enums ---

// NOLINTNEXTLINE(performance-enum-size)
enum class MsDopplerColumn : std::int32_t { doppler_id, source_id, transition_id, veldef };

// NOLINTNEXTLINE(performance-enum-size)
enum class MsFreqOffsetColumn : std::int32_t {
    antenna1,
    antenna2,
    feed_id,
    spectral_window_id,
    time,
    interval,
    offset
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsSourceColumn : std::int32_t {
    source_id,
    time,
    interval,
    spectral_window_id,
    num_lines,
    name,
    calibration_group,
    code,
    direction,
    position,
    proper_motion,
    transition,
    rest_frequency,
    sysvel
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsSysCalColumn : std::int32_t {
    antenna_id,
    feed_id,
    spectral_window_id,
    time,
    interval,
    tsys,
    trx,
    tsky,
    phase_diff
};

// NOLINTNEXTLINE(performance-enum-size)
enum class MsWeatherColumn : std::int32_t {
    antenna_id,
    time,
    interval,
    temperature,
    pressure,
    rel_humidity,
    dew_point,
    wind_direction,
    wind_speed
};

// ---------------------------------------------------------------------------
// Schema factory functions
// ---------------------------------------------------------------------------

/// Create a TableDesc-compatible column list for the ANTENNA subtable.
[[nodiscard]] std::vector<ColumnDesc> make_antenna_desc();
/// Create column list for DATA_DESCRIPTION subtable.
[[nodiscard]] std::vector<ColumnDesc> make_data_description_desc();
/// Create column list for FEED subtable.
[[nodiscard]] std::vector<ColumnDesc> make_feed_desc();
/// Create column list for FIELD subtable.
[[nodiscard]] std::vector<ColumnDesc> make_field_desc();
/// Create column list for FLAG_CMD subtable.
[[nodiscard]] std::vector<ColumnDesc> make_flag_cmd_desc();
/// Create column list for HISTORY subtable.
[[nodiscard]] std::vector<ColumnDesc> make_history_desc();
/// Create column list for OBSERVATION subtable.
[[nodiscard]] std::vector<ColumnDesc> make_observation_desc();
/// Create column list for POINTING subtable.
[[nodiscard]] std::vector<ColumnDesc> make_pointing_desc();
/// Create column list for POLARIZATION subtable.
[[nodiscard]] std::vector<ColumnDesc> make_polarization_desc();
/// Create column list for PROCESSOR subtable.
[[nodiscard]] std::vector<ColumnDesc> make_processor_desc();
/// Create column list for SPECTRAL_WINDOW subtable.
[[nodiscard]] std::vector<ColumnDesc> make_spectral_window_desc();
/// Create column list for STATE subtable.
[[nodiscard]] std::vector<ColumnDesc> make_state_desc();

// Optional subtable factories:
/// Create column list for DOPPLER subtable.
[[nodiscard]] std::vector<ColumnDesc> make_doppler_desc();
/// Create column list for FREQ_OFFSET subtable.
[[nodiscard]] std::vector<ColumnDesc> make_freq_offset_desc();
/// Create column list for SOURCE subtable.
[[nodiscard]] std::vector<ColumnDesc> make_source_desc();
/// Create column list for SYSCAL subtable.
[[nodiscard]] std::vector<ColumnDesc> make_syscal_desc();
/// Create column list for WEATHER subtable.
[[nodiscard]] std::vector<ColumnDesc> make_weather_desc();

/// Get column descriptors for a subtable by name.
/// Returns empty vector if the name is not recognized.
[[nodiscard]] std::vector<ColumnDesc> make_subtable_desc_by_name(std::string_view name);

/// Get column name string for a subtable column enum value.
[[nodiscard]] std::string_view ms_antenna_column_name(MsAntennaColumn col);
[[nodiscard]] std::string_view ms_data_desc_column_name(MsDataDescColumn col);
[[nodiscard]] std::string_view ms_feed_column_name(MsFeedColumn col);
[[nodiscard]] std::string_view ms_field_column_name(MsFieldColumn col);
[[nodiscard]] std::string_view ms_flag_cmd_column_name(MsFlagCmdColumn col);
[[nodiscard]] std::string_view ms_history_column_name(MsHistoryColumn col);
[[nodiscard]] std::string_view ms_observation_column_name(MsObservationColumn col);
[[nodiscard]] std::string_view ms_pointing_column_name(MsPointingColumn col);
[[nodiscard]] std::string_view ms_polarization_column_name(MsPolarizationColumn col);
[[nodiscard]] std::string_view ms_processor_column_name(MsProcessorColumn col);
[[nodiscard]] std::string_view ms_spw_column_name(MsSpWindowColumn col);
[[nodiscard]] std::string_view ms_state_column_name(MsStateColumn col);
[[nodiscard]] std::string_view ms_doppler_column_name(MsDopplerColumn col);
[[nodiscard]] std::string_view ms_freq_offset_column_name(MsFreqOffsetColumn col);
[[nodiscard]] std::string_view ms_source_column_name(MsSourceColumn col);
[[nodiscard]] std::string_view ms_syscal_column_name(MsSysCalColumn col);
[[nodiscard]] std::string_view ms_weather_column_name(MsWeatherColumn col);

} // namespace casacore_mini
