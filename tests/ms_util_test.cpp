#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_util.hpp"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_util_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void test_stokes_circular_to_iquv() {
    std::cout << "  StokesConverter: circular -> IQUV... ";

    // RR=5, RL=6, LR=7, LL=8
    StokesConverter conv({5, 6, 7, 8}, {1, 2, 3, 4});

    // RR=1+0i, RL=0+0i, LR=0+0i, LL=1+0i (pure Stokes I).
    std::vector<std::complex<float>> in = {{1.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}, {1.0F, 0.0F}};
    auto out = conv.convert(in);
    assert(out.size() == 4);
    // I = (RR + LL) / 2 = 1.0
    assert(std::abs(out[0].real() - 1.0F) < 1e-6F);
    assert(std::abs(out[0].imag()) < 1e-6F);
    // Q = (RL + LR) / 2 = 0
    assert(std::abs(out[1].real()) < 1e-6F);
    // U = i*(LR - RL) / 2 = 0
    assert(std::abs(out[2].real()) < 1e-6F);
    // V = (RR - LL) / 2 = 0
    assert(std::abs(out[3].real()) < 1e-6F);

    std::cout << "PASS\n";
}

static void test_stokes_linear_to_iquv() {
    std::cout << "  StokesConverter: linear -> IQUV... ";

    // XX=9, XY=10, YX=11, YY=12
    StokesConverter conv({9, 10, 11, 12}, {1, 2, 3, 4});

    // XX=3, XY=1, YX=1, YY=1 -> I=(3+1)/2=2, Q=(3-1)/2=1, U=(1+1)/2=1
    std::vector<std::complex<float>> in = {{3.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 0.0F}};
    auto out = conv.convert(in);
    assert(out.size() == 4);
    assert(std::abs(out[0].real() - 2.0F) < 1e-6F); // I
    assert(std::abs(out[1].real() - 1.0F) < 1e-6F); // Q
    assert(std::abs(out[2].real() - 1.0F) < 1e-6F); // U

    std::cout << "PASS\n";
}

static void test_stokes_passthrough() {
    std::cout << "  StokesConverter: passthrough... ";

    // Same in and out types -> identity.
    StokesConverter conv({5, 8}, {5, 8});
    std::vector<std::complex<float>> in = {{2.0F, 1.0F}, {3.0F, -1.0F}};
    auto out = conv.convert(in);
    assert(out.size() == 2);
    assert(out[0] == in[0]);
    assert(out[1] == in[1]);

    std::cout << "PASS\n";
}

static void test_doppler_util() {
    std::cout << "  MsDopplerUtil conversions... ";

    const double rest_freq = 1.420405751e9; // HI 21cm in Hz

    // Zero velocity -> rest frequency.
    double f = MsDopplerUtil::velocity_to_frequency(0.0, rest_freq);
    (void)f;
    assert(std::abs(f - rest_freq) < 1.0);

    // Round-trip: v -> f -> v.
    double v_in = 1000.0; // 1 km/s
    double freq = MsDopplerUtil::velocity_to_frequency(v_in, rest_freq);
    double v_out = MsDopplerUtil::frequency_to_velocity(freq, rest_freq);
    (void)v_out;
    assert(std::abs(v_out - v_in) < 0.01);

    std::cout << "PASS\n";
}

static void test_history_handler() {
    std::cout << "  MsHistoryHandler... ";

    auto ms_path = make_temp_dir("hist");
    cleanup(ms_path);

    {
        auto ms = MeasurementSet::create(ms_path, false);
        MsHistoryHandler hist(ms);
        hist.add_entry("Test message 1", "INFO", "test_app", 4.8e9);
        hist.add_entry("Test message 2", "WARN", "test_app", 4.8e9 + 10.0);
        hist.flush();
    }

    // Reopen and verify HISTORY subtable has 2 rows.
    {
        auto ms = MeasurementSet::open(ms_path);
        const auto& st = ms.subtable("HISTORY");
        assert(st.nrow() == 2);

        // Read back the scalar columns via Table API.
        auto msg0 = std::get<std::string>(st.read_scalar_cell("MESSAGE", 0));
        assert(msg0 == "Test message 1");
        auto msg1 = std::get<std::string>(st.read_scalar_cell("MESSAGE", 1));
        assert(msg1 == "Test message 2");
        auto prio1 = std::get<std::string>(st.read_scalar_cell("PRIORITY", 1));
        assert(prio1 == "WARN");
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_util_test\n";

        test_stokes_circular_to_iquv();
        test_stokes_linear_to_iquv();
        test_stokes_passthrough();
        test_doppler_util();
        test_history_handler();

        std::cout << "all ms_util tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
