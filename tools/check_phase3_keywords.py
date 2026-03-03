#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import hashlib
import json
import pathlib
import sys
from typing import Any
def canonical_text(text: str) -> str:
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) + "\n"
def sha256_hex(data: bytes) -> str:
    digest = hashlib.sha256()
    digest.update(data)
    return digest.hexdigest()
def extract_keywords_section(showtableinfo_text: str) -> str:
    marker = "Keywords of main table"
    marker_index = showtableinfo_text.find(marker)
    if marker_index < 0:
        raise RuntimeError("missing 'Keywords of main table' section")
    return showtableinfo_text[marker_index + len(marker) :]
EXPECTED: dict[str, dict[str, Any]] = {
    "logtable_stdstman_keywords": {
        "fixture_path": "data/corpus/fixtures/logtable_stdstman_keywords/showtableinfo.txt",
        "full_sha256": "8d300584d59358df6f6f99920843cbc09d44f68d0ba8176cd87ff2c7145b97ed",
        "keywords_sha256": "feb15fe247b7dd1d9e7eb4cd213082ed68b2a228fba67bbf5a8bff7f1c330d32",
        "required_substrings": [
            "Column TIME",
            "UNIT: String \"s\"",
            "MEASURE_TYPE: String \"EPOCH\"",
            "MEASURE_REFERENCE: String \"UTC\"",
        ],
    },
    "ms_tree": {
        "fixture_path": "data/corpus/fixtures/ms_tree/showtableinfo.txt",
        "full_sha256": "ce9618ddaa3a9c949a9348192130e2c209a30895f1f273b60e184c729e514069",
        "keywords_sha256": "b702b3055fe382412d6e4657bd4f099ac40b1c04b267eba9c0df8cd7953baaf6",
        "required_substrings": [
            "Column UVW",
            "QuantumUnits: String array with shape [3]",
            "type: String \"uvw\"",
            "Ref: String \"ITRF\"",
            "Column TIME",
            "QuantumUnits: String array with shape [1]",
            "type: String \"epoch\"",
            "Ref: String \"UTC\"",
        ],
    },
    "pagedimage_coords": {
        "fixture_path": "data/corpus/fixtures/pagedimage_coords/showtableinfo.txt",
        "full_sha256": "bd9aadd849bd169d4351f95961c6a5216d9dbaa20a1ee507684b4086698f29b0",
        "keywords_sha256": "24932095c16e6d3c9fea6b2aa9f414624848c869120c6b2e88f15ed7eaaefd89",
        "required_substrings": [
            "coords: {",
            "obsdate: {",
            "type: String \"epoch\"",
            "direction0: {",
            "system: String \"J2000\"",
            "axes: String array with shape [2]",
            "[Right Ascension, Declination]",
            "pixelmap0: Int array with shape [2]",
        ],
    },
}
def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    failures: list[str] = []
    report: dict[str, Any] = {"artifacts": {}}
    for artifact_id, spec in EXPECTED.items():
        fixture_path = (repo_root / str(spec["fixture_path"])).resolve()
        artifact_report: dict[str, Any] = {"fixture_path": str(fixture_path)}
        if not fixture_path.exists():
            failures.append(f"{artifact_id}: missing fixture file {fixture_path}")
            report["artifacts"][artifact_id] = artifact_report
            continue
        text = fixture_path.read_text(encoding="utf-8")
        full_canonical = canonical_text(text)
        keywords_canonical = canonical_text(extract_keywords_section(text))
        observed_full_sha = sha256_hex(full_canonical.encode("utf-8"))
        observed_keywords_sha = sha256_hex(keywords_canonical.encode("utf-8"))
        artifact_report["observed_full_sha256"] = observed_full_sha
        artifact_report["observed_keywords_sha256"] = observed_keywords_sha
        expected_full_sha = str(spec["full_sha256"])
        expected_keywords_sha = str(spec["keywords_sha256"])
        if observed_full_sha != expected_full_sha:
            failures.append(
                f"{artifact_id}: full showtableinfo hash mismatch "
                f"(expected {expected_full_sha}, observed {observed_full_sha})"
            )
        if observed_keywords_sha != expected_keywords_sha:
            failures.append(
                f"{artifact_id}: keywords section hash mismatch "
                f"(expected {expected_keywords_sha}, observed {observed_keywords_sha})"
            )
        missing = [token for token in spec["required_substrings"] if token not in text]
        artifact_report["missing_substrings"] = missing
        if missing:
            failures.append(f"{artifact_id}: missing required metadata markers: {missing}")
        report["artifacts"][artifact_id] = artifact_report
    report["status"] = "PASS" if not failures else "FAIL"
    print(json.dumps(report, sort_keys=True, indent=2))
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    return 0
if __name__ == "__main__":
    sys.exit(main())
