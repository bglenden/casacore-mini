#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse
import json
import pathlib
import subprocess
import sys
from typing import Any
EXPECTED: dict[str, dict[str, Any]] = {
    "table_dat_ttable2_v0": {
        "table_version": 1,
        "row_count": 10,
        "big_endian": True,
        "table_type": "PlainTable",
    },
    "table_dat_ttable2_v1": {
        "table_version": 2,
        "row_count": 10,
        "big_endian": True,
        "table_type": "PlainTable",
    },
}
def run_or_throw(args: list[str]) -> str:
    proc = subprocess.run(args, check=False, capture_output=True, text=True)
    if proc.returncode != 0:
        cmd = " ".join(args)
        raise RuntimeError(f"command failed ({proc.returncode}): {cmd}\n{proc.stderr.strip()}")
    return proc.stdout
def load_json(path: pathlib.Path) -> dict[str, Any]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise RuntimeError(f"oracle output must be JSON object: {path}")
    return payload
def verify_artifact(artifact_id: str, oracle_path: pathlib.Path) -> None:
    payload = load_json(oracle_path)
    schema = payload.get("schema")
    data = payload.get("data")
    if not isinstance(schema, dict):
        raise RuntimeError(f"{artifact_id}: schema missing")
    if not isinstance(data, dict):
        raise RuntimeError(f"{artifact_id}: data missing")
    if schema.get("table_kind") != "table_dat_fixture":
        raise RuntimeError(f"{artifact_id}: unexpected schema.table_kind={schema.get('table_kind')!r}")
    if schema.get("column_count") != 0:
        raise RuntimeError(f"{artifact_id}: expected schema.column_count=0")
    if schema.get("columns") != []:
        raise RuntimeError(f"{artifact_id}: expected empty schema.columns")
    metadata = schema.get("table_dat_metadata")
    if not isinstance(metadata, dict):
        raise RuntimeError(f"{artifact_id}: schema.table_dat_metadata missing")
    expected = EXPECTED[artifact_id]
    for key, expected_value in expected.items():
        if metadata.get(key) != expected_value:
            raise RuntimeError(
                f"{artifact_id}: metadata mismatch for {key}: "
                f"expected={expected_value!r} observed={metadata.get(key)!r}"
            )
    if schema.get("row_count") != expected["row_count"]:
        raise RuntimeError(f"{artifact_id}: schema.row_count mismatch")
    if data.get("row_count") != expected["row_count"]:
        raise RuntimeError(f"{artifact_id}: data.row_count mismatch")
    if not isinstance(data.get("table_dat_payload_sha256"), str):
        raise RuntimeError(f"{artifact_id}: missing data.table_dat_payload_sha256")
    if not isinstance(data.get("table_dat_metadata_sha256"), str):
        raise RuntimeError(f"{artifact_id}: missing data.table_dat_metadata_sha256")
def main() -> int:
    parser = argparse.ArgumentParser(description="Phase 2 non-replay corpus checks")
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    manifest = pathlib.Path(args.manifest).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve()
    run1_dir = output_dir / "run1"
    run2_dir = output_dir / "run2"
    run1_dir.mkdir(parents=True, exist_ok=True)
    run2_dir.mkdir(parents=True, exist_ok=True)
    for artifact_id in EXPECTED:
        run_or_throw(
            [
                "python3",
                "tools/oracle_dump.py",
                "--manifest",
                str(manifest),
                "--artifact-id",
                artifact_id,
                "--output-dir",
                str(run1_dir),
                "--strict-checksum",
            ]
        )
        run_or_throw(
            [
                "python3",
                "tools/oracle_dump.py",
                "--manifest",
                str(manifest),
                "--artifact-id",
                artifact_id,
                "--output-dir",
                str(run2_dir),
                "--strict-checksum",
            ]
        )
        run_or_throw(
            [
                "python3",
                "tools/oracle_compare.py",
                "--left",
                str(run1_dir / f"{artifact_id}.oracle.json"),
                "--right",
                str(run2_dir / f"{artifact_id}.oracle.json"),
            ]
        )
        verify_artifact(artifact_id, run1_dir / f"{artifact_id}.oracle.json")
    print(json.dumps({"status": "PASS", "checked_artifacts": sorted(EXPECTED.keys())}, sort_keys=True, indent=2))
    return 0
if __name__ == "__main__":
    sys.exit(main())
