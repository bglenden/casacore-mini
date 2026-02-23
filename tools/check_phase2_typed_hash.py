#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import pathlib
import struct
import sys

ROOT_DIR = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT_DIR / "tools"))

import oracle_dump  # noqa: E402


def sha256_hex(data: bytes) -> str:
    digest = hashlib.sha256()
    digest.update(data)
    return digest.hexdigest()


def payload_hash(rows: list[bytes]) -> str:
    payload = bytearray()
    payload.extend(struct.pack("<I", len(rows)))
    for row in rows:
        payload.extend(struct.pack("<I", len(row)))
        payload.extend(row)
    return sha256_hex(bytes(payload))


def encode_array_row(elements: list[bytes]) -> bytes:
    encoded = bytearray()
    encoded.extend(struct.pack("<I", len(elements)))
    for element in elements:
        encoded.extend(struct.pack("<I", len(element)))
        encoded.extend(element)
    return bytes(encoded)


def encode_int32(value: int) -> bytes:
    return struct.pack("<i", value)


def encode_complex64(real: float, imag: float) -> bytes:
    return struct.pack("<ff", real, imag)


def encode_complex128(real: float, imag: float) -> bytes:
    return struct.pack("<dd", real, imag)


def require_hash(name: str, column: dict[str, str], taql_text: str, expected: str) -> None:
    observed, reason = oracle_dump.typed_payload_hash(column, taql_text)
    if reason is not None:
        raise AssertionError(f"{name}: unexpected reason {reason}")
    if observed != expected:
        raise AssertionError(f"{name}: expected {expected}, got {observed}")


def test_complex_scalar() -> None:
    taql_text = """    select result of 2 rows
1 selected columns:  ccol
(1,2)
(3.5,-4)
"""
    expected = payload_hash(
        [
            encode_complex64(1.0, 2.0),
            encode_complex64(3.5, -4.0),
        ]
    )
    require_hash(
        "complex_scalar",
        {"value_kind": "scalar", "data_type": "Complex"},
        taql_text,
        expected,
    )


def test_dcomplex_scalar() -> None:
    taql_text = """    select result of 1 rows
1 selected columns:  zcol
(-1.25,2.5)
"""
    expected = payload_hash([encode_complex128(-1.25, 2.5)])
    require_hash(
        "dcomplex_scalar",
        {"value_kind": "scalar", "data_type": "DComplex"},
        taql_text,
        expected,
    )


def test_complex_array() -> None:
    taql_text = """    select result of 2 rows
1 selected columns:  carr
[(1,2), (3,4)]
[(5.5,-6.5), (7.25,8.75)]
"""
    expected = payload_hash(
        [
            encode_array_row([encode_complex64(1.0, 2.0), encode_complex64(3.0, 4.0)]),
            encode_array_row([encode_complex64(5.5, -6.5), encode_complex64(7.25, 8.75)]),
        ]
    )
    require_hash(
        "complex_array",
        {"value_kind": "array", "data_type": "Complex"},
        taql_text,
        expected,
    )


def test_multidim_array_matrix_format() -> None:
    taql_text = """    select result of 2 rows
1 selected columns:  arrcol
Axis Lengths: [2, 2]  (NB: Matrix in Row/Column order)
[1, 3
 2, 4]
Axis Lengths: [2, 2]  (NB: Matrix in Row/Column order)
[10, 30
 20, 40]
"""
    expected = payload_hash(
        [
            encode_array_row([encode_int32(1), encode_int32(3), encode_int32(2), encode_int32(4)]),
            encode_array_row([encode_int32(10), encode_int32(30), encode_int32(20), encode_int32(40)]),
        ]
    )
    require_hash(
        "multidim_matrix",
        {"value_kind": "array", "data_type": "Int"},
        taql_text,
        expected,
    )


def test_multidim_array_slice_format() -> None:
    taql_text = """Ndim=3 Axis Lengths: [2, 2, 2]
[0, 0, 0][1, 2]
[0, 1, 0][3, 4]
[0, 0, 1][5, 6]
[0, 1, 1][7, 8]
"""
    expected = payload_hash(
        [
            encode_array_row(
                [
                    encode_int32(1),
                    encode_int32(2),
                    encode_int32(3),
                    encode_int32(4),
                    encode_int32(5),
                    encode_int32(6),
                    encode_int32(7),
                    encode_int32(8),
                ]
            )
        ]
    )
    require_hash(
        "multidim_slice",
        {"value_kind": "array", "data_type": "Int"},
        taql_text,
        expected,
    )


def main() -> int:
    test_complex_scalar()
    test_dcomplex_scalar()
    test_complex_array()
    test_multidim_array_matrix_format()
    test_multidim_array_slice_format()
    print("Phase 2 typed hash checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
