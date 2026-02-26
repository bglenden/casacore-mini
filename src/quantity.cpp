#include "casacore_mini/quantity.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {

Quantity::Quantity() = default;

Quantity::Quantity(double v, std::string u) : value(v), unit_(std::move(u)) {}

Quantity::Quantity(double v, const Unit& u) : value(v), unit_(u) {}

// ===== Conversion ==========================================================

Quantity Quantity::convert(const Unit& target) const {
    if (!unit_.defined()) {
        throw std::invalid_argument("Unknown source unit: \"" + unit_.name() + "\"");
    }
    if (!target.defined()) {
        throw std::invalid_argument("Unknown target unit: \"" + target.name() + "\"");
    }
    if (!unit_.value().conforms(target.value())) {
        throw std::invalid_argument(
            "Incompatible units: \"" + unit_.name() + "\" -> \"" + target.name() + "\"");
    }
    double result = value * unit_.value().factor() / target.value().factor();
    return Quantity(result, target);
}

Quantity Quantity::convert(std::string_view target) const {
    return convert(Unit(target));
}

double Quantity::get_value(std::string_view target) const {
    return convert(target).value;
}

// ===== Dimension checking ==================================================

bool Quantity::conforms(const Unit& other) const {
    return unit_.defined() && other.defined() && unit_.value().conforms(other.value());
}

bool Quantity::conforms(const Quantity& other) const {
    return unit_.defined() && other.unit_.defined() &&
           unit_.value().conforms(other.unit_.value());
}

// ===== Arithmetic ==========================================================

Quantity Quantity::operator+(const Quantity& rhs) const {
    if (!conforms(rhs)) {
        throw std::invalid_argument(
            "Cannot add non-conformant units: \"" + unit_.name() + "\" + \"" +
            rhs.unit_.name() + "\"");
    }
    double rhs_val = rhs.value * rhs.unit_.value().factor() / unit_.value().factor();
    return Quantity(value + rhs_val, unit_);
}

Quantity Quantity::operator-(const Quantity& rhs) const {
    if (!conforms(rhs)) {
        throw std::invalid_argument(
            "Cannot subtract non-conformant units: \"" + unit_.name() + "\" - \"" +
            rhs.unit_.name() + "\"");
    }
    double rhs_val = rhs.value * rhs.unit_.value().factor() / unit_.value().factor();
    return Quantity(value - rhs_val, unit_);
}

Quantity Quantity::operator*(const Quantity& rhs) const {
    std::string new_name;
    if (unit_.name().empty() && rhs.unit_.name().empty()) {
        // both dimensionless
    } else if (unit_.name().empty()) {
        new_name = rhs.unit_.name();
    } else if (rhs.unit_.name().empty()) {
        new_name = unit_.name();
    } else {
        new_name = "(" + unit_.name() + ").(" + rhs.unit_.name() + ")";
    }
    return Quantity(value * rhs.value, Unit(new_name));
}

Quantity Quantity::operator/(const Quantity& rhs) const {
    std::string new_name;
    if (unit_.name().empty() && rhs.unit_.name().empty()) {
        // both dimensionless
    } else if (rhs.unit_.name().empty()) {
        new_name = unit_.name();
    } else if (unit_.name().empty()) {
        new_name = "(" + rhs.unit_.name() + ")-1";
    } else {
        new_name = "(" + unit_.name() + ")/(" + rhs.unit_.name() + ")";
    }
    return Quantity(value / rhs.value, Unit(new_name));
}

Quantity Quantity::operator*(double scale) const {
    return Quantity(value * scale, unit_);
}

Quantity Quantity::operator/(double scale) const {
    return Quantity(value / scale, unit_);
}

Quantity Quantity::operator-() const {
    return Quantity(-value, unit_);
}

// ===== Comparison ==========================================================

bool Quantity::operator==(const Quantity& rhs) const {
    if (!conforms(rhs)) {
        return false;
    }
    double lhs_si = value * unit_.value().factor();
    double rhs_si = rhs.value * rhs.unit_.value().factor();
    return lhs_si == rhs_si;
}

bool Quantity::operator<(const Quantity& rhs) const {
    if (!conforms(rhs)) {
        throw std::invalid_argument(
            "Cannot compare non-conformant units: \"" + unit_.name() + "\" vs \"" +
            rhs.unit_.name() + "\"");
    }
    double lhs_si = value * unit_.value().factor();
    double rhs_si = rhs.value * rhs.unit_.value().factor();
    return lhs_si < rhs_si;
}

bool Quantity::operator>(const Quantity& rhs) const {
    return rhs < *this;
}

bool Quantity::operator<=(const Quantity& rhs) const {
    return !(rhs < *this);
}

bool Quantity::operator>=(const Quantity& rhs) const {
    return !(*this < rhs);
}

// ===== Free function (backward compat) =====================================

Quantity convert_quantity(const Quantity& q, std::string_view target_unit) {
    return q.convert(target_unit);
}

} // namespace casacore_mini
