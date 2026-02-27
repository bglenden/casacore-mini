#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is required but was not found"
  exit 1
fi

search_roots=()
for dir in include src tests; do
  if [[ -d "${dir}" ]]; then
    search_roots+=("${dir}")
  fi
done

if [[ ${#search_roots[@]} -eq 0 ]]; then
  echo "No include/src/tests directories found; format check skipped"
  exit 0
fi

scope="${CASACORE_MINI_FORMAT_SCOPE:-changed}"

files=()
if [[ "${scope}" == "changed" ]]; then
  while IFS= read -r file; do
    [[ -n "${file}" ]] || continue
    [[ -f "${file}" ]] || continue
    case "${file}" in
      *.h|*.hpp|*.hh|*.c|*.cc|*.cpp|*.cxx)
        files+=("${file}")
        ;;
      *)
        ;;
    esac
  done < <(git diff-tree --no-commit-id --name-only -r HEAD -- "${search_roots[@]}")
else
  while IFS= read -r -d '' file; do
    files+=("${file}")
  done < <(
    find "${search_roots[@]}" -type f \
      \( \
        -name '*.h' -o \
        -name '*.hpp' -o \
        -name '*.hh' -o \
        -name '*.c' -o \
        -name '*.cc' -o \
        -name '*.cpp' -o \
        -name '*.cxx' \
      \) -print0
  )
fi

if [[ ${#files[@]} -eq 0 ]]; then
  if [[ "${scope}" == "changed" ]]; then
    echo "No changed C/C++ files to format-check; skipped"
  else
    echo "No C/C++ files found; format check skipped"
  fi
  exit 0
fi

clang-format --dry-run --Werror "${files[@]}"
echo "clang-format check passed for ${#files[@]} files (scope=${scope})"
