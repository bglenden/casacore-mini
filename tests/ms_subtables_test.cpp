#include "casacore_mini/ms_subtables.hpp"

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

/// Verify column names and types match expected schema for a subtable.
static void
check_schema(const std::string& table_name, const std::vector<casacore_mini::ColumnDesc>& cols,
             const std::vector<std::pair<std::string, casacore_mini::DataType>>& expected) {
    if (cols.size() != expected.size()) {
        std::cerr << "FAIL: " << table_name << " column count " << cols.size() << " != expected "
                  << expected.size() << "\n";
        assert(false);
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (cols[i].name != expected[i].first) {
            std::cerr << "FAIL: " << table_name << " col[" << i << "] name '" << cols[i].name
                      << "' != expected '" << expected[i].first << "'\n";
            assert(false);
        }
        if (cols[i].data_type != expected[i].second) {
            std::cerr << "FAIL: " << table_name << " col[" << i << "] '" << cols[i].name
                      << "' type mismatch\n";
            assert(false);
        }
    }
}

static void test_antenna_schema() {
    std::cout << "  ANTENNA schema... ";
    auto cols = casacore_mini::make_antenna_desc();
    check_schema("ANTENNA", cols,
                 {
                     {"NAME", casacore_mini::DataType::tp_string},
                     {"STATION", casacore_mini::DataType::tp_string},
                     {"TYPE", casacore_mini::DataType::tp_string},
                     {"MOUNT", casacore_mini::DataType::tp_string},
                     {"POSITION", casacore_mini::DataType::tp_double},
                     {"OFFSET", casacore_mini::DataType::tp_double},
                     {"DISH_DIAMETER", casacore_mini::DataType::tp_double},
                     {"FLAG_ROW", casacore_mini::DataType::tp_bool},
                 });
    // POSITION is array with shape [3].
    assert(cols[4].kind == casacore_mini::ColumnKind::array);
    assert(cols[4].ndim == 1);
    assert(cols[4].shape == std::vector<std::int64_t>{3});
    std::cout << "PASS\n";
}

static void test_data_description_schema() {
    std::cout << "  DATA_DESCRIPTION schema... ";
    auto cols = casacore_mini::make_data_description_desc();
    check_schema("DATA_DESCRIPTION", cols,
                 {
                     {"SPECTRAL_WINDOW_ID", casacore_mini::DataType::tp_int},
                     {"POLARIZATION_ID", casacore_mini::DataType::tp_int},
                     {"FLAG_ROW", casacore_mini::DataType::tp_bool},
                 });
    std::cout << "PASS\n";
}

static void test_polarization_schema() {
    std::cout << "  POLARIZATION schema... ";
    auto cols = casacore_mini::make_polarization_desc();
    check_schema("POLARIZATION", cols,
                 {
                     {"NUM_CORR", casacore_mini::DataType::tp_int},
                     {"CORR_TYPE", casacore_mini::DataType::tp_int},
                     {"CORR_PRODUCT", casacore_mini::DataType::tp_int},
                     {"FLAG_ROW", casacore_mini::DataType::tp_bool},
                 });
    assert(cols[1].kind == casacore_mini::ColumnKind::array);
    assert(cols[2].kind == casacore_mini::ColumnKind::array);
    assert(cols[2].ndim == 2);
    std::cout << "PASS\n";
}

static void test_spectral_window_schema() {
    std::cout << "  SPECTRAL_WINDOW schema... ";
    auto cols = casacore_mini::make_spectral_window_desc();
    assert(cols.size() == 14);
    assert(cols[0].name == "NUM_CHAN");
    assert(cols[3].name == "CHAN_FREQ");
    assert(cols[3].kind == casacore_mini::ColumnKind::array);
    assert(cols[8].name == "TOTAL_BANDWIDTH");
    assert(cols[13].name == "FLAG_ROW");
    std::cout << "PASS\n";
}

static void test_field_schema() {
    std::cout << "  FIELD schema... ";
    auto cols = casacore_mini::make_field_desc();
    assert(cols.size() == 9);
    assert(cols[0].name == "NAME");
    assert(cols[4].name == "DELAY_DIR");
    assert(cols[4].kind == casacore_mini::ColumnKind::array);
    assert(cols[5].name == "PHASE_DIR");
    std::cout << "PASS\n";
}

static void test_observation_schema() {
    std::cout << "  OBSERVATION schema... ";
    auto cols = casacore_mini::make_observation_desc();
    assert(cols.size() == 9);
    assert(cols[0].name == "TELESCOPE_NAME");
    assert(cols[1].name == "TIME_RANGE");
    assert(cols[1].kind == casacore_mini::ColumnKind::array);
    assert(cols[1].shape == std::vector<std::int64_t>{2});
    std::cout << "PASS\n";
}

static void test_all_17_factories() {
    std::cout << "  all 17 subtable factories produce non-empty schemas... ";
    std::vector<std::string> all_names = {
        "ANTENNA",     "DATA_DESCRIPTION", "FEED",
        "FIELD",       "FLAG_CMD",         "HISTORY",
        "OBSERVATION", "POINTING",         "POLARIZATION",
        "PROCESSOR",   "SPECTRAL_WINDOW",  "STATE",
        "DOPPLER",     "FREQ_OFFSET",      "SOURCE",
        "SYSCAL",      "WEATHER",
    };
    for (const auto& name : all_names) {
        auto cols = casacore_mini::make_subtable_desc_by_name(name);
        if (cols.empty()) {
            std::cerr << "FAIL: " << name << " returned empty schema\n";
            assert(false);
        }
        // Verify all columns have non-empty names and valid type strings.
        for (const auto& col : cols) {
            (void)col;
            assert(!col.name.empty());
            assert(!col.type_string.empty());
        }
    }
    // Unknown name returns empty.
    assert(casacore_mini::make_subtable_desc_by_name("NONEXISTENT").empty());
    std::cout << "PASS\n";
}

static void test_column_name_lookups() {
    std::cout << "  column name enum lookups... ";
    assert(casacore_mini::ms_antenna_column_name(casacore_mini::MsAntennaColumn::name) == "NAME");
    assert(casacore_mini::ms_antenna_column_name(casacore_mini::MsAntennaColumn::position) ==
           "POSITION");
    assert(casacore_mini::ms_spw_column_name(casacore_mini::MsSpWindowColumn::chan_freq) ==
           "CHAN_FREQ");
    assert(casacore_mini::ms_polarization_column_name(
               casacore_mini::MsPolarizationColumn::num_corr) == "NUM_CORR");
    assert(casacore_mini::ms_state_column_name(casacore_mini::MsStateColumn::obs_mode) ==
           "OBS_MODE");
    assert(casacore_mini::ms_weather_column_name(casacore_mini::MsWeatherColumn::temperature) ==
           "TEMPERATURE");
    assert(casacore_mini::ms_source_column_name(casacore_mini::MsSourceColumn::name) == "NAME");
    assert(casacore_mini::ms_doppler_column_name(casacore_mini::MsDopplerColumn::veldef) ==
           "VELDEF");
    std::cout << "PASS\n";
}

static void test_optional_subtable_schemas() {
    std::cout << "  optional subtable schemas... ";

    auto doppler = casacore_mini::make_doppler_desc();
    assert(doppler.size() == 4);
    assert(doppler[0].name == "DOPPLER_ID");

    auto source = casacore_mini::make_source_desc();
    assert(source.size() == 14);
    assert(source[8].name == "DIRECTION");
    assert(source[8].shape == std::vector<std::int64_t>{2});

    auto weather = casacore_mini::make_weather_desc();
    assert(weather.size() == 9);
    assert(weather[3].name == "TEMPERATURE");
    assert(weather[3].data_type == casacore_mini::DataType::tp_float);

    auto syscal = casacore_mini::make_syscal_desc();
    assert(syscal.size() == 9);
    assert(syscal[5].name == "TSYS");
    assert(syscal[5].kind == casacore_mini::ColumnKind::array);

    auto freq_offset = casacore_mini::make_freq_offset_desc();
    assert(freq_offset.size() == 7);

    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_subtables_test\n";

        test_antenna_schema();
        test_data_description_schema();
        test_polarization_schema();
        test_spectral_window_schema();
        test_field_schema();
        test_observation_schema();
        test_all_17_factories();
        test_column_name_lookups();
        test_optional_subtable_schemas();

        std::cout << "all ms_subtables tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
