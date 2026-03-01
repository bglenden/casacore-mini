#include "casacore_mini/spectral_coordinate.hpp"

#include <stdexcept>
#include <string>

namespace casacore_mini {

SpectralCoordinate::SpectralCoordinate(FrequencyRef ref_frame, double crval_hz, double cdelt_hz,
                                       double crpix, double rest_freq_hz)
    : ref_frame_(ref_frame), crval_hz_(crval_hz), cdelt_hz_(cdelt_hz), crpix_(crpix),
      rest_freq_hz_(rest_freq_hz) {}

std::vector<std::string> SpectralCoordinate::world_axis_names() const {
    return {"Frequency"};
}
std::vector<std::string> SpectralCoordinate::world_axis_units() const {
    return {"Hz"};
}
std::vector<double> SpectralCoordinate::reference_value() const {
    return {crval_hz_};
}
std::vector<double> SpectralCoordinate::reference_pixel() const {
    return {crpix_};
}
std::vector<double> SpectralCoordinate::increment() const {
    return {cdelt_hz_};
}

std::vector<double> SpectralCoordinate::to_world(const std::vector<double>& pixel) const {
    if (pixel.size() != 1) {
        throw std::invalid_argument("SpectralCoordinate::to_world: expected 1 pixel axis");
    }
    return {crval_hz_ + cdelt_hz_ * (pixel[0] - crpix_)};
}

std::vector<double> SpectralCoordinate::to_pixel(const std::vector<double>& world) const {
    if (world.size() != 1) {
        throw std::invalid_argument("SpectralCoordinate::to_pixel: expected 1 world axis");
    }
    if (cdelt_hz_ == 0.0) {
        throw std::invalid_argument("SpectralCoordinate: zero channel width");
    }
    return {crpix_ + (world[0] - crval_hz_) / cdelt_hz_};
}

Record SpectralCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("spectral")));

    // Version 2 format expected by casacore's SpectralCoordinate::restore().
    // Use WCS sub-record fields for broad image-table interoperability.
    rec.set("version", RecordValue(static_cast<std::int32_t>(2)));
    rec.set("system", RecordValue(std::string(frequency_ref_to_string(ref_frame_))));
    rec.set("restfreq", RecordValue(rest_freq_hz_));
    rec.set("restfreqs", RecordValue(RecordValue::double_array{{1}, {rest_freq_hz_}}));
    rec.set("unit", RecordValue(std::string("Hz")));
    rec.set("name", RecordValue(std::string("Frequency")));

    // WCS sub-record with scalar crval/crpix/cdelt/pc/ctype.
    Record wcs;
    wcs.set("crval", RecordValue(crval_hz_));
    wcs.set("crpix", RecordValue(crpix_));
    wcs.set("cdelt", RecordValue(cdelt_hz_));
    wcs.set("pc", RecordValue(1.0));
    wcs.set("ctype", RecordValue(std::string("FREQ")));
    rec.set("wcs", RecordValue::from_record(wcs));

    return rec;
}

std::unique_ptr<Coordinate> SpectralCoordinate::clone() const {
    return std::make_unique<SpectralCoordinate>(ref_frame_, crval_hz_, cdelt_hz_, crpix_,
                                                rest_freq_hz_);
}

std::unique_ptr<SpectralCoordinate> SpectralCoordinate::from_record(const Record& rec) {
    FrequencyRef ref = FrequencyRef::lsrk; // default
    if (const auto* v = rec.find("system")) {
        if (const auto* sp = std::get_if<std::string>(&v->storage())) {
            ref = string_to_frequency_ref(*sp);
        }
    }

    auto get_double = [](const Record& r, const char* key, double dflt) -> double {
        const auto* v = r.find(key);
        if (v == nullptr) {
            return dflt;
        }
        if (const auto* dp = std::get_if<double>(&v->storage())) {
            return *dp;
        }
        return dflt;
    };

    double restfreq = get_double(rec, "restfreq", 0.0);

    // Version 2: read from "wcs" sub-record if present.
    if (const auto* wv = rec.find("wcs")) {
        if (const auto* wp = std::get_if<RecordValue::record_ptr>(&wv->storage());
            wp != nullptr && *wp != nullptr) {
            const auto& wrec = **wp;
            return std::make_unique<SpectralCoordinate>(ref, get_double(wrec, "crval", 0.0),
                                                        get_double(wrec, "cdelt", 1.0),
                                                        get_double(wrec, "crpix", 0.0), restfreq);
        }
    }

    // Legacy / v1 fallback: crval/cdelt/crpix at top level.
    return std::make_unique<SpectralCoordinate>(ref, get_double(rec, "crval", 0.0),
                                                get_double(rec, "cdelt", 1.0),
                                                get_double(rec, "crpix", 0.0), restfreq);
}

} // namespace casacore_mini
