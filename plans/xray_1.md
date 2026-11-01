# Xray TLS 验证迁移：完成情况与验收范围

本文件是工程计划及状态记录，不是要求重复实施的任务指令。

## 已实现

基础实现已提交为 `c60cebf`：

- 内部模型保留旧版不安全校验意图；继续解析 `scv`、`allowInsecure`、`skip-cert-verify`。
- Xray 兼容分享链接导出不再写出 `allowInsecure`；忽略旧意图时通过日志警告。
- 支持解析 `pcs` / `pinnedPeerCertSha256`、`vcn` / `verifyPeerCertByName`。
- 支持 Xray JSON 输入的 `tlsSettings.pinnedPeerCertSha256` 和 `tlsSettings.verifyPeerCertByName`。
- 指纹支持冒号形式，规范为 64 位小写十六进制；多个值以逗号分隔；名称去除两端空白。
- 节点输入的非法指纹忽略并警告；显式请求参数非法时返回 HTTP 400。
- `/sub`、`/digest` 和 compact digest 字符串别名兼容；既有 `scv` 布尔位位置不变。
- Clash 的 `skip-cert-verify`、Sing-box 的 `insecure` 保持目标各自的行为。
- 中英文 README 包含迁移说明及示例。

## 输出边界：修正原计划的假设

本仓库没有完整 Xray/V2Ray JSON 出站配置输出目标：

- `target=v2ray` 输出 VMess 分享链接。
- `target=vless`、`target=trojan` 输出对应分享链接，可携带显式 `pcs/vcn`。
- VMess 分享链接导出目前不携带 `pcs/vcn`，不宣称完整往返保存。
- Xray JSON 是输入格式之一；解析其字段不等于能生成完整 Xray JSON。
- 因此原计划中“验收输出 tlsSettings 新字段”的要求不适用于现有生成器；若需要完整 Xray JSON 输出，应另立任务并确认客户端导入契约。
- 不为不支持的输出格式杜撰校验字段；不将 `scv=true` 自动改写为证书指纹或名称。

## 本次收尾

- 补齐 HTTP 入口的 VLESS/Hysteria2 简单订阅目标白名单，避免被提前返回 HTTP 400。
- 修正 HTTP 入口 VLESS/Hysteria2 类型位接反的问题：VLESS=32，Hysteria2=16，与共享导出器保持一致。
- 修复明文 packed query 被提前整体 URL 解码的问题，保留节点 URI 内的 `&` 分隔符及传输参数。
- 扩展现有 HTTP 集成脚本，使用混合协议源验证 `/sub`、`/digest` 两个入口的协议筛选。
- 验证普通 `scv=true` 及 compact digest 的既有 `scv` 位不产生 `allowInsecure`。
- 验证 VLESS 输出的 `pcs/vcn` 规范化及非法请求返回 400；修复只有空白或逗号的显式校验参数被当成有效空值的问题。
- README 明确完整 JSON 输出与 VMess 字段透传的限制。

验收命令（使用已构建的可执行程序及测试目标）：

```sh
SUBCONVERTER_BIN="$PWD/build-live/subconverter" bash scripts/tests/test_digest_alias_q_formats.sh
build-tests-local/tests/subconverter_unit_tests --test-case='*tls*,*pcs*,*vcn*,*trojan single*' --no-breaks
```

## 独立后续工作

- 前端若要提供 `pcs/vcn` 输入控件，需要单独评估；本计划不修改 sub-web。
- REALITY X25519MLKEM768 握手兼容与普通 TLS 证书验证迁移是两项独立工作。
- Shadowrocket 等客户端仍需按实际版本验证；订阅转换不能替代客户端内核能力。
- 不通过新增不安全旁路恢复旧 `allowInsecure` 行为。

## 本次本地验收结果

- 完整可执行程序 CMake 构建通过。
- HTTP 集成脚本通过，包括原有 digest 格式、嵌套 URL 和新增协议/TLS 校验用例。
- TLS、VLESS、REALITY 相关定点测试：31 个用例、285 个断言通过。
- HTTP 集成脚本已接入 Linux amd64 构建 CI；本地通过不代表远程 CI 已通过。
- 未部署线上服务，也未验证 Shadowrocket 等闭源客户端。
