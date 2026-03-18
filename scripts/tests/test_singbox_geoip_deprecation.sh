#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin_path="${SUBCONVERTER_BIN:-$repo_root/build/subconverter}"

if [[ ! -x "$bin_path" ]]; then
  echo "subconverter binary not found or not executable: $bin_path" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

cp "$repo_root/base/base/singbox.json" "$tmp_dir/singbox.json"

pref_path="$tmp_dir/pref.ini"
gen_path="$tmp_dir/generate.ini"
out_v111="$tmp_dir/singbox_v111.json"
out_v114="$tmp_dir/singbox_v114.json"

ss_link='ss://YWVzLTEyOC1nY206cHdk@1.1.1.1:8388#awesome-node'

cat > "$pref_path" <<PREF
[common]
api_mode=false
base_path=.
singbox_rule_base=singbox.json

[template]
template_path=.

[rulesets]
enabled=true
overwrite_original_rules=true
ruleset=Proxy,[]GEOIP,CN
ruleset=Proxy,[]SRC-GEOIP,CN
ruleset=Proxy,[]GEOSITE,GOOGLE
ruleset=Proxy,[]DOMAIN,example.com

[proxy_groups]
custom_proxy_group=Proxy\`select\`[]awesome-node\`[]DIRECT
PREF

cat > "$gen_path" <<GEN
[singbox_v111]
path=$out_v111
target=singbox
singbox_ver=1.11.0
url=$ss_link

[singbox_v114]
path=$out_v114
target=singbox
singbox_ver=1.14.0
url=$ss_link
GEN

(
  cd "$tmp_dir"
  "$bin_path" -f "$pref_path" -g --artifact singbox_v111 >/dev/null 2>&1
  "$bin_path" -f "$pref_path" -g --artifact singbox_v114 >/dev/null 2>&1
)

assert_non_empty() {
  local path="$1"
  if [[ ! -s "$path" ]]; then
    echo "expected non-empty output file: $path" >&2
    exit 1
  fi
}

assert_rule_key_state() {
  local path="$1"
  local key="$2"
  local expect="$3"
  local exists="false"
  if jq -e --arg key "$key" '.route.rules // [] | map(has($key)) | any' "$path" >/dev/null; then
    exists="true"
  fi
  if [[ "$exists" != "$expect" ]]; then
    echo "expected rule key '$key'=$expect in $path, got $exists" >&2
    exit 1
  fi
}

assert_non_empty "$out_v111"
assert_non_empty "$out_v114"

# 1.11 keeps legacy database rules.
assert_rule_key_state "$out_v111" "geoip" "true"
assert_rule_key_state "$out_v111" "source_geoip" "true"
assert_rule_key_state "$out_v111" "geosite" "true"

# 1.12+ must drop deprecated database rules to avoid invalid configs.
assert_rule_key_state "$out_v114" "geoip" "false"
assert_rule_key_state "$out_v114" "source_geoip" "false"
assert_rule_key_state "$out_v114" "geosite" "false"

# Non-deprecated rules should still be generated.
assert_rule_key_state "$out_v114" "domain" "true"

echo "PASS: singbox >=1.12 skips deprecated geoip/geosite database rules"
