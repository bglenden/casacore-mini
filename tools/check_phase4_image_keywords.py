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
EXPECTED: dict[str, Any] = {
    "fixture_path": "data/corpus/fixtures/pagedimage_coords/showtableinfo.txt",
    "keywords_sha256": "24932095c16e6d3c9fea6b2aa9f414624848c869120c6b2e88f15ed7eaaefd89",
    "required_substrings": [
        "direction0: {",
        "projection: String \"SIN\"",
        "projection_parameters: Double array with shape [2]",
        "crval: Double array with shape [2]",
        "crpix: Double array with shape [2]",
        "cdelt: Double array with shape [2]",
        "pc: Double array with shape [2, 2]",
        "units: String array with shape [2]",
        "conversionSystem: String \"J2000\"",
        "longpole: Double 180",
        "latpole: Double 0",
        "worldmap0: Int array with shape [2]",
        "worldreplace0: Double array with shape [2]",
        "pixelmap0: Int array with shape [2]",
        "pixelreplace0: Double array with shape [2]",
    ],
}
def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    fixture_path = (repo_root / str(EXPECTED["fixture_path"])).resolve()
    report: dict[str, Any] = {"fixture_path": str(fixture_path)}
    failures: list[str] = []
    if not fixture_path.exists():
        print(
            json.dumps({"status": "FAIL", "error": f"missing fixture {fixture_path}"}, indent=2),
            file=sys.stderr,
        )
        return 1
    text = fixture_path.read_text(encoding="utf-8")
    keywords_text = extract_keywords_section(text)
    canonical_keywords = canonical_text(keywords_text)
    observed_keywords_sha = sha256_hex(canonical_keywords.encode("utf-8"))
    report["observed_keywords_sha256"] = observed_keywords_sha
    expected_keywords_sha = str(EXPECTED["keywords_sha256"])
    if observed_keywords_sha != expected_keywords_sha:
        failures.append(
            "keywords hash mismatch "
            f"(expected {expected_keywords_sha}, observed {observed_keywords_sha})"
        )
    missing = [token for token in EXPECTED["required_substrings"] if token not in text]
    report["missing_substrings"] = missing
    if missing:
        failures.append(f"missing required image-coordinate markers: {missing}")
    report["status"] = "PASS" if not failures else "FAIL"
    print(json.dumps(report, sort_keys=True, indent=2))
    if failures:
        for failure in failures:
            print(f"ERROR: {failure}", file=sys.stderr)
        return 1
    return 0
if __name__ == "__main__":
    sys.exit(main())
