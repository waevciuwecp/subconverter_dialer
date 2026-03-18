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

cat > "$tmp_dir/source.json" <<'JSON'
{
  "inbounds": [],
  "outbounds": [
    {
      "type": "vless",
      "tag": "ws-node",
      "server": "cfyes.example.com",
      "server_port": 443,
      "uuid": "11111111-1111-1111-1111-111111111111",
      "packet_encoding": "xudp",
      "tls": {
        "enabled": true,
        "server_name": "sni.example.com",
        "utls": {
          "enabled": true,
          "fingerprint": "safari"
        }
      },
      "transport": {
        "type": "ws",
        "path": "/ws",
        "headers": {
          "Host": [
            "edge.example.com"
          ]
        },
        "max_early_data": 2048,
        "early_data_header_name": "Sec-WebSocket-Protocol"
      }
    }
  ],
  "route": {}
}
JSON

pref_path="$tmp_dir/pref.ini"
gen_path="$tmp_dir/generate.ini"
out_path="$tmp_dir/out.json"

cat > "$pref_path" <<PREF
[common]
api_mode=false
base_path=.
singbox_rule_base=singbox.json

[template]
template_path=.
PREF

cat > "$gen_path" <<GEN
[case]
path=$out_path
target=singbox
singbox_ver=1.14.0
url=$tmp_dir/source.json
GEN

(
  cd "$tmp_dir"
  "$bin_path" -f "$pref_path" -g --artifact case >/dev/null 2>&1
)

if [[ ! -s "$out_path" ]]; then
  echo "expected non-empty output file: $out_path" >&2
  exit 1
fi

assert_jq_true() {
  local path="$1"
  local expr="$2"
  if ! jq -e "$expr" "$path" >/dev/null; then
    echo "expected jq expression to be true for $path: $expr" >&2
    exit 1
  fi
}

assert_jq_true "$out_path" '.outbounds[] | select(.tag == "ws-node") | .transport.headers.Host == "edge.example.com"'
assert_jq_true "$out_path" '.outbounds[] | select(.tag == "ws-node") | .transport.max_early_data == 2048'
assert_jq_true "$out_path" '.outbounds[] | select(.tag == "ws-node") | .transport.early_data_header_name == "Sec-WebSocket-Protocol"'
assert_jq_true "$out_path" '.outbounds[] | select(.tag == "ws-node") | .tls.utls.fingerprint == "safari"'

echo "PASS: singbox ws transport fields round-trip correctly"
