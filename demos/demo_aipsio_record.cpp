// demo_aipsio_record.cpp -- Phases 1-2 + 4: AipsIO primitives, Record, keywords
//
// casacore-original equivalent:
//   AipsIO aos("test.dat", ByteIO::New);
//   aos << (Int)42 << (Double)3.14 << String("hello");
//   RecordDesc rd;
//   rd.addField("id",    TpInt);
//   rd.addField("value", TpDouble);
//   rd.addField("label", TpString);
//   Record rec(rd);
//   rec.define("id",    42);
//   rec.define("value", 3.14);
//   rec.define("label", String("hello"));
//   AipsIO aos2("rec.dat", ByteIO::New);
//   aos2 << rec;
//   // Read back
//   AipsIO ais("rec.dat");
//   Record rec2;
//   ais >> rec2;
//   AlwaysAssert(rec2.asInt("id") == 42, AipsError);

#include <casacore_mini/aipsio_reader.hpp>
#include <casacore_mini/aipsio_record_reader.hpp>
#include <casacore_mini/aipsio_record_writer.hpp>
#include <casacore_mini/aipsio_writer.hpp>
#include <casacore_mini/record.hpp>

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

using namespace casacore_mini;

// ---------------------------------------------------------------------------
// Part 1: AipsIO primitives -- write and read basic types
// ---------------------------------------------------------------------------
static void demo_aipsio_primitives() {
    std::cout << "\n=== Part 1: AipsIO Primitives ===\n";

    // Write primitives into a byte buffer.
    AipsIoWriter writer;
    writer.write_i32(42);
    writer.write_f64(3.14159265358979);
    writer.write_string("hello world");
    writer.write_complex64({1.0F, -2.5F});

    auto bytes = writer.take_bytes();
    std::cout << "  Written " << bytes.size() << " bytes of AipsIO primitives\n";

    // Read them back.
    AipsIoReader reader(bytes);
    auto i = reader.read_i32();
    auto d = reader.read_f64();
    auto s = reader.read_string();
    auto c = reader.read_complex64();

    std::cout << "  int32  = " << i << "\n";
    std::cout << "  double = " << d << "\n";
    std::cout << "  string = \"" << s << "\"\n";
    std::cout << "  complex = (" << c.real() << ", " << c.imag() << ")\n";

    assert(i == 42);
    assert(std::fabs(d - 3.14159265358979) < 1e-14);
    assert(s == "hello world");
    assert(c == std::complex<float>(1.0F, -2.5F));

    std::cout << "  [OK] All primitives round-tripped correctly.\n";
}

// ---------------------------------------------------------------------------
// Part 2: Record creation and field access
// ---------------------------------------------------------------------------
static void demo_record_basics() {
    std::cout << "\n=== Part 2: Record Basics ===\n";

    Record rec;
    rec.set("id", RecordValue(std::int32_t{42}));
    rec.set("value", RecordValue(3.14));
    rec.set("label", RecordValue(std::string{"hello"}));
    rec.set("flag", RecordValue(true));

    std::cout << "  Record has " << rec.size() << " fields\n";
    assert(rec.size() == 4);

    // Access fields by name.
    const auto* id_val = rec.find("id");
    assert(id_val != nullptr);
    assert(std::get<std::int32_t>(id_val->storage()) == 42);
    std::cout << "  id    = " << std::get<std::int32_t>(id_val->storage()) << "\n";

    const auto* val = rec.find("value");
    assert(val != nullptr);
    std::cout << "  value = " << std::get<double>(val->storage()) << "\n";

    const auto* lbl = rec.find("label");
    assert(lbl != nullptr);
    std::cout << "  label = \"" << std::get<std::string>(lbl->storage()) << "\"\n";

    const auto* flg = rec.find("flag");
    assert(flg != nullptr);
    std::cout << "  flag  = " << std::boolalpha << std::get<bool>(flg->storage()) << "\n";

    // Iterate over entries.
    std::cout << "  Entry iteration: ";
    for (const auto& [key, value] : rec.entries()) {
        std::cout << key << " ";
    }
    std::cout << "\n";

    std::cout << "  [OK] Record field access verified.\n";
}

// ---------------------------------------------------------------------------
// Part 3: Nested sub-records (Phase 4 keyword concepts)
// ---------------------------------------------------------------------------
static void demo_nested_records() {
    std::cout << "\n=== Part 3: Nested Sub-Records ===\n";

    // Build a nested structure like casacore table keywords:
    //   top_record:
    //     telescope: "VLA"
    //     observer: "test"
    //     obs_info:
    //       mjd: 59000.5
    //       project: "VLASS"

    Record obs_info;
    obs_info.set("mjd", RecordValue(59000.5));
    obs_info.set("project", RecordValue(std::string{"VLASS"}));

    Record top;
    top.set("telescope", RecordValue(std::string{"VLA"}));
    top.set("observer", RecordValue(std::string{"test"}));
    top.set("obs_info", RecordValue::from_record(obs_info));

    std::cout << "  Top record has " << top.size() << " fields\n";

    // Navigate into sub-record.
    const auto* sub = top.find("obs_info");
    assert(sub != nullptr);
    const auto* sub_rec = std::get_if<RecordValue::record_ptr>(&sub->storage());
    assert(sub_rec != nullptr && *sub_rec != nullptr);

    const auto* mjd = (*sub_rec)->find("mjd");
    assert(mjd != nullptr);
    std::cout << "  obs_info.mjd     = " << std::get<double>(mjd->storage()) << "\n";
    assert(std::fabs(std::get<double>(mjd->storage()) - 59000.5) < 1e-10);

    const auto* proj = (*sub_rec)->find("project");
    assert(proj != nullptr);
    std::cout << "  obs_info.project = \"" << std::get<std::string>(proj->storage()) << "\"\n";
    assert(std::get<std::string>(proj->storage()) == "VLASS");

    std::cout << "  [OK] Nested record traversal verified.\n";
}

// ---------------------------------------------------------------------------
// Part 4: Record AipsIO round-trip (Phase 2 persistence)
// ---------------------------------------------------------------------------
static void demo_record_roundtrip() {
    std::cout << "\n=== Part 4: Record AipsIO Round-Trip ===\n";

    // Build a record with diverse types including arrays and nested records.
    Record original;
    original.set("int_field", RecordValue(std::int32_t{-7}));
    original.set("float_field", RecordValue(float{2.5F}));
    original.set("double_field", RecordValue(99.99));
    original.set("string_field", RecordValue(std::string{"casacore-mini"}));
    original.set("complex_field",
                 RecordValue(std::complex<double>{1.0, -3.0}));

    // Add an array field.
    RecordValue::double_array arr;
    arr.shape = {3};
    arr.elements = {1.1, 2.2, 3.3};
    original.set("array_field", RecordValue(arr));

    // Add a nested sub-record.
    Record nested;
    nested.set("sub_int", RecordValue(std::int32_t{100}));
    nested.set("sub_str", RecordValue(std::string{"nested value"}));
    original.set("sub_record", RecordValue::from_record(nested));

    std::cout << "  Original record has " << original.size() << " fields\n";

    // Serialize via AipsIO.
    AipsIoWriter writer;
    write_aipsio_record(writer, original);
    auto bytes = writer.take_bytes();
    std::cout << "  Serialized to " << bytes.size() << " bytes\n";

    // Deserialize.
    AipsIoReader reader(bytes);
    Record restored = read_aipsio_record(reader);

    std::cout << "  Restored record has " << restored.size() << " fields\n";
    assert(restored.size() == original.size());

    // Verify scalar fields.
    auto check_int = [&](const char* name, std::int32_t expected) {
        const auto* v = restored.find(name);
        assert(v != nullptr);
        assert(std::get<std::int32_t>(v->storage()) == expected);
    };
    check_int("int_field", -7);

    const auto* ff = restored.find("float_field");
    assert(ff != nullptr);
    assert(std::fabs(std::get<float>(ff->storage()) - 2.5F) < 1e-6F);

    const auto* df = restored.find("double_field");
    assert(df != nullptr);
    assert(std::fabs(std::get<double>(df->storage()) - 99.99) < 1e-10);

    const auto* sf = restored.find("string_field");
    assert(sf != nullptr);
    assert(std::get<std::string>(sf->storage()) == "casacore-mini");

    const auto* cf = restored.find("complex_field");
    assert(cf != nullptr);
    auto cval = std::get<std::complex<double>>(cf->storage());
    assert(std::fabs(cval.real() - 1.0) < 1e-10);
    assert(std::fabs(cval.imag() - (-3.0)) < 1e-10);

    // Verify array field.
    const auto* af = restored.find("array_field");
    assert(af != nullptr);
    const auto& restored_arr =
        std::get<RecordValue::double_array>(af->storage());
    assert(restored_arr.shape.size() == 1);
    assert(restored_arr.shape[0] == 3);
    assert(restored_arr.elements.size() == 3);
    assert(std::fabs(restored_arr.elements[0] - 1.1) < 1e-10);
    assert(std::fabs(restored_arr.elements[1] - 2.2) < 1e-10);
    assert(std::fabs(restored_arr.elements[2] - 3.3) < 1e-10);

    // Verify nested sub-record.
    const auto* sr = restored.find("sub_record");
    assert(sr != nullptr);
    const auto* sr_ptr =
        std::get_if<RecordValue::record_ptr>(&sr->storage());
    assert(sr_ptr != nullptr && *sr_ptr != nullptr);
    const auto* si = (*sr_ptr)->find("sub_int");
    assert(si != nullptr);
    assert(std::get<std::int32_t>(si->storage()) == 100);
    const auto* ss = (*sr_ptr)->find("sub_str");
    assert(ss != nullptr);
    assert(std::get<std::string>(ss->storage()) == "nested value");

    std::cout << "  [OK] Full record round-trip verified (7 fields incl. array + nested).\n";
}

// ---------------------------------------------------------------------------
// Part 5: Record equality
// ---------------------------------------------------------------------------
static void demo_record_equality() {
    std::cout << "\n=== Part 5: Record Equality ===\n";

    Record a;
    a.set("x", RecordValue(std::int32_t{1}));
    a.set("y", RecordValue(2.0));

    Record b;
    b.set("x", RecordValue(std::int32_t{1}));
    b.set("y", RecordValue(2.0));

    Record c;
    c.set("x", RecordValue(std::int32_t{1}));
    c.set("y", RecordValue(3.0)); // different

    assert(a == b);
    assert(!(a == c));

    std::cout << "  a == b: true  (identical)\n";
    std::cout << "  a == c: false (different y value)\n";
    std::cout << "  [OK] Record equality verified.\n";
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: AipsIO + Record (Phases 1-2 + 4) ===\n";

        demo_aipsio_primitives();
        demo_record_basics();
        demo_nested_records();
        demo_record_roundtrip();
        demo_record_equality();

        std::cout << "\n=== All AipsIO + Record demos passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
