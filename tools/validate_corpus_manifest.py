#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
from typing import Any

try:
    import yaml
except ImportError:  # pragma: no cover - optional dependency
    yaml = None

REQUIRED_FEATURE_CLASSES = [
    "generic_table_scalar_keywords",
    "generic_table_array_columns",
    "storage_manager_standard",
    "storage_manager_incremental",
    "storage_manager_tiledshape",
    "storage_manager_tileddata",
    "storage_manager_tiledcolumn",
    "storage_manager_tiledcell",
    "record_valued_metadata",
    "table_measures_metadata",
    "measurement_set_tree",
    "pagedimage_coordinates",
    "fits_fixtures",
    "persistent_expression_artifacts",
]

HEX64_RE = re.compile(r"^[0-9a-f]{64}$")


def load_manifest(path: pathlib.Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        if yaml is None:
            raise SystemExit("Manifest is YAML and PyYAML is unavailable; install python3-yaml")
        data = yaml.safe_load(text)
    if not isinstance(data, dict):
        raise SystemExit("Manifest must be a YAML/JSON object")
    return data


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


def resolve_manifest_path(
    source: dict[str, Any],
    repo_root: pathlib.Path,
    casacore_build_root: str | None,
) -> pathlib.Path | None:
    kind = source.get("kind")
    if kind == "replay":
        fixture = source.get("fixture_dir")
        if not isinstance(fixture, str):
            return None
        return (repo_root / fixture).resolve()

    if kind == "path":
        raw = source.get("path")
        if not isinstance(raw, str):
            return None
        if casacore_build_root:
            raw = raw.replace("${CASACORE_BUILD_ROOT}", casacore_build_root)
        return pathlib.Path(raw).expanduser().resolve()

    return None


def validate_manifest_structure(manifest: dict[str, Any]) -> list[str]:
    errors: list[str] = []

    if manifest.get("contract_version") != "0.1":
        errors.append("contract_version must be '0.1'")

    baseline = manifest.get("baseline")
    if not isinstance(baseline, dict):
        errors.append("baseline must be an object")
    else:
        for key in ("upstream_commit", "fork_commit"):
            value = baseline.get(key)
            if not isinstance(value, str) or not value:
                errors.append(f"baseline.{key} must be a non-empty string")

    artifacts = manifest.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        errors.append("artifacts must be a non-empty list")
        return errors

    seen_ids: set[str] = set()
    for idx, artifact in enumerate(artifacts):
        if not isinstance(artifact, dict):
            errors.append(f"artifacts[{idx}] must be an object")
            continue

        artifact_id = artifact.get("artifact_id")
        if not isinstance(artifact_id, str) or not artifact_id:
            errors.append(f"artifacts[{idx}].artifact_id must be a non-empty string")
            continue
        if artifact_id in seen_ids:
            errors.append(f"duplicate artifact_id: {artifact_id}")
        seen_ids.add(artifact_id)

        if not isinstance(artifact.get("artifact_type"), str) or not artifact.get("artifact_type"):
            errors.append(f"artifacts[{idx}] ({artifact_id}) missing artifact_type")

        tags = artifact.get("feature_tags")
        if not isinstance(tags, list) or not tags or not all(isinstance(tag, str) and tag for tag in tags):
            errors.append(f"artifacts[{idx}] ({artifact_id}) feature_tags must be a non-empty list of strings")

        source = artifact.get("source")
        if not isinstance(source, dict):
            errors.append(f"artifacts[{idx}] ({artifact_id}) source must be an object")
        else:
            kind = source.get("kind")
            if kind not in {"path", "replay"}:
                errors.append(f"artifacts[{idx}] ({artifact_id}) source.kind must be 'path' or 'replay'")
            if kind == "path" and (not isinstance(source.get("path"), str) or not source.get("path")):
                errors.append(f"artifacts[{idx}] ({artifact_id}) source.path must be a non-empty string")
            if kind == "replay" and (not isinstance(source.get("fixture_dir"), str) or not source.get("fixture_dir")):
                errors.append(f"artifacts[{idx}] ({artifact_id}) source.fixture_dir must be a non-empty string")

        checksum = artifact.get("checksum")
        if not isinstance(checksum, dict):
            errors.append(f"artifacts[{idx}] ({artifact_id}) checksum must be an object")
        else:
            value = checksum.get("value")
            if not isinstance(value, str) or not HEX64_RE.match(value):
                errors.append(f"artifacts[{idx}] ({artifact_id}) checksum.value must be a 64-char lowercase hex string")
            algo = checksum.get("algorithm")
            if algo not in {"sha256-file", "sha256-tree-v1"}:
                errors.append(
                    f"artifacts[{idx}] ({artifact_id}) checksum.algorithm must be sha256-file or sha256-tree-v1"
                )

    coverage = manifest.get("required_feature_coverage")
    if not isinstance(coverage, dict):
        errors.append("required_feature_coverage must be an object")
        return errors

    for feature in REQUIRED_FEATURE_CLASSES:
        entry = coverage.get(feature)
        if not isinstance(entry, dict):
            errors.append(f"required_feature_coverage.{feature} must be an object")
            continue

        status = entry.get("status")
        if status not in {"covered", "unavailable"}:
            errors.append(f"required_feature_coverage.{feature}.status must be covered|unavailable")
            continue

        if status == "covered":
            refs = entry.get("artifacts")
            if not isinstance(refs, list) or not refs or not all(isinstance(item, str) and item for item in refs):
                errors.append(f"required_feature_coverage.{feature}.artifacts must be a non-empty list when covered")
            else:
                for artifact_id in refs:
                    if artifact_id not in seen_ids:
                        errors.append(f"required_feature_coverage.{feature} references unknown artifact_id {artifact_id}")

        if status == "unavailable":
            mitigation = entry.get("mitigation")
            if not isinstance(mitigation, str) or not mitigation.strip():
                errors.append(f"required_feature_coverage.{feature}.mitigation must be non-empty when unavailable")

    return errors


def verify_checksums(
    manifest: dict[str, Any],
    repo_root: pathlib.Path,
    casacore_build_root: str | None,
) -> list[str]:
    errors: list[str] = []

    for artifact in manifest.get("artifacts", []):
        artifact_id = artifact["artifact_id"]
        source = artifact["source"]
        path = resolve_manifest_path(source, repo_root, casacore_build_root)
        if path is None:
            errors.append(f"{artifact_id}: cannot resolve source path")
            continue

        if not path.exists():
            required_local = bool(source.get("required_local", False))
            if required_local:
                errors.append(f"{artifact_id}: required path not found: {path}")
            continue

        observed = hash_path(path)
        expected = artifact.get("checksum", {}).get("value")
        if observed != expected:
            errors.append(f"{artifact_id}: checksum mismatch expected={expected} observed={observed}")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate docs/phase0/corpus_manifest.yaml")
    parser.add_argument("manifest", help="Manifest path")
    parser.add_argument(
        "--verify-paths",
        action="store_true",
        help="Verify checksums for artifacts whose sources exist in current environment",
    )
    parser.add_argument("--casacore-build-root", default=None)
    args = parser.parse_args()

    manifest_path = pathlib.Path(args.manifest).resolve()
    repo_root = pathlib.Path(__file__).resolve().parent.parent

    manifest_data = load_manifest(manifest_path)

    errors = validate_manifest_structure(manifest_data)
    if args.verify_paths:
        errors.extend(verify_checksums(manifest_data, repo_root, args.casacore_build_root))

    if errors:
        for err in errors:
            print(f"ERROR: {err}")
        return 1

    print("Manifest validation passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
