// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/lattice_shape.hpp"

#include <stdexcept>

namespace casacore_mini {

// ── IPosition ──────────────────────────────────────────────────────────

IPosition::IPosition(std::size_t ndim, std::int64_t initial_value)
    : values_(ndim, initial_value) {}

IPosition::IPosition(std::initializer_list<std::int64_t> il)
    : values_(il) {}

IPosition::IPosition(std::vector<std::int64_t> values)
    : values_(std::move(values)) {}

std::int64_t IPosition::product() const noexcept {
    std::int64_t p = 1;
    for (auto v : values_) p *= v;
    return p;
}

std::vector<std::uint64_t> IPosition::to_unsigned() const {
    std::vector<std::uint64_t> result(values_.size());
    for (std::size_t i = 0; i < values_.size(); ++i) {
        result[i] = static_cast<std::uint64_t>(values_[i]);
    }
    return result;
}

IPosition IPosition::from_unsigned(std::span<const std::uint64_t> u) {
    std::vector<std::int64_t> values(u.size());
    for (std::size_t i = 0; i < u.size(); ++i) {
        values[i] = static_cast<std::int64_t>(u[i]);
    }
    return IPosition(std::move(values));
}

std::string IPosition::to_string() const {
    std::string s = "(";
    for (std::size_t i = 0; i < values_.size(); ++i) {
        if (i > 0) s += ", ";
        s += std::to_string(values_[i]);
    }
    s += ")";
    return s;
}

// ── Slicer ─────────────────────────────────────────────────────────────

Slicer Slicer::full(const IPosition& shape) {
    IPosition start(shape.ndim(), 0);
    IPosition stride(shape.ndim(), 1);
    return Slicer(std::move(start), IPosition(shape), std::move(stride));
}

Slicer::Slicer(IPosition start, IPosition length, IPosition stride)
    : start_(std::move(start)),
      length_(std::move(length)),
      stride_(std::move(stride)) {
    if (start_.ndim() != length_.ndim() || start_.ndim() != stride_.ndim()) {
        throw std::invalid_argument(
            "Slicer: rank mismatch (start=" + std::to_string(start_.ndim()) +
            ", length=" + std::to_string(length_.ndim()) +
            ", stride=" + std::to_string(stride_.ndim()) + ")");
    }
    for (std::size_t i = 0; i < stride_.ndim(); ++i) {
        if (stride_[i] < 1) {
            throw std::invalid_argument(
                "Slicer: stride[" + std::to_string(i) + "] = " +
                std::to_string(stride_[i]) + " must be >= 1");
        }
    }
}

Slicer::Slicer(IPosition start, const IPosition& length)
    : Slicer(std::move(start), IPosition(length),
             IPosition(length.ndim(), 1)) {}

// ── Free functions ─────────────────────────────────────────────────────

IPosition fortran_strides(const IPosition& shape) {
    IPosition strides(shape.ndim());
    if (shape.empty()) return strides;
    strides[0] = 1;
    for (std::size_t i = 1; i < shape.ndim(); ++i) {
        strides[i] = strides[i - 1] * shape[i - 1];
    }
    return strides;
}

std::int64_t linear_index(const IPosition& index,
                          const IPosition& strides) noexcept {
    std::int64_t offset = 0;
    for (std::size_t i = 0; i < index.ndim(); ++i) {
        offset += index[i] * strides[i];
    }
    return offset;
}

IPosition delinearize(std::int64_t offset, const IPosition& shape) {
    IPosition result(shape.ndim());
    for (std::size_t i = 0; i < shape.ndim(); ++i) {
        result[i] = offset % shape[i];
        offset /= shape[i];
    }
    return result;
}

void validate_index(const IPosition& index, const IPosition& shape) {
    if (index.ndim() != shape.ndim()) {
        throw std::out_of_range(
            "validate_index: rank mismatch (index=" +
            std::to_string(index.ndim()) +
            ", shape=" + std::to_string(shape.ndim()) + ")");
    }
    for (std::size_t i = 0; i < index.ndim(); ++i) {
        if (index[i] < 0 || index[i] >= shape[i]) {
            throw std::out_of_range(
                "validate_index: axis " + std::to_string(i) +
                " index " + std::to_string(index[i]) +
                " out of range [0, " + std::to_string(shape[i]) + ")");
        }
    }
}

void validate_slicer(const Slicer& slicer, const IPosition& shape) {
    if (slicer.ndim() != shape.ndim()) {
        throw std::out_of_range(
            "validate_slicer: rank mismatch (slicer=" +
            std::to_string(slicer.ndim()) +
            ", shape=" + std::to_string(shape.ndim()) + ")");
    }
    for (std::size_t i = 0; i < slicer.ndim(); ++i) {
        auto end = slicer.start()[i] +
                   (slicer.length()[i] - 1) * slicer.stride()[i];
        if (slicer.start()[i] < 0 || end >= shape[i]) {
            throw std::out_of_range(
                "validate_slicer: axis " + std::to_string(i) +
                " slice [" + std::to_string(slicer.start()[i]) +
                ", " + std::to_string(end) +
                "] exceeds shape " + std::to_string(shape[i]));
        }
    }
}

} // namespace casacore_mini
