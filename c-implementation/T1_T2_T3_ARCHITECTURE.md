# SAV IPFIX T1/T2/T3 Template Architecture（新架构）

## 概述

本实现以“固定规则宇宙”为前提，生成语义自证的 spoofed packets，并在 exporter 侧直接聚合导出三套互不依赖的观测模型：

```
[Fixed SAV Rule Sets]
                      ↓
[Semantic-consistent Spoofed Packets]
                      ↓
[Exporter-side Flow Aggregation]
                      ↓
[T1 / T2 / T3 Templates]
                      ↓
[Collector / Display]
```

强约束（本仓库实现必须遵守）：

- packet 是唯一事实源
- exporter 只做“按模板定义聚合”，不做额外推断
- T1/T2/T3 互不依赖（不存在“从 T1 再派生 T2/T3”）
- `subTemplateID` 固定为 900–903

对应实现入口：

- packet 生成与导出：`c-implementation/src/sav_models.c`
- E2E：`c-implementation/test/test_sav_e2e.c`

---

## Validation Modes（不显式编码）

validation mode 由 `savRuleType × savTargetType` 唯一决定：

| Mode | savRuleType   | savTargetType       | 含义                     |
| ---- | ------------- | ------------------- | ------------------------ |
| 1    | allowlist (0) | interface-based (0) | IB prefix allowlist      |
| 2    | blocklist (1) | interface-based (0) | IB prefix blocklist      |
| 3    | allowlist (0) | prefix-based (1)    | PB interface allowlist   |
| 4    | blocklist (1) | prefix-based (1)    | PB interface blocklist   |

---

## SAV Rule Tuples（= savMatchedContentList 的 subtemplate 内容）

### Mode 1 / 2（Interface → Prefix）

- IPv4：SubTemplate 900
   - `ingressInterface`
   - `sourceIPv4Prefix`
   - `sourceIPv4PrefixLength`
- IPv6：SubTemplate 901
   - `ingressInterface`
   - `sourceIPv6Prefix`
   - `sourceIPv6PrefixLength`

### Mode 3 / 4（Prefix → Interface）

- IPv4：SubTemplate 902
   - `sourceIPv4Prefix`
   - `sourceIPv4PrefixLength`
   - `ingressInterface`
- IPv6：SubTemplate 903
   - `sourceIPv6Prefix`
   - `sourceIPv6PrefixLength`
   - `ingressInterface`

---

## savMatchedContentList 约束（强制）

- allowlist：list length ≥ 1，semantic = allOf
- blocklist：list length = 1，semantic = exactlyOneOf
- 同一个 Data Record：subTemplateID 必须一致，且不允许混合 mode

---

## T1 / T2 / T3 定义（互不依赖）

### T1 — Rule-Outcome（最细）

- Flow Key：`ingressInterface + sourceIP + savRuleType + savTargetType + savMatchedContentList`
- 非 Key IE：`flowStartMilliseconds + flowEndMilliseconds + packetDeltaCount + octetDeltaCount + savPolicyAction + observationTimeMilliseconds`

### T2 — Interface（运维视角）

- Flow Key：`ingressInterface + savRuleType + savTargetType + savMatchedContentList`
- 非 Key IE：`flowStartMilliseconds + flowEndMilliseconds + packetDeltaCount + octetDeltaCount + savPolicyAction`

### T3 — Prefix / Mode（态势视角）

- Flow Key：`sourcePrefix + savRuleType + savTargetType + savMatchedContentList`
- 非 Key IE：`flowStartMilliseconds + flowEndMilliseconds + packetDeltaCount + octetDeltaCount + savPolicyAction`

---

## T3 的 Prefix 生成（方案 A，固定掩码）

- IPv4：从 `sourceIPv4Address` 按 /24 掩码得到 `sourceIPv4Prefix`
- IPv6：从 `sourceIPv6Address` 按 /48 掩码得到 `sourceIPv6Prefix`

---

## Template IDs（实现固定，Task 1）

- 400：T1
- 410：T2 / IPv4
- 420：T2 / IPv6
- 430：T3 / IPv4
- 440：T3 / IPv6

---

## 运行方式

```bash
cd c-implementation
make clean
make tests
./build/bin/test_test_sav_e2e
```

该程序会生成 `test_sav_e2e.ipfix`，其中包含 exporter-side 聚合后的 T1/T2/T3 records。
