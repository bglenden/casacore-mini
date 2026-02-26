#!/usr/bin/env bash
# run_bench.sh -- Build and run casacore-original vs casacore-mini benchmarks.
#
# Usage: ./tools/perf/run_bench.sh [build_dir]
#   build_dir defaults to build-bench
#
# Prerequisites:
#   - casacore-original installed and discoverable via pkg-config
#   - cmake, clang++ available
#
# Output: comparison table with ratios, warnings for >1.5x

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_DIR/build-bench}"

WARN_THRESHOLD="1.5"

BENCH_TABLES="${TMPDIR:-/tmp}/casacore_bench_tables_$$"
BENCH_WRITE_CASACORE="${TMPDIR:-/tmp}/casacore_bench_write_orig_$$"
BENCH_WRITE_MINI="${TMPDIR:-/tmp}/casacore_bench_write_mini_$$"

cleanup() {
    rm -rf "$BENCH_TABLES" "$BENCH_WRITE_CASACORE" "$BENCH_WRITE_MINI"
}
trap cleanup EXIT

echo "=== Performance Benchmark: casacore-original vs casacore-mini ==="
echo ""
echo "Project dir:  $PROJECT_DIR"
echo "Build dir:    $BUILD_DIR"
echo "Tables dir:   $BENCH_TABLES"
echo ""

# ---- Step 1: Build casacore-original programs ----
echo "--- Building casacore-original benchmark programs ---"

CASACORE_CFLAGS=$(pkg-config --cflags casacore 2>/dev/null || echo "")
CASACORE_LIBS=$(pkg-config --libs casacore 2>/dev/null || echo "-lcasa_tables -lcasa_casa")

ORIG_BIN_DIR="$BUILD_DIR/orig_bench"
mkdir -p "$ORIG_BIN_DIR"

for src in create_bench_tables bench_casacore_read bench_casacore_write; do
    echo "  Compiling $src ..."
    # shellcheck disable=SC2086
    eval clang++ -std=c++20 -O2 $CASACORE_CFLAGS \
        "\"$SCRIPT_DIR/$src.cpp\"" \
        $CASACORE_LIBS \
        -o "\"$ORIG_BIN_DIR/$src\""
done
echo "  casacore-original programs built."
echo ""

# ---- Step 2: Build casacore-mini programs ----
echo "--- Building casacore-mini benchmark programs ---"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCASACORE_MINI_WARNINGS_AS_ERRORS=OFF \
    -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
    2>&1 | tail -5

cmake --build "$BUILD_DIR" --target bench_mini_read bench_mini_write -j "$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
echo "  casacore-mini programs built."
echo ""

# ---- Step 3: Create read-benchmark tables ----
echo "--- Creating read-benchmark tables (500k rows x 11 columns) ---"
"$ORIG_BIN_DIR/create_bench_tables" "$BENCH_TABLES"
echo ""

# ---- Step 4: Run read benchmarks ----
echo "--- Running read benchmarks ---"

# Associative arrays to hold timings
declare -A CASACORE_TIMINGS
declare -A MINI_TIMINGS

for table_name in bench_ssm bench_ism bench_tsm; do
    table_path="$BENCH_TABLES/$table_name"
    echo "  Reading $table_name with casacore-original ..."
    while IFS=' ' read -r tag op_name timing; do
        if [ "$tag" = "TIMING" ]; then
            CASACORE_TIMINGS["${table_name}_${op_name}"]="$timing"
        fi
    done < <("$ORIG_BIN_DIR/bench_casacore_read" "$table_path" 2>/dev/null)

    echo "  Reading $table_name with casacore-mini ..."
    while IFS=' ' read -r tag op_name timing; do
        if [ "$tag" = "TIMING" ]; then
            MINI_TIMINGS["${table_name}_${op_name}"]="$timing"
        fi
    done < <("$BUILD_DIR/bench_mini_read" "$table_path" 2>/dev/null)
done
echo ""

# ---- Step 5: Run write benchmarks ----
echo "--- Running write benchmarks ---"

echo "  Writing with casacore-original ..."
while IFS=' ' read -r tag op_name timing; do
    if [ "$tag" = "TIMING" ]; then
        CASACORE_TIMINGS["write_${op_name}"]="$timing"
    fi
done < <("$ORIG_BIN_DIR/bench_casacore_write" "$BENCH_WRITE_CASACORE" 2>/dev/null)

echo "  Writing with casacore-mini ..."
while IFS=' ' read -r tag op_name timing; do
    if [ "$tag" = "TIMING" ]; then
        MINI_TIMINGS["write_${op_name}"]="$timing"
    fi
done < <("$BUILD_DIR/bench_mini_write" "$BENCH_WRITE_MINI" 2>/dev/null)
echo ""

# ---- Step 6: Print comparison table ----
WARNINGS=0

print_row() {
    local label="$1"
    local key="$2"
    local casacore_t="${CASACORE_TIMINGS[$key]:-N/A}"
    local mini_t="${MINI_TIMINGS[$key]:-N/A}"

    if [ "$casacore_t" = "N/A" ] || [ "$mini_t" = "N/A" ]; then
        printf "  %-24s casacore=%8s   mini=%8s   ratio=N/A\n" \
            "${label}:" "$casacore_t" "$mini_t"
        return
    fi

    local ratio
    ratio=$(awk "BEGIN { if ($casacore_t > 0) printf \"%.2f\", $mini_t / $casacore_t; else print \"inf\" }")

    local warn=""
    if awk "BEGIN { exit !($ratio > $WARN_THRESHOLD) }"; then
        warn=" [WARN]"
        WARNINGS=$((WARNINGS + 1))
    fi

    printf "  %-24s casacore=%7.2f s   mini=%7.2f s   ratio=%sx%s\n" \
        "${label}:" "$casacore_t" "$mini_t" "$ratio" "$warn"
}

OPS="scalar_seq_read array_seq_read keyword_access row_oriented_read"

for table_name in bench_ssm bench_ism bench_tsm; do
    echo "=== READ: $table_name ==="
    for op in $OPS; do
        print_row "$op" "${table_name}_${op}"
    done
    echo ""
done

WRITE_OPS="full_table_write scalar_only_write array_only_write"

echo "=== WRITE: SSM ==="
for op in $WRITE_OPS; do
    print_row "$op" "write_${op}"
done
echo ""

if [ "$WARNINGS" -gt 0 ]; then
    echo "[WARN] $WARNINGS measurement(s) exceeded ${WARN_THRESHOLD}x threshold -- investigate."
else
    echo "All measurements within ${WARN_THRESHOLD}x threshold."
fi
echo ""
echo "Done."
