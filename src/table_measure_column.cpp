#include "casacore_mini/table_measure_column.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

// Number of value components for a given measure type.
std::size_t value_count(MeasureType type) {
    switch (type) {
    case MeasureType::frequency:
    case MeasureType::doppler:
    case MeasureType::radial_velocity:
        return 1;
    case MeasureType::epoch:
    case MeasureType::direction:
        return 2;
    case MeasureType::position:
    case MeasureType::baseline:
    case MeasureType::uvw:
    case MeasureType::earth_magnetic:
        return 3;
    }
    return 1; // unreachable
}

// Extract a double from a CellValue.
double cell_to_double(const CellValue& v) {
    if (const auto* dp = std::get_if<double>(&v)) {
        return *dp;
    }
    if (const auto* fp = std::get_if<float>(&v)) {
        return static_cast<double>(*fp);
    }
    if (const auto* ip = std::get_if<std::int32_t>(&v)) {
        return static_cast<double>(*ip);
    }
    if (const auto* lp = std::get_if<std::int64_t>(&v)) {
        return static_cast<double>(*lp);
    }
    throw std::invalid_argument("Cell value is not numeric");
}

// Resolve the reference frame for a row, using either fixed or variable ref.
MeasureRefType resolve_ref(const TableMeasDesc& desc, const ScalarCellReader& read_scalar,
                           std::uint64_t row) {
    if (!desc.has_variable_ref()) {
        return desc.default_ref;
    }

    CellValue ref_cell = read_scalar(desc.var_ref_column, row);

    // String reference column.
    if (const auto* sp = std::get_if<std::string>(&ref_cell)) {
        switch (desc.measure_type) {
        case MeasureType::epoch:
            return string_to_epoch_ref(*sp);
        case MeasureType::direction:
            return string_to_direction_ref(*sp);
        case MeasureType::position:
            return string_to_position_ref(*sp);
        case MeasureType::frequency:
            return string_to_frequency_ref(*sp);
        case MeasureType::doppler:
            return string_to_doppler_ref(*sp);
        case MeasureType::radial_velocity:
            return string_to_radial_velocity_ref(*sp);
        case MeasureType::baseline:
            return string_to_baseline_ref(*sp);
        case MeasureType::uvw:
            return string_to_uvw_ref(*sp);
        case MeasureType::earth_magnetic:
            return string_to_earth_magnetic_ref(*sp);
        }
    }

    // Integer reference column — use TabRefCodes/TabRefTypes mapping.
    std::int32_t code = 0;
    if (const auto* ip = std::get_if<std::int32_t>(&ref_cell)) {
        code = *ip;
    } else if (const auto* up = std::get_if<std::uint32_t>(&ref_cell)) {
        code = static_cast<std::int32_t>(*up);
    } else {
        throw std::invalid_argument("Variable reference column has unexpected type");
    }

    // Map code through TabRefCodes -> TabRefTypes.
    if (!desc.tab_ref_codes.empty() && !desc.tab_ref_types.empty()) {
        for (std::size_t i = 0; i < desc.tab_ref_codes.size(); ++i) {
            if (static_cast<std::int32_t>(desc.tab_ref_codes[i]) == code) {
                switch (desc.measure_type) {
                case MeasureType::epoch:
                    return string_to_epoch_ref(desc.tab_ref_types[i]);
                case MeasureType::direction:
                    return string_to_direction_ref(desc.tab_ref_types[i]);
                case MeasureType::position:
                    return string_to_position_ref(desc.tab_ref_types[i]);
                case MeasureType::frequency:
                    return string_to_frequency_ref(desc.tab_ref_types[i]);
                case MeasureType::doppler:
                    return string_to_doppler_ref(desc.tab_ref_types[i]);
                case MeasureType::radial_velocity:
                    return string_to_radial_velocity_ref(desc.tab_ref_types[i]);
                case MeasureType::baseline:
                    return string_to_baseline_ref(desc.tab_ref_types[i]);
                case MeasureType::uvw:
                    return string_to_uvw_ref(desc.tab_ref_types[i]);
                case MeasureType::earth_magnetic:
                    return string_to_earth_magnetic_ref(desc.tab_ref_types[i]);
                }
            }
        }
    }

    throw std::invalid_argument("Unknown reference code " + std::to_string(code) + " for column '" +
                                desc.column_name + "'");
}

// Build a MeasureValue from raw doubles.
MeasureValue build_value_from_doubles(MeasureType type, const double* values, std::size_t count) {
    std::size_t need = value_count(type);
    if (count < need) {
        throw std::invalid_argument("Expected " + std::to_string(need) + " value components, got " +
                                    std::to_string(count));
    }

    switch (type) {
    case MeasureType::epoch:
        return EpochValue{values[0], values[1]};
    case MeasureType::direction:
        return DirectionValue{values[0], values[1]};
    case MeasureType::position:
        return PositionValue{values[0], values[1], values[2]};
    case MeasureType::frequency:
        return FrequencyValue{values[0]};
    case MeasureType::doppler:
        return DopplerValue{values[0]};
    case MeasureType::radial_velocity:
        return RadialVelocityValue{values[0]};
    case MeasureType::baseline:
        return BaselineValue{values[0], values[1], values[2]};
    case MeasureType::uvw:
        return UvwValue{values[0], values[1], values[2]};
    case MeasureType::earth_magnetic:
        return EarthMagneticValue{values[0], values[1], values[2]};
    }

    throw std::invalid_argument("Unknown measure type in build_value_from_doubles");
}

// Extract doubles from a MeasureValue into a vector.
void extract_doubles(const MeasureValue& val, std::vector<double>& out) {
    // NOLINTBEGIN(bugprone-branch-clone) — distinct value types can share field patterns
    std::visit(
        [&out](const auto& v) {
            using vt = std::decay_t<decltype(v)>; // NOLINT(readability-identifier-naming)
            if constexpr (std::is_same_v<vt, EpochValue>) {
                out.push_back(v.day);
                out.push_back(v.fraction);
            } else if constexpr (std::is_same_v<vt, DirectionValue>) {
                out.push_back(v.lon_rad);
                out.push_back(v.lat_rad);
            } else if constexpr (std::is_same_v<vt, PositionValue>) {
                out.push_back(v.x_m);
                out.push_back(v.y_m);
                out.push_back(v.z_m);
            } else if constexpr (std::is_same_v<vt, FrequencyValue>) {
                out.push_back(v.hz);
            } else if constexpr (std::is_same_v<vt, DopplerValue>) {
                out.push_back(v.ratio);
            } else if constexpr (std::is_same_v<vt, RadialVelocityValue>) {
                out.push_back(v.mps);
            } else if constexpr (std::is_same_v<vt, BaselineValue>) {
                out.push_back(v.x_m);
                out.push_back(v.y_m);
                out.push_back(v.z_m);
            } else if constexpr (std::is_same_v<vt, UvwValue>) {
                out.push_back(v.u_m);
                out.push_back(v.v_m);
                out.push_back(v.w_m);
            } else if constexpr (std::is_same_v<vt, EarthMagneticValue>) {
                out.push_back(v.x_t);
                out.push_back(v.y_t);
                out.push_back(v.z_t);
            }
        },
        val);
    // NOLINTEND(bugprone-branch-clone)
}

} // namespace

Measure read_scalar_measure(const TableMeasDesc& desc, const ScalarCellReader& read_scalar,
                            std::uint64_t row) {
    std::size_t nvals = value_count(desc.measure_type);
    double values[3] = {0.0, 0.0, 0.0}; // NOLINT(modernize-avoid-c-arrays)

    // Read the first (or only) component as a scalar Double.
    // For single-component measures (epoch as MJD, frequency as Hz) this is the
    // full value. Multi-component scalar measures should use read_array_measures.
    values[0] = cell_to_double(read_scalar(desc.column_name, row));

    MeasureRefType ref = resolve_ref(desc, read_scalar, row);
    MeasureValue val = build_value_from_doubles(desc.measure_type, values, nvals);

    return Measure{desc.measure_type, MeasureRef{ref, std::nullopt}, val};
}

std::vector<Measure> read_array_measures(const TableMeasDesc& desc,
                                         const ArrayCellReader& read_array,
                                         const ScalarCellReader& read_scalar, std::uint64_t row) {
    std::vector<double> data = read_array(desc.column_name, row);
    std::size_t ncomp = value_count(desc.measure_type);
    if (data.empty() || data.size() % ncomp != 0) {
        throw std::invalid_argument("Array data size " + std::to_string(data.size()) +
                                    " not divisible by " + std::to_string(ncomp) + " for column '" +
                                    desc.column_name + "'");
    }

    std::size_t nmeas = data.size() / ncomp;
    MeasureRefType ref = resolve_ref(desc, read_scalar, row);

    std::vector<Measure> result;
    result.reserve(nmeas);
    for (std::size_t i = 0; i < nmeas; ++i) {
        MeasureValue val = build_value_from_doubles(desc.measure_type, &data[i * ncomp], ncomp);
        result.push_back(Measure{desc.measure_type, MeasureRef{ref, std::nullopt}, val});
    }

    return result;
}

void write_scalar_measure(const TableMeasDesc& desc, const ScalarCellWriter& write_scalar,
                          std::size_t col_index, std::uint64_t row, const Measure& m) {
    (void)desc; // Used for type consistency; multi-component writes use array path.
    std::vector<double> values;
    extract_doubles(m.value, values);

    // For scalar measures, write the first component value.
    // Multi-component measures are typically stored in array columns instead.
    write_scalar(col_index, row, CellValue{values[0]});
}

void write_array_measures(const TableMeasDesc& desc,
                          const std::function<void(const std::string& col_name, std::uint64_t row,
                                                   const std::vector<double>& values)>& write_fn,
                          std::uint64_t row, const std::vector<Measure>& measures) {
    std::vector<double> flat;
    for (const auto& m : measures) {
        extract_doubles(m.value, flat);
    }
    write_fn(desc.column_name, row, flat);
}

} // namespace casacore_mini
