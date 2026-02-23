#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys
from typing import Any


def run_cli(table_schema_cli: pathlib.Path, showtableinfo_file: pathlib.Path) -> dict[str, Any]:
    proc = subprocess.run(
        [str(table_schema_cli), str(showtableinfo_file)],
        check=False,
        capture_output=True,
        text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"table_schema_cli failed ({proc.returncode}):\n{proc.stderr.strip()}")

    parsed: dict[str, Any] = {
        "storage_managers": [],
        "columns": [],
    }

    for raw_line in proc.stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        if line.startswith("table_path="):
            parsed["table_path"] = line.split("=", 1)[1]
            continue
        if line.startswith("table_kind="):
            parsed["table_kind"] = line.split("=", 1)[1]
            continue
        if line.startswith("row_count="):
            parsed["row_count"] = int(line.split("=", 1)[1])
            continue
        if line.startswith("column_count="):
            parsed["column_count"] = int(line.split("=", 1)[1])
            continue

        if line.startswith("storage_manager="):
            payload = line.split("=", 1)[1]
            parts = payload.split("|")
            if len(parts) != 3:
                raise RuntimeError(f"Malformed storage_manager line: {line}")
            parsed["storage_managers"].append(
                {
                    "class": parts[0],
                    "file": parts[1],
                    "name": parts[2],
                }
            )
            continue

        if line.startswith("column="):
            payload = line.split("=", 1)[1]
            parts = payload.split("|")
            if len(parts) != 8:
                raise RuntimeError(f"Malformed column line: {line}")
            shape: list[int] | None
            if parts[4] == "-":
                shape = None
            else:
                shape = [int(item) for item in parts[4].split(",") if item]

            ndim: int | None
            if parts[3] == "-":
                ndim = None
            else:
                ndim = int(parts[3])

            storage_manager = None
            if not (parts[5] == "-" and parts[6] == "-" and parts[7] == "-"):
                storage_manager = {
                    "class": parts[5],
                    "file": parts[6],
                    "name": parts[7],
                }

            parsed["columns"].append(
                {
                    "name": parts[0],
                    "data_type": parts[1],
                    "value_kind": parts[2],
                    "ndim": ndim,
                    "shape": shape,
                    "storage_manager": storage_manager,
                }
            )
            continue

        raise RuntimeError(f"Unknown output line from table_schema_cli: {line}")

    return parsed


def load_oracle_schema(oracle_json: pathlib.Path) -> dict[str, Any]:
    payload = json.loads(oracle_json.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise RuntimeError("Oracle JSON payload must be an object")

    schema = payload.get("schema")
    if not isinstance(schema, dict):
        raise RuntimeError("Oracle JSON payload is missing schema object")

    return schema


def mismatch(pointer: str, left: Any, right: Any) -> dict[str, Any]:
    return {
        "json_pointer": pointer,
        "left": left,
        "right": right,
    }


def compare(cli_schema: dict[str, Any], oracle_schema: dict[str, Any]) -> dict[str, Any] | None:
    scalar_fields = ["table_path", "table_kind", "row_count", "column_count"]
    for field in scalar_fields:
        left = cli_schema.get(field)
        right = oracle_schema.get(field)
        if left != right:
            return mismatch(f"/schema/{field}", left, right)

    cli_managers = cli_schema.get("storage_managers", [])
    oracle_managers = oracle_schema.get("storage_managers", [])
    if cli_managers != oracle_managers:
        return mismatch("/schema/storage_managers", cli_managers, oracle_managers)

    cli_columns = cli_schema.get("columns", [])
    oracle_columns = oracle_schema.get("columns", [])
    if len(cli_columns) != len(oracle_columns):
        return mismatch("/schema/columns", len(cli_columns), len(oracle_columns))

    for index, (left, right) in enumerate(zip(cli_columns, oracle_columns)):
        normalized = {
            "name": right.get("name"),
            "data_type": right.get("data_type"),
            "value_kind": right.get("value_kind"),
            "ndim": right.get("ndim"),
            "shape": right.get("shape"),
            "storage_manager": right.get("storage_manager"),
        }
        if left != normalized:
            return mismatch(f"/schema/columns/{index}", left, normalized)

    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare table_schema_cli output against oracle schema JSON")
    parser.add_argument("--table-schema-cli", required=True)
    parser.add_argument("--showtableinfo-file", required=True)
    parser.add_argument("--oracle-json", required=True)
    args = parser.parse_args()

    table_schema_cli = pathlib.Path(args.table_schema_cli).resolve()
    showtableinfo_file = pathlib.Path(args.showtableinfo_file).resolve()
    oracle_json = pathlib.Path(args.oracle_json).resolve()

    cli_schema = run_cli(table_schema_cli, showtableinfo_file)
    oracle_schema = load_oracle_schema(oracle_json)
    diff = compare(cli_schema, oracle_schema)

    result = {
        "status": "PASS" if diff is None else "FAIL_SCHEMA",
        "mismatch": diff,
        "table_schema_cli": str(table_schema_cli),
        "showtableinfo_file": str(showtableinfo_file),
        "oracle_json": str(oracle_json),
    }
    print(json.dumps(result, sort_keys=True, indent=2))

    return 0 if diff is None else 1


if __name__ == "__main__":
    sys.exit(main())
