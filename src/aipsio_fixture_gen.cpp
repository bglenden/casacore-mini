// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// Fixture generator: produces binary Record .bin files matching existing
/// showtableinfo text fixture structures. Run once, commit output to git.

#include "casacore_mini/aipsio_record_writer.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

namespace {

using casacore_mini::Record;
using casacore_mini::RecordValue;

void write_bin(const std::filesystem::path& path, const Record& record) {
    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, record);
    auto bytes = writer.take_bytes();

    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot open output: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    std::cout << "wrote " << bytes.size() << " bytes to " << path << '\n';
}

/// logtable_stdstman_keywords/column_TIME_keywords.bin
/// Matches: UNIT="s", MEASURE_TYPE="EPOCH", MEASURE_REFERENCE="UTC"
Record build_logtable_time_keywords() {
    Record record;
    record.set("UNIT", RecordValue("s"));
    record.set("MEASURE_TYPE", RecordValue("EPOCH"));
    record.set("MEASURE_REFERENCE", RecordValue("UTC"));
    return record;
}

/// ms_tree/table_keywords.bin
/// Matches: MS_VERSION (Float 2), subtable references (String).
Record build_ms_table_keywords() {
    Record record;
    record.set("MS_VERSION", RecordValue(2.0F));
    record.set("ANTENNA", RecordValue("Table path/ANTENNA"));
    record.set("DATA_DESCRIPTION", RecordValue("Table path/DATA_DESCRIPTION"));
    record.set("FEED", RecordValue("Table path/FEED"));
    record.set("FLAG_CMD", RecordValue("Table path/FLAG_CMD"));
    record.set("FIELD", RecordValue("Table path/FIELD"));
    record.set("HISTORY", RecordValue("Table path/HISTORY"));
    record.set("OBSERVATION", RecordValue("Table path/OBSERVATION"));
    record.set("POINTING", RecordValue("Table path/POINTING"));
    record.set("POLARIZATION", RecordValue("Table path/POLARIZATION"));
    record.set("PROCESSOR", RecordValue("Table path/PROCESSOR"));
    record.set("SPECTRAL_WINDOW", RecordValue("Table path/SPECTRAL_WINDOW"));
    record.set("STATE", RecordValue("Table path/STATE"));
    return record;
}

/// ms_tree/column_UVW_keywords.bin
/// Matches: QuantumUnits string array [m,m,m], MEASINFO sub-record {type, Ref}.
Record build_ms_uvw_keywords() {
    Record record;

    RecordValue::string_array units;
    units.shape = {3};
    units.elements = {"m", "m", "m"};
    record.set("QuantumUnits", RecordValue(std::move(units)));

    Record measinfo;
    measinfo.set("type", RecordValue("uvw"));
    measinfo.set("Ref", RecordValue("ITRF"));
    record.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    return record;
}

/// pagedimage_coords/table_keywords.bin
/// Matches: logtable (string), coords sub-record with nested structure.
Record build_pagedimage_table_keywords() {
    Record record;
    record.set("logtable", RecordValue("Table path/logtable"));

    // Build coords sub-record.
    Record coords;
    coords.set("telescope", RecordValue("UNKNOWN"));
    coords.set("observer", RecordValue("UNKNOWN"));

    // obsdate
    Record obsdate;
    obsdate.set("type", RecordValue("epoch"));
    obsdate.set("refer", RecordValue("LAST"));
    Record m0;
    m0.set("value", RecordValue(0.0));
    m0.set("unit", RecordValue("d"));
    obsdate.set("m0", RecordValue::from_record(std::move(m0)));
    coords.set("obsdate", RecordValue::from_record(std::move(obsdate)));

    // pointingcenter
    Record pointingcenter;
    RecordValue::double_array pc_value;
    pc_value.shape = {2};
    pc_value.elements = {0.0, 0.0};
    pointingcenter.set("value", RecordValue(std::move(pc_value)));
    pointingcenter.set("initial", RecordValue(true));
    coords.set("pointingcenter", RecordValue::from_record(std::move(pointingcenter)));

    // direction0
    Record direction0;
    direction0.set("system", RecordValue("J2000"));
    direction0.set("projection", RecordValue("SIN"));

    RecordValue::double_array proj_params;
    proj_params.shape = {2};
    proj_params.elements = {0.0, 0.0};
    direction0.set("projection_parameters", RecordValue(std::move(proj_params)));

    RecordValue::double_array crval;
    crval.shape = {2};
    crval.elements = {0.0, 0.0};
    direction0.set("crval", RecordValue(std::move(crval)));

    RecordValue::double_array crpix;
    crpix.shape = {2};
    crpix.elements = {0.0, 0.0};
    direction0.set("crpix", RecordValue(std::move(crpix)));

    RecordValue::double_array cdelt;
    cdelt.shape = {2};
    cdelt.elements = {-1.0, 1.0};
    direction0.set("cdelt", RecordValue(std::move(cdelt)));

    RecordValue::double_array pc;
    pc.shape = {2, 2};
    pc.elements = {1.0, 0.0, 0.0, 1.0};
    direction0.set("pc", RecordValue(std::move(pc)));

    RecordValue::string_array axes;
    axes.shape = {2};
    axes.elements = {"Right Ascension", "Declination"};
    direction0.set("axes", RecordValue(std::move(axes)));

    RecordValue::string_array units;
    units.shape = {2};
    units.elements = {"'", "'"};
    direction0.set("units", RecordValue(std::move(units)));

    direction0.set("conversionSystem", RecordValue("J2000"));
    direction0.set("longpole", RecordValue(180.0));
    direction0.set("latpole", RecordValue(0.0));
    coords.set("direction0", RecordValue::from_record(std::move(direction0)));

    // worldmap0, worldreplace0, pixelmap0, pixelreplace0
    RecordValue::int32_array worldmap0;
    worldmap0.shape = {2};
    worldmap0.elements = {0, 1};
    coords.set("worldmap0", RecordValue(std::move(worldmap0)));

    RecordValue::double_array worldreplace0;
    worldreplace0.shape = {2};
    worldreplace0.elements = {0.0, 0.0};
    coords.set("worldreplace0", RecordValue(std::move(worldreplace0)));

    RecordValue::int32_array pixelmap0;
    pixelmap0.shape = {2};
    pixelmap0.elements = {0, 1};
    coords.set("pixelmap0", RecordValue(std::move(pixelmap0)));

    RecordValue::double_array pixelreplace0;
    pixelreplace0.shape = {2};
    pixelreplace0.elements = {0.0, 0.0};
    coords.set("pixelreplace0", RecordValue(std::move(pixelreplace0)));

    record.set("coords", RecordValue::from_record(std::move(coords)));

    return record;
}

} // namespace

int main() noexcept {
    try {
        const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
        const auto base = root / "data" / "corpus" / "fixtures";

        write_bin(base / "logtable_stdstman_keywords" / "column_TIME_keywords.bin",
                  build_logtable_time_keywords());

        write_bin(base / "ms_tree" / "table_keywords.bin", build_ms_table_keywords());

        write_bin(base / "ms_tree" / "column_UVW_keywords.bin", build_ms_uvw_keywords());

        write_bin(base / "pagedimage_coords" / "table_keywords.bin",
                  build_pagedimage_table_keywords());

        std::cout << "All fixtures generated successfully.\n";
    } catch (const std::exception& error) {
        std::cerr << "aipsio_fixture_gen failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
