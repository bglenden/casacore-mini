// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_measure_desc.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Typed column wrappers for the MS main table and subtables.
///
/// These classes provide type-safe read access to MeasurementSet columns,
/// routing all I/O through the Table abstraction. Storage manager types
/// never appear here.

/// <summary>
/// Type-safe column accessor classes for the MeasurementSet main table
/// and its standard subtables.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> MeasurementSet — the MS container whose subtables are accessed
///   <li> Table — low-level cell read/write primitives
///   <li> Measure — measure type used by time and UVW readers
/// </prerequisite>
///
/// <synopsis>
/// Each class in this header wraps one table (the main table or a named
/// subtable) and exposes per-column reader methods whose return types match
/// the MS2 specification.  All I/O is delegated to the <src>Table</src>
/// abstraction; no storage-manager internals are visible here.
///
/// The available column wrappers are:
/// <ul>
///   <li> <src>MsMainColumns</src>     — main table (TIME, ANTENNA1/2, UVW, …)
///   <li> <src>MsAntennaColumns</src>  — ANTENNA subtable
///   <li> <src>MsSpWindowColumns</src> — SPECTRAL_WINDOW subtable
///   <li> <src>MsFieldColumns</src>    — FIELD subtable
///   <li> <src>MsDataDescColumns</src> — DATA_DESCRIPTION subtable
///   <li> <src>MsPolarizationColumns</src> — POLARIZATION subtable
///   <li> <src>MsObservationColumns</src>  — OBSERVATION subtable
/// </ul>
///
/// Measure-aware accessors (<src>time_measure</src>, <src>uvw_measure</src>)
/// lazily load the column's measure descriptor from the keyword set on first
/// call and cache it for subsequent reads.
/// </synopsis>
///
/// <example>
/// Read antenna names and UVW coordinates from an open MS:
/// <srcblock>
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   MsAntennaColumns ant(ms);
///   for (std::uint64_t r = 0; r < ant.row_count(); ++r)
///       std::cout << r << ": " << ant.name(r) << "\n";
///
///   MsMainColumns main(ms);
///   for (std::uint64_t r = 0; r < main.row_count(); ++r) {
///       auto uvw = main.uvw(r);           // std::vector<double> [u,v,w]
///       auto t   = main.time_measure(r);  // Measure in UTC seconds
///   }
/// </srcblock>
/// </example>
///
/// <example>
/// Read spectral window reference frequencies:
/// <srcblock>
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   MsSpWindowColumns spw(ms);
///   for (std::uint64_t r = 0; r < spw.row_count(); ++r)
///       std::cout << "SPW " << r << " ref_freq=" << spw.ref_frequency(r) << " Hz\n";
/// </srcblock>
/// </example>

/// Typed column accessors for the MS main table.
///
/// Provides typed read methods for all 21 required columns plus optional
/// array columns (DATA, FLOAT_DATA, etc.). All reads go through Table.
class MsMainColumns {
  public:
    explicit MsMainColumns(MeasurementSet& ms);

    // --- Scalar column readers ---

    [[nodiscard]] std::int32_t antenna1(std::uint64_t row) const;
    [[nodiscard]] std::int32_t antenna2(std::uint64_t row) const;
    [[nodiscard]] std::int32_t array_id(std::uint64_t row) const;
    [[nodiscard]] std::int32_t data_desc_id(std::uint64_t row) const;
    [[nodiscard]] double exposure(std::uint64_t row) const;
    [[nodiscard]] std::int32_t feed1(std::uint64_t row) const;
    [[nodiscard]] std::int32_t feed2(std::uint64_t row) const;
    [[nodiscard]] std::int32_t field_id(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;
    [[nodiscard]] double interval(std::uint64_t row) const;
    [[nodiscard]] std::int32_t observation_id(std::uint64_t row) const;
    [[nodiscard]] std::int32_t processor_id(std::uint64_t row) const;
    [[nodiscard]] std::int32_t scan_number(std::uint64_t row) const;
    [[nodiscard]] std::int32_t state_id(std::uint64_t row) const;
    [[nodiscard]] double time(std::uint64_t row) const;
    [[nodiscard]] double time_centroid(std::uint64_t row) const;

    // --- Array column readers ---

    /// UVW coordinates [u, v, w] in meters.
    [[nodiscard]] std::vector<double> uvw(std::uint64_t row) const;
    /// Sigma per correlation.
    [[nodiscard]] std::vector<float> sigma(std::uint64_t row) const;
    /// Weight per correlation.
    [[nodiscard]] std::vector<float> weight(std::uint64_t row) const;

    // --- Measure column readers ---

    /// Read TIME as an MEpoch measure (UTC, seconds).
    [[nodiscard]] Measure time_measure(std::uint64_t row) const;
    /// Read UVW as an Muvw measure (J2000, meters).
    [[nodiscard]] Measure uvw_measure(std::uint64_t row) const;

    /// Number of rows in the main table.
    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    void ensure_measure_descs() const;

    Table& table_;
    mutable std::optional<TableMeasDesc> time_meas_desc_;
    mutable std::optional<TableMeasDesc> uvw_meas_desc_;
    mutable bool measure_descs_loaded_ = false;
};

/// Typed column accessors for the ANTENNA subtable.
///
/// Each row describes one physical antenna: its name, station pad,
/// mount type, ITRF position, feed offset, and dish diameter.
class MsAntennaColumns {
  public:
    explicit MsAntennaColumns(MeasurementSet& ms);

    [[nodiscard]] std::string name(std::uint64_t row) const;
    [[nodiscard]] std::string station(std::uint64_t row) const;
    [[nodiscard]] std::string type(std::uint64_t row) const;
    [[nodiscard]] std::string mount(std::uint64_t row) const;
    [[nodiscard]] std::vector<double> position(std::uint64_t row) const;
    [[nodiscard]] std::vector<double> offset(std::uint64_t row) const;
    [[nodiscard]] double dish_diameter(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

/// Typed column accessors for the SPECTRAL_WINDOW subtable.
///
/// Each row defines one frequency setup: number of channels, reference
/// frequency, frequency measure reference code, and total bandwidth.
class MsSpWindowColumns {
  public:
    explicit MsSpWindowColumns(MeasurementSet& ms);

    [[nodiscard]] std::int32_t num_chan(std::uint64_t row) const;
    [[nodiscard]] std::string name(std::uint64_t row) const;
    [[nodiscard]] double ref_frequency(std::uint64_t row) const;
    [[nodiscard]] std::int32_t meas_freq_ref(std::uint64_t row) const;
    [[nodiscard]] double total_bandwidth(std::uint64_t row) const;
    [[nodiscard]] std::int32_t net_sideband(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

/// Typed column accessors for the FIELD subtable.
///
/// Each row names a target field and stores its phase/delay/reference
/// direction polynomial coefficients and the epoch at which they apply.
class MsFieldColumns {
  public:
    explicit MsFieldColumns(MeasurementSet& ms);

    [[nodiscard]] std::string name(std::uint64_t row) const;
    [[nodiscard]] std::string code(std::uint64_t row) const;
    [[nodiscard]] double time(std::uint64_t row) const;
    [[nodiscard]] std::int32_t num_poly(std::uint64_t row) const;
    [[nodiscard]] std::int32_t source_id(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

/// Typed column accessors for the DATA_DESCRIPTION subtable.
///
/// Each row pairs one spectral window with one polarisation setup, forming
/// the (SPW, POL) combination that identifies a data description ID used in
/// the main table's DATA_DESC_ID column.
class MsDataDescColumns {
  public:
    explicit MsDataDescColumns(MeasurementSet& ms);

    [[nodiscard]] std::int32_t spectral_window_id(std::uint64_t row) const;
    [[nodiscard]] std::int32_t polarization_id(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

/// Typed column accessors for the POLARIZATION subtable.
///
/// Each row defines the number and types of the Stokes correlations produced
/// by the correlator for a given polarisation setup.
class MsPolarizationColumns {
  public:
    explicit MsPolarizationColumns(MeasurementSet& ms);

    [[nodiscard]] std::int32_t num_corr(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

/// Typed column accessors for the OBSERVATION subtable.
///
/// Each row records top-level observation metadata: the telescope name,
/// observer name, project code, and data release date.
class MsObservationColumns {
  public:
    explicit MsObservationColumns(MeasurementSet& ms);

    [[nodiscard]] std::string telescope_name(std::uint64_t row) const;
    [[nodiscard]] std::string observer(std::uint64_t row) const;
    [[nodiscard]] std::string project(std::uint64_t row) const;
    [[nodiscard]] double release_date(std::uint64_t row) const;
    [[nodiscard]] bool flag_row(std::uint64_t row) const;

    [[nodiscard]] std::uint64_t row_count() const noexcept;

  private:
    Table& table_;
};

} // namespace casacore_mini
