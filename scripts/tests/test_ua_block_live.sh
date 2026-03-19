#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

"$repo_root/scripts/tests/test_ua_block_live_native.sh"
"$repo_root/scripts/tests/test_ua_block_live_docker.sh"

echo "PASS: UA blocker live tests (native + docker) succeeded"
