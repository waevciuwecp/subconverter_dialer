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
out_v112="$tmp_dir/singbox_v112.json"
out_v114="$tmp_dir/singbox_v114.json"
out_v111="$tmp_dir/singbox_v111.json"
err_v111="$tmp_dir/singbox_v111.err"

ss_link_1='ss://YWVzLTEyOC1nY206cHdk@1.1.1.1:8388#awesome-node'
ss_link_2='ss://YWVzLTEyOC1nY206cHdk@2.2.2.2:8388#relay-node'
ss_link_3='ss://YWVzLTEyOC1nY206cHdk@3.3.3.3:8388#plain-node'

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
ruleset=Proxy,[]DOMAIN,example.com

[proxy_groups]
custom_proxy_group=dialer-select\`select-use\`(sub|relay)
custom_proxy_group=dialer-lb\`load-balance-use\`(sub|relay)\`http://www.gstatic.com/generate_204\`6537,,100\`round-robin
custom_proxy_group=dialer\`select\`[]dialer-select\`[]dialer-lb\`[]DIRECT
custom_proxy_group=Proxy\`select\`[]awesome-node\`[]plain-node\`[]DIRECT
PREF

cat > "$gen_path" <<GEN
[singbox_v112]
path=$out_v112
target=singbox
ver=1.12.0
url=tag:sub,$ss_link_1|tag:relay,$ss_link_2|tag:other,$ss_link_3
use_dialer=true
dialer_group_name=dialer
apply_dialer_to=awesome

[singbox_v114]
path=$out_v114
target=singbox
singbox_ver=1.14.0
url=tag:sub,$ss_link_1|tag:relay,$ss_link_2|tag:other,$ss_link_3
use_dialer=true
dialer_group_name=dialer
apply_dialer_to=awesome

[singbox_v111_invalid]
path=$out_v111
target=singbox
singbox_ver=1.11.0
url=$ss_link_1
GEN

(
  cd "$tmp_dir"
  "$bin_path" -f "$pref_path" -g --artifact singbox_v112 >/dev/null 2>&1
  "$bin_path" -f "$pref_path" -g --artifact singbox_v114 >/dev/null 2>&1
  if "$bin_path" -f "$pref_path" -g --artifact singbox_v111_invalid >"$err_v111" 2>&1; then
    echo "expected singbox 1.11 artifact generation to fail" >&2
    exit 1
  fi
)

if ! rg -n --fixed-strings 'Invalid singbox_ver! Supported range: 1.12.x - 1.14.x' "$err_v111" >/dev/null; then
  echo "expected 1.11 validation error not found" >&2
  cat "$err_v111" >&2
  exit 1
fi

assert_non_empty() {
  local path="$1"
  if [[ ! -s "$path" ]]; then
    echo "expected non-empty output file: $path" >&2
    exit 1
  fi
}

assert_contains_fixed() {
  local path="$1"
  local pattern="$2"
  if ! rg -n --fixed-strings "$pattern" "$path" >/dev/null; then
    echo "expected pattern not found in $path: $pattern" >&2
    exit 1
  fi
}

assert_jq_true() {
  local path="$1"
  local expr="$2"
  if ! jq -e "$expr" "$path" >/dev/null; then
    echo "expected jq expression to be true for $path: $expr" >&2
    exit 1
  fi
}

assert_non_empty "$out_v112"
assert_non_empty "$out_v114"

assert_contains_fixed "$out_v112" "\"awesome-node\""
assert_contains_fixed "$out_v114" "\"awesome-node\""
assert_contains_fixed "$out_v112" "\"relay-node\""
assert_contains_fixed "$out_v114" "\"relay-node\""
assert_contains_fixed "$out_v112" "\"detour\":\"dialer\""
assert_contains_fixed "$out_v114" "\"detour\":\"dialer\""

detour_count_v112="$(rg -o --fixed-strings '"detour":"dialer"' "$out_v112" | wc -l | tr -d ' ')"
detour_count_v114="$(rg -o --fixed-strings '"detour":"dialer"' "$out_v114" | wc -l | tr -d ' ')"
if [[ "$detour_count_v112" != "1" ]]; then
  echo "expected exactly one detour in v1.12 output, got $detour_count_v112" >&2
  exit 1
fi
if [[ "$detour_count_v114" != "1" ]]; then
  echo "expected exactly one detour in v1.14 output, got $detour_count_v114" >&2
  exit 1
fi

assert_contains_fixed "$out_v112" "\"action\":\"route\""
assert_contains_fixed "$out_v114" "\"action\":\"route\""
assert_contains_fixed "$out_v112" "\"tag\":\"dialer-select\""
assert_contains_fixed "$out_v114" "\"tag\":\"dialer-select\""
assert_contains_fixed "$out_v112" "\"tag\":\"dialer-lb\""
assert_contains_fixed "$out_v114" "\"tag\":\"dialer-lb\""
assert_contains_fixed "$out_v112" "\"tag\":\"dialer\""
assert_contains_fixed "$out_v114" "\"tag\":\"dialer\""
assert_contains_fixed "$out_v112" "\"tag\":\"REJECT\""
assert_contains_fixed "$out_v114" "\"tag\":\"REJECT\""
assert_contains_fixed "$out_v112" "\"outbounds\":[\"awesome-node\",\"relay-node\"]"
assert_contains_fixed "$out_v114" "\"outbounds\":[\"awesome-node\",\"relay-node\"]"
assert_contains_fixed "$out_v112" "\"outbounds\":[\"dialer-select\",\"dialer-lb\",\"DIRECT\"]"
assert_contains_fixed "$out_v114" "\"outbounds\":[\"dialer-select\",\"dialer-lb\",\"DIRECT\"]"
assert_jq_true "$out_v112" '.dns.servers[] | select(.tag == "dns_direct") | .type == "udp" and .server == "223.5.5.5"'
assert_jq_true "$out_v114" '.dns.servers[] | select(.tag == "dns_direct") | .type == "udp" and .server == "223.5.5.5"'
assert_jq_true "$out_v112" '([.dns.servers[]? | select(.detour == "DIRECT")] | length) == 0'
assert_jq_true "$out_v114" '([.dns.servers[]? | select(.detour == "DIRECT")] | length) == 0'
assert_jq_true "$out_v112" '(.ntp.detour // "") != "DIRECT"'
assert_jq_true "$out_v114" '(.ntp.detour // "") != "DIRECT"'
assert_jq_true "$out_v112" '.dns.fakeip == null'
assert_jq_true "$out_v114" '.dns.fakeip == null'
assert_jq_true "$out_v112" '.route.default_domain_resolver == "dns_direct"'
assert_jq_true "$out_v114" '.route.default_domain_resolver == "dns_direct"'

echo "PASS: singbox dialer output is valid for 1.12-1.14 and 1.11 is rejected"
