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

files=()
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

if [[ ${#files[@]} -eq 0 ]]; then
  echo "No C/C++ files found; format check skipped"
  exit 0
fi

clang-format --dry-run --Werror "${files[@]}"
echo "clang-format check passed for ${#files[@]} files"
