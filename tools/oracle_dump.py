#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
from typing import Any

try:
    import yaml
except ImportError:  # pragma: no cover - optional dependency
    yaml = None

CONTRACT_VERSION = "0.1"


def load_manifest(path: pathlib.Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        if yaml is None:
            raise SystemExit("Manifest is YAML and PyYAML is unavailable; install python3-yaml")
        data = yaml.safe_load(text)
    if not isinstance(data, dict):
        raise SystemExit(f"Manifest must be an object: {path}")
    return data


def canonical_text(text: str) -> str:
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    while lines and lines[-1] == "":
        lines.pop()
    return "\n".join(lines) + "\n"


def sha256_hex(data: bytes) -> str:
    digest = hashlib.sha256()
    digest.update(data)
    return digest.hexdigest()


def hash_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_path(path: pathlib.Path) -> str:
    if path.is_file():
        return hash_file(path)
    if not path.is_dir():
        raise FileNotFoundError(f"Path not found for hashing: {path}")

    digest = hashlib.sha256()
    for file_path in sorted(p for p in path.rglob("*") if p.is_file()):
        rel_path = file_path.relative_to(path).as_posix()
        digest.update(rel_path.encode("utf-8"))
        digest.update(b"\0")
        digest.update(hash_file(file_path).encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest()


def json_dump(path: pathlib.Path, payload: dict[str, Any]) -> None:
    text = json.dumps(payload, sort_keys=True, indent=2)
    path.write_text(text + "\n", encoding="utf-8")


def parse_showtableinfo(show_text: str) -> dict[str, Any]:
    lines = show_text.splitlines()
    table_path = ""
    table_kind = "table"
    row_count = 0
    col_count = 0

    table_match = re.search(r"^Structure of table\s+(.+)$", show_text, re.MULTILINE)
    if table_match:
        table_path = table_match.group(1).strip()

    kind_match = re.search(r"^------------------\s*(.*)$", show_text, re.MULTILINE)
    if kind_match and kind_match.group(1).strip():
        table_kind = kind_match.group(1).strip()

    rc_match = re.search(r"^\s*(\d+)\s+rows,\s+(\d+)\s+columns", show_text, re.MULTILINE)
    if rc_match:
        row_count = int(rc_match.group(1))
        col_count = int(rc_match.group(2))

    columns: list[dict[str, Any]] = []
    managers: list[dict[str, Any]] = []
    manager_by_key: dict[tuple[str, str, str], dict[str, Any]] = {}

    current_manager: dict[str, Any] | None = None
    in_schema = False
    for line in lines:
        stripped = line.rstrip("\n")
        if stripped.startswith("Keywords of main table"):
            break
        if stripped.startswith("Structure of table") or stripped.startswith("------------------"):
            in_schema = True
            continue
        if not in_schema:
            continue
        if not stripped.strip():
            continue

        mgr_match = re.match(r"^\s+([A-Za-z0-9_]+StMan)\s+file=([^\s]+)\s+name=([^\s]*)", stripped)
        if mgr_match:
            key = (mgr_match.group(1), mgr_match.group(2), mgr_match.group(3))
            current_manager = manager_by_key.get(key)
            if current_manager is None:
                current_manager = {
                    "class": mgr_match.group(1),
                    "file": mgr_match.group(2),
                    "name": mgr_match.group(3),
                }
                manager_by_key[key] = current_manager
                managers.append(current_manager)
            continue

        col_match = re.match(r"^\s{2}([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s+(.+)$", stripped)
        if not col_match:
            continue

        descriptor = col_match.group(3).strip()
        if "=" in descriptor and ("hypercubes" in descriptor.lower() or "bucketsize" in descriptor.lower()):
            continue
        if descriptor.startswith("file="):
            continue

        shape: list[int] | None = None
        shape_match = re.search(r"shape=\[([^\]]*)\]", descriptor)
        if shape_match:
            shape = [int(part.strip()) for part in shape_match.group(1).split(",") if part.strip()]

        ndim: int | None = None
        ndim_match = re.search(r"ndim=(\d+)", descriptor)
        if ndim_match:
            ndim = int(ndim_match.group(1))

        value_kind = "unknown"
        lower_desc = descriptor.lower()
        if "scalar" in lower_desc:
            value_kind = "scalar"
        elif "array" in lower_desc or "shape=" in lower_desc or "ndim=" in lower_desc:
            value_kind = "array"

        columns.append(
            {
                "name": col_match.group(1),
                "data_type": col_match.group(2),
                "descriptor": descriptor,
                "value_kind": value_kind,
                "shape": shape,
                "ndim": ndim,
                "storage_manager": current_manager,
            }
        )

    keywords_section = ""
    kw_marker = "Keywords of main table"
    kw_index = show_text.find(kw_marker)
    if kw_index >= 0:
        keywords_section = show_text[kw_index + len(kw_marker) :]

    type_leaves: list[dict[str, str]] = []
    for line in keywords_section.splitlines():
        leaf_match = re.match(r"^\s*([A-Za-z0-9_]+):\s+([A-Za-z][A-Za-z0-9_]*)\b", line)
        if leaf_match:
            type_leaves.append({"key": leaf_match.group(1), "type": leaf_match.group(2)})

    return {
        "table_path": table_path,
        "table_kind": table_kind,
        "row_count": row_count,
        "column_count": col_count,
        "columns": columns,
        "storage_managers": managers,
        "keywords": {
            "raw_showtableinfo_sha256": sha256_hex(canonical_text(keywords_section).encode("utf-8")),
            "typed_leaves": type_leaves,
            "raw_showtableinfo": canonical_text(keywords_section),
        },
        "raw_showtableinfo_sha256": sha256_hex(canonical_text(show_text).encode("utf-8")),
    }


def sample_indices(row_count: int, full_threshold: int) -> tuple[list[int], str]:
    if row_count <= 0:
        return ([], "full")
    if row_count <= full_threshold:
        return (list(range(row_count)), "full")

    stride = max(1, row_count // 1024)
    sampled = {0, row_count - 1}
    for idx in range(0, row_count, stride):
        sampled.add(idx)
    ordered = sorted(sampled)
    if len(ordered) > 4096:
        anchors = {0, row_count - 1}
        keep = sorted([idx for idx in ordered if idx in anchors])
        for idx in ordered:
            if idx in anchors:
                continue
            if len(keep) >= 4096:
                break
            keep.append(idx)
        ordered = sorted(set(keep))
    return (ordered, "sampled")


def run_command(args: list[str]) -> str:
    proc = subprocess.run(args, capture_output=True, text=True, check=False)
    if proc.returncode != 0:
        cmd = " ".join(args)
        raise RuntimeError(f"Command failed ({proc.returncode}): {cmd}\n{proc.stderr.strip()}")
    return proc.stdout


def resolve_path(raw_path: str, casacore_build_root: str | None) -> pathlib.Path:
    value = raw_path
    if "${CASACORE_BUILD_ROOT}" in value and not casacore_build_root:
        raise SystemExit(
            "Manifest path uses ${CASACORE_BUILD_ROOT}; pass --casacore-build-root or set CASACORE_BUILD_ROOT"
        )
    if casacore_build_root:
        value = value.replace("${CASACORE_BUILD_ROOT}", casacore_build_root)
    return pathlib.Path(value).expanduser().resolve()


def git_revision(repo_root: pathlib.Path) -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "--short=12", "HEAD"],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        return "unknown"
    return proc.stdout.strip() or "unknown"


def dump_replay_artifact(
    artifact: dict[str, Any],
    repo_root: pathlib.Path,
    full_threshold: int,
) -> tuple[dict[str, Any], list[str]]:
    source = artifact["source"]
    fixture_dir = (repo_root / source["fixture_dir"]).resolve()
    showtable_path = fixture_dir / "showtableinfo.txt"
    if not showtable_path.exists():
        raise FileNotFoundError(f"Replay showtableinfo fixture missing: {showtable_path}")

    show_text = showtable_path.read_text(encoding="utf-8")
    parsed = parse_showtableinfo(show_text)

    row_count = int(parsed["row_count"])
    rows, value_mode = sample_indices(row_count, full_threshold)
    unsupported: list[str] = []

    column_payloads: list[dict[str, Any]] = []
    for column in parsed["columns"]:
        taql_path = fixture_dir / "taql" / f"{column['name']}.txt"
        if not taql_path.exists():
            unsupported.append(f"missing replay taql fixture for column {column['name']}")
            continue
        taql_text = taql_path.read_text(encoding="utf-8")
        canonical = canonical_text(taql_text)
        column_payloads.append(
            {
                "column": column["name"],
                "row_count": row_count,
                "value_mode": value_mode,
                "sample_row_indices": rows,
                "full_payload_sha256": sha256_hex(canonical.encode("utf-8")),
                "sample_preview": canonical.splitlines()[:20],
            }
        )

    artifact_hash = hash_path(fixture_dir)

    doc = {
        "artifact": {
            "artifact_id": artifact["artifact_id"],
            "artifact_type": artifact["artifact_type"],
            "content_hash": artifact_hash,
            "source_path": str(fixture_dir),
        },
        "data": {
            "column_payloads": column_payloads,
            "row_count": row_count,
        },
        "keywords": parsed["keywords"],
        "schema": {
            "column_count": parsed["column_count"],
            "columns": parsed["columns"],
            "row_count": row_count,
            "storage_managers": parsed["storage_managers"],
            "table_kind": parsed["table_kind"],
            "table_path": parsed["table_path"],
        },
    }
    return (doc, unsupported)


def dump_casacore_artifact(
    artifact: dict[str, Any],
    casacore_build_root: str | None,
    showtableinfo_bin: str,
    taql_bin: str,
    full_threshold: int,
) -> tuple[dict[str, Any], list[str]]:
    source = artifact["source"]
    source_path = resolve_path(source["path"], casacore_build_root)
    if artifact["artifact_type"] == "fits":
        content_hash = hash_path(source_path)
        doc = {
            "artifact": {
                "artifact_id": artifact["artifact_id"],
                "artifact_type": artifact["artifact_type"],
                "content_hash": content_hash,
                "source_path": str(source_path),
            },
            "schema": {
                "column_count": 0,
                "columns": [],
                "row_count": 0,
                "storage_managers": [],
                "table_kind": "fits",
                "table_path": str(source_path),
            },
            "keywords": {
                "raw_showtableinfo": "",
                "raw_showtableinfo_sha256": sha256_hex(b"\n"),
                "typed_leaves": [],
            },
            "data": {
                "column_payloads": [],
                "row_count": 0,
            },
        }
        return (doc, ["fits_payload_not_decoded_v0"])

    show_text = run_command(
        [
            showtableinfo_bin,
            f"in={source_path}",
            "dm=t",
            "col=t",
            "tabkey=t",
            "colkey=t",
            "maxval=25",
        ]
    )
    parsed = parse_showtableinfo(show_text)
    row_count = int(parsed["row_count"])
    rows, value_mode = sample_indices(row_count, full_threshold)

    if row_count > 0:
        row_clause = f" where rownr() in [{','.join(str(idx) for idx in rows)}]"
    else:
        row_clause = " where false"

    unsupported: list[str] = []
    payloads: list[dict[str, Any]] = []
    for column in parsed["columns"]:
        query = f"select {column['name']} from {source_path}{row_clause}"
        taql_out = run_command([taql_bin, "-ps", "-ph", "-pr", query])
        canonical = canonical_text(taql_out)
        payloads.append(
            {
                "column": column["name"],
                "row_count": row_count,
                "value_mode": value_mode,
                "sample_row_indices": rows,
                "full_payload_sha256": sha256_hex(canonical.encode("utf-8")),
                "sample_preview": canonical.splitlines()[:20],
            }
        )

    content_hash = hash_path(source_path)
    doc = {
        "artifact": {
            "artifact_id": artifact["artifact_id"],
            "artifact_type": artifact["artifact_type"],
            "content_hash": content_hash,
            "source_path": str(source_path),
        },
        "data": {
            "column_payloads": payloads,
            "row_count": row_count,
        },
        "keywords": parsed["keywords"],
        "schema": {
            "column_count": parsed["column_count"],
            "columns": parsed["columns"],
            "row_count": row_count,
            "storage_managers": parsed["storage_managers"],
            "table_kind": parsed["table_kind"],
            "table_path": parsed["table_path"],
        },
    }
    return (doc, unsupported)


def main() -> int:
    parser = argparse.ArgumentParser(description="Dump canonical oracle JSON for Phase 0 artifacts")
    parser.add_argument("--manifest", required=True, help="Path to docs/phase0/corpus_manifest.yaml")
    parser.add_argument("--artifact-id", action="append", default=[], help="Artifact ID(s) to dump (default: all)")
    parser.add_argument("--output-dir", required=True, help="Directory for output JSON files")
    parser.add_argument("--casacore-build-root", default=os.environ.get("CASACORE_BUILD_ROOT"))
    parser.add_argument("--showtableinfo", default=os.environ.get("SHOWTABLEINFO_BIN", "showtableinfo"))
    parser.add_argument("--taql", default=os.environ.get("TAQL_BIN", "taql"))
    parser.add_argument("--producer-version", default="0.1.0")
    parser.add_argument("--full-threshold", type=int, default=1024)
    parser.add_argument("--strict-checksum", action="store_true", help="Fail if manifest checksum mismatches")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent.parent
    manifest_path = pathlib.Path(args.manifest).resolve()
    manifest = load_manifest(manifest_path)
    artifacts = manifest.get("artifacts", [])

    selected_ids = set(args.artifact_id)
    selected = artifacts
    if selected_ids:
        selected = [item for item in artifacts if item.get("artifact_id") in selected_ids]
        missing = sorted(selected_ids - {item.get("artifact_id") for item in selected})
        if missing:
            raise SystemExit(f"Unknown artifact IDs: {', '.join(missing)}")

    output_dir = pathlib.Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    source_commit = git_revision(repo_root)
    generated_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    for artifact in selected:
        source_kind = artifact.get("source", {}).get("kind")
        if source_kind == "replay":
            doc, unsupported = dump_replay_artifact(artifact, repo_root, args.full_threshold)
        elif source_kind == "path":
            doc, unsupported = dump_casacore_artifact(
                artifact,
                args.casacore_build_root,
                args.showtableinfo,
                args.taql,
                args.full_threshold,
            )
        else:
            raise SystemExit(f"Unsupported source.kind for artifact {artifact.get('artifact_id')}: {source_kind}")

        declared_checksum = artifact.get("checksum", {}).get("value")
        observed_checksum = doc["artifact"]["content_hash"]
        checksum_matches = (declared_checksum == observed_checksum)
        if args.strict_checksum and declared_checksum and not checksum_matches:
            raise SystemExit(
                f"Checksum mismatch for {artifact['artifact_id']}: manifest={declared_checksum} observed={observed_checksum}"
            )

        payload = {
            "artifact": doc["artifact"],
            "baseline": manifest.get("baseline", {}),
            "contract_version": CONTRACT_VERSION,
            "data": doc["data"],
            "diagnostics": {
                "checksum_matches_manifest": checksum_matches,
                "generated_at_utc": generated_at,
                "manifest_path": str(manifest_path),
                "notes": unsupported,
            },
            "keywords": doc["keywords"],
            "producer": {
                "source_commit": source_commit,
                "tool_name": "oracle_dump",
                "tool_version": args.producer_version,
            },
            "schema": doc["schema"],
        }

        out_path = output_dir / f"{artifact['artifact_id']}.oracle.json"
        json_dump(out_path, payload)
        print(out_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
