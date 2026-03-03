#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later

from __future__ import annotations
import argparse
import json
import pathlib
import sys
from typing import Any
def load_json(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"Oracle payload is not a JSON object: {path}")
    return data
def escape_token(token: str) -> str:
    return token.replace("~", "~0").replace("/", "~1")
def first_diff(left: Any, right: Any, path: str = "") -> tuple[str, Any, Any] | None:
    if type(left) is not type(right):
        return (path or "/", left, right)
    if isinstance(left, dict):
        left_keys = set(left.keys())
        right_keys = set(right.keys())
        if left_keys != right_keys:
            missing_left = sorted(right_keys - left_keys)
            if missing_left:
                key = missing_left[0]
                pointer = f"{path}/{escape_token(key)}" if path else f"/{escape_token(key)}"
                return (pointer, None, right[key])
            missing_right = sorted(left_keys - right_keys)
            key = missing_right[0]
            pointer = f"{path}/{escape_token(key)}" if path else f"/{escape_token(key)}"
            return (pointer, left[key], None)
        for key in sorted(left.keys()):
            pointer = f"{path}/{escape_token(key)}" if path else f"/{escape_token(key)}"
            diff = first_diff(left[key], right[key], pointer)
            if diff is not None:
                return diff
        return None
    if isinstance(left, list):
        if len(left) != len(right):
            return (path or "/", len(left), len(right))
        for idx, (left_item, right_item) in enumerate(zip(left, right)):
            pointer = f"{path}/{idx}" if path else f"/{idx}"
            diff = first_diff(left_item, right_item, pointer)
            if diff is not None:
                return diff
        return None
    if left != right:
        return (path or "/", left, right)
    return None
def normalized(payload: dict[str, Any]) -> dict[str, Any]:
    clean = json.loads(json.dumps(payload))
    diagnostics = clean.get("diagnostics")
    if isinstance(diagnostics, dict):
        diagnostics.pop("generated_at_utc", None)
    return clean
def column_storage_map(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    mapping: dict[str, dict[str, Any]] = {}
    schema = payload.get("schema", {})
    columns = schema.get("columns", []) if isinstance(schema, dict) else []
    if not isinstance(columns, list):
        return mapping
    for col in columns:
        if isinstance(col, dict) and isinstance(col.get("name"), str):
            mapping[col["name"]] = col.get("storage_manager") if isinstance(col.get("storage_manager"), dict) else {}
    return mapping
def status_for(left: dict[str, Any], right: dict[str, Any]) -> tuple[str, str, Any, Any]:
    left_diag = left.get("diagnostics", {})
    right_diag = right.get("diagnostics", {})
    for diag in (left_diag, right_diag):
        if isinstance(diag, dict):
            notes = diag.get("notes", [])
            if isinstance(notes, list) and notes:
                return ("UNSUPPORTED", "/diagnostics/notes", notes, notes)
    left_norm = normalized(left)
    right_norm = normalized(right)
    left_schema = left_norm.get("schema", {})
    right_schema = right_norm.get("schema", {})
    diff = first_diff(left_schema, right_schema, "/schema")
    if diff is not None:
        left_storage = column_storage_map(left_norm)
        right_storage = column_storage_map(right_norm)
        if left_storage != right_storage:
            return ("FAIL_STORAGE_BINDING", diff[0], diff[1], diff[2])
        return ("FAIL_SCHEMA", diff[0], diff[1], diff[2])
    diff = first_diff(left_norm.get("keywords", {}), right_norm.get("keywords", {}), "/keywords")
    if diff is not None:
        return ("FAIL_KEYWORDS", diff[0], diff[1], diff[2])
    diff = first_diff(left_norm.get("data", {}), right_norm.get("data", {}), "/data")
    if diff is not None:
        return ("FAIL_DATA", diff[0], diff[1], diff[2])
    return ("PASS", "", None, None)
def main() -> int:
    parser = argparse.ArgumentParser(description="Compare two Phase 0 oracle JSON files")
    parser.add_argument("--left", required=True, help="Left oracle JSON path")
    parser.add_argument("--right", required=True, help="Right oracle JSON path")
    parser.add_argument("--output", help="Optional JSON output path")
    parser.add_argument("--allow-unsupported", action="store_true", help="Treat UNSUPPORTED as success")
    args = parser.parse_args()
    left_path = pathlib.Path(args.left).resolve()
    right_path = pathlib.Path(args.right).resolve()
    left = load_json(left_path)
    right = load_json(right_path)
    artifact_id = left.get("artifact", {}).get("artifact_id")
    if artifact_id != right.get("artifact", {}).get("artifact_id"):
        status = "ERROR"
        pointer = "/artifact/artifact_id"
        left_value = artifact_id
        right_value = right.get("artifact", {}).get("artifact_id")
    else:
        status, pointer, left_value, right_value = status_for(left, right)
    result = {
        "artifact_id": artifact_id,
        "status": status,
        "left": str(left_path),
        "right": str(right_path),
        "mismatch": {
            "json_pointer": pointer,
            "left": left_value,
            "right": right_value,
        },
        "comparator_version": "0.1.0",
        "baseline": left.get("baseline", {}),
    }
    text = json.dumps(result, sort_keys=True, indent=2) + "\n"
    if args.output:
        pathlib.Path(args.output).write_text(text, encoding="utf-8")
    print(text, end="")
    if status == "PASS":
        return 0
    if status == "UNSUPPORTED" and args.allow_unsupported:
        return 0
    if status in {"FAIL_SCHEMA", "FAIL_KEYWORDS", "FAIL_DATA", "FAIL_STORAGE_BINDING", "UNSUPPORTED", "ERROR"}:
        return 1
    return 1
if __name__ == "__main__":
    sys.exit(main())
