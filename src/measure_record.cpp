#include "casacore_mini/measure_record.hpp"

#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

// Helper: get a string field from a Record, throwing on missing/wrong type.
std::string require_string(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        throw std::invalid_argument("Missing field '" + std::string(key) + "' in measure record");
    }
    const auto* sp = std::get_if<std::string>(&val->storage());
    if (sp == nullptr) {
        throw std::invalid_argument("Field '" + std::string(key) +
                                    "' is not a string in measure record");
    }
    return *sp;
}

// Helper: get a sub-record field, throwing on missing/wrong type.
const Record& require_sub_record(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        throw std::invalid_argument("Missing sub-record '" + std::string(key) +
                                    "' in measure record");
    }
    const auto* rp = std::get_if<RecordValue::record_ptr>(&val->storage());
    if (rp == nullptr || *rp == nullptr) {
        throw std::invalid_argument("Field '" + std::string(key) +
                                    "' is not a sub-record in measure record");
    }
    return **rp;
}

// Helper: extract a Quantity {value, unit} from a sub-record.
struct QuantityFields {
    double value;
    std::string unit;
};

QuantityFields extract_quantity(const Record& rec) {
    const auto* val_field = rec.find("value");
    if (val_field == nullptr) {
        throw std::invalid_argument("Missing 'value' in quantity sub-record");
    }
    double value = 0.0;
    const auto& st = val_field->storage();
    if (const auto* dp = std::get_if<double>(&st)) {
        value = *dp;
    } else if (const auto* fp = std::get_if<float>(&st)) {
        value = static_cast<double>(*fp);
    } else {
        throw std::invalid_argument("Quantity 'value' is not a number");
    }

    std::string unit;
    const auto* unit_field = rec.find("unit");
    if (unit_field != nullptr) {
        if (const auto* sp = std::get_if<std::string>(&unit_field->storage())) {
            unit = *sp;
        }
    }

    return {value, std::move(unit)};
}

// Helper: create a quantity sub-record {value: double, unit: string}.
Record make_quantity_record(double value, const std::string& unit) {
    Record qrec;
    qrec.set("value", RecordValue(value));
    qrec.set("unit", RecordValue(unit));
    return qrec;
}

// Parse a reference string for a given measure type into the correct variant.
MeasureRefType parse_ref_for_type(MeasureType type, std::string_view refer) {
    switch (type) {
    case MeasureType::epoch:
        return string_to_epoch_ref(refer);
    case MeasureType::direction:
        return string_to_direction_ref(refer);
    case MeasureType::position:
        return string_to_position_ref(refer);
    case MeasureType::frequency:
        return string_to_frequency_ref(refer);
    case MeasureType::doppler:
        return string_to_doppler_ref(refer);
    case MeasureType::radial_velocity:
        return string_to_radial_velocity_ref(refer);
    case MeasureType::baseline:
        return string_to_baseline_ref(refer);
    case MeasureType::uvw:
        return string_to_uvw_ref(refer);
    case MeasureType::earth_magnetic:
        return string_to_earth_magnetic_ref(refer);
    }
    throw std::invalid_argument("Unknown measure type for reference parsing");
}

// Extract value components from m0, m1, m2 sub-records into a MeasureValue.
MeasureValue build_value(MeasureType type, const Record& rec) {
    const auto& m0 = require_sub_record(rec, "m0");
    auto q0 = extract_quantity(m0);

    switch (type) {
    // 1-component types
    case MeasureType::frequency:
        return FrequencyValue{q0.value};
    case MeasureType::doppler:
        return DopplerValue{q0.value};
    case MeasureType::radial_velocity:
        return RadialVelocityValue{q0.value};

    // 2-component types
    case MeasureType::epoch: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        return EpochValue{q0.value, q1.value};
    }
    case MeasureType::direction: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        return DirectionValue{q0.value, q1.value};
    }

    // 3-component types
    case MeasureType::position: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        const auto& m2 = require_sub_record(rec, "m2");
        auto q2 = extract_quantity(m2);
        return PositionValue{q0.value, q1.value, q2.value};
    }
    case MeasureType::baseline: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        const auto& m2 = require_sub_record(rec, "m2");
        auto q2 = extract_quantity(m2);
        return BaselineValue{q0.value, q1.value, q2.value};
    }
    case MeasureType::uvw: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        const auto& m2 = require_sub_record(rec, "m2");
        auto q2 = extract_quantity(m2);
        return UvwValue{q0.value, q1.value, q2.value};
    }
    case MeasureType::earth_magnetic: {
        const auto& m1 = require_sub_record(rec, "m1");
        auto q1 = extract_quantity(m1);
        const auto& m2 = require_sub_record(rec, "m2");
        auto q2 = extract_quantity(m2);
        return EarthMagneticValue{q0.value, q1.value, q2.value};
    }
    }

    throw std::invalid_argument("Unexpected measure type in build_value");
}

// Unit strings for value components per measure type.
struct ValueUnits {
    std::string u0;
    std::string u1;
    std::string u2;
};

// NOLINTBEGIN(bugprone-branch-clone) — different measure types intentionally share units
ValueUnits units_for_type(MeasureType type) {
    switch (type) {
    case MeasureType::epoch:
        return {"d", "d", ""};
    case MeasureType::direction:
        return {"rad", "rad", ""};
    case MeasureType::position:
        return {"m", "m", "m"};
    case MeasureType::frequency:
        return {"Hz", "", ""};
    case MeasureType::doppler:
        return {"", "", ""};
    case MeasureType::radial_velocity:
        return {"m/s", "", ""};
    case MeasureType::baseline:
        return {"m", "m", "m"};
    case MeasureType::uvw:
        return {"m", "m", "m"};
    case MeasureType::earth_magnetic:
        return {"T", "T", "T"};
    }
    return {"", "", ""}; // unreachable
}
// NOLINTEND(bugprone-branch-clone)

} // namespace

Record measure_to_record(const Measure& m) {
    Record rec;

    rec.set("type", RecordValue(std::string(measure_type_to_string(m.type))));
    rec.set("refer", RecordValue(measure_ref_to_string(m.ref.ref_type)));

    auto units = units_for_type(m.type);

    // NOLINTBEGIN(bugprone-branch-clone) — distinct value types can share field patterns
    std::visit(
        [&](const auto& val) {
            using val_t = std::decay_t<decltype(val)>; // NOLINT(readability-identifier-naming)
            if constexpr (std::is_same_v<val_t, EpochValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.day, units.u0)));
                rec.set("m1",
                        RecordValue::from_record(make_quantity_record(val.fraction, units.u1)));
            } else if constexpr (std::is_same_v<val_t, DirectionValue>) {
                rec.set("m0",
                        RecordValue::from_record(make_quantity_record(val.lon_rad, units.u0)));
                rec.set("m1",
                        RecordValue::from_record(make_quantity_record(val.lat_rad, units.u1)));
            } else if constexpr (std::is_same_v<val_t, PositionValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.x_m, units.u0)));
                rec.set("m1", RecordValue::from_record(make_quantity_record(val.y_m, units.u1)));
                rec.set("m2", RecordValue::from_record(make_quantity_record(val.z_m, units.u2)));
            } else if constexpr (std::is_same_v<val_t, FrequencyValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.hz, units.u0)));
            } else if constexpr (std::is_same_v<val_t, DopplerValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.ratio, units.u0)));
            } else if constexpr (std::is_same_v<val_t, RadialVelocityValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.mps, units.u0)));
            } else if constexpr (std::is_same_v<val_t, BaselineValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.x_m, units.u0)));
                rec.set("m1", RecordValue::from_record(make_quantity_record(val.y_m, units.u1)));
                rec.set("m2", RecordValue::from_record(make_quantity_record(val.z_m, units.u2)));
            } else if constexpr (std::is_same_v<val_t, UvwValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.u_m, units.u0)));
                rec.set("m1", RecordValue::from_record(make_quantity_record(val.v_m, units.u1)));
                rec.set("m2", RecordValue::from_record(make_quantity_record(val.w_m, units.u2)));
            } else if constexpr (std::is_same_v<val_t, EarthMagneticValue>) {
                rec.set("m0", RecordValue::from_record(make_quantity_record(val.x_t, units.u0)));
                rec.set("m1", RecordValue::from_record(make_quantity_record(val.y_t, units.u1)));
                rec.set("m2", RecordValue::from_record(make_quantity_record(val.z_t, units.u2)));
            }
        },
        m.value);
    // NOLINTEND(bugprone-branch-clone)

    if (m.ref.offset.has_value() && *m.ref.offset != nullptr) {
        rec.set("offset", RecordValue::from_record(measure_to_record(**m.ref.offset)));
    }

    return rec;
}

Measure measure_from_record(const Record& rec) {
    std::string type_str = require_string(rec, "type");
    MeasureType type = string_to_measure_type(type_str);

    std::string refer_str = require_string(rec, "refer");
    MeasureRefType ref_type = parse_ref_for_type(type, refer_str);

    MeasureValue value = build_value(type, rec);

    std::optional<std::shared_ptr<const Measure>> offset;
    const auto* offset_val = rec.find("offset");
    if (offset_val != nullptr) {
        const auto* rp = std::get_if<RecordValue::record_ptr>(&offset_val->storage());
        if (rp != nullptr && *rp != nullptr) {
            offset = std::make_shared<const Measure>(measure_from_record(**rp));
        }
    }

    return Measure{type, MeasureRef{ref_type, offset}, value};
}

} // namespace casacore_mini
