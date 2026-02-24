#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

HOOKS_DIR="$(git rev-parse --git-path hooks)"
PRE_PUSH_HOOK="${HOOKS_DIR}/pre-push"

cat >"${PRE_PUSH_HOOK}" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(git rev-parse --show-toplevel)"
cd "${ROOT_DIR}"

bash tools/pre_push_quality.sh
EOF

chmod +x "${PRE_PUSH_HOOK}"

echo "Installed pre-push hook: ${PRE_PUSH_HOOK}"
