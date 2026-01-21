# ENGINEERING_GUIDE (SAV IPFIX Validator)

本文件为“实施规范 + 验收标准 + 关键踩坑 checklist”的**原文汇编**。

- 约束：只做搬运工、一对一映射、不推断。
- 说明：以下内容按块从现有文件中**原样搬运**；仅添加来源分隔符，不对正文做改写。

---

## 来源：c-implementation/T1_T2_T3_ARCHITECTURE.md（原文）

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

- packet 生成与导出：c-implementation/src/sav_models.c
- E2E：c-implementation/test/test_sav_e2e.c

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

---
## 来源：c-implementation/LIBFIXBUF_SUBTEMPLATELIST_NOTES.md（原文）

# libfixbuf SubTemplateList 关键知识点

## 问题背景
在使用 libfixbuf 3.x 导出带有 SubTemplateList 的 IPFIX 数据时，遇到 "Missing external template" 错误。

## 关键文档
https://tools.netsa.cert.org/fixbuf/libfixbuf/templates.html
https://tools.netsa.cert.org/fixbuf/libfixbuf/public_8h.html

## 核心发现

### **关键问题：C 结构体自动 padding 必须在 Template spec 中显式声明！**

**症状**：
- `fbSubTemplateListInit` 后 `tmplID` 正确（例如 903）
- `fBufAppend` 前 `tmplID` 仍然正确
- 但 `fBufAppend` 报错 "Missing external template 0:8700"
- 8700 ≠ 903，看起来像是"随机值"

**根因**：
libfixbuf 在编码时**从错误的 offset 读取数据**，因为 C 结构体的内存布局与 Template spec 不匹配！

**示例**：
```c
// ❌ 错误的定义
typedef struct {
    uint32_t ingressInterface;      // offset 0 (4 bytes)
    uint8_t  savRuleType;           // offset 4 (1 byte)
    uint8_t  savTargetType;         // offset 5 (1 byte)  
    uint8_t  savPolicyAction;       // offset 6 (1 byte)
    // ⚠️ 编译器在这里自动添加 1 字节 padding！
    uint64_t flowStartMilliseconds; // offset 8 (不是 7！)
    // ...
} IpfixT1Record;

// ❌ Template spec 期望没有 padding
fbInfoElementSpec_t spec[] = {
    { "ingressInterface",      4, 0 },  // offset 0-3
    { "savRuleType",           1, 0 },  // offset 4
    { "savTargetType",         1, 0 },  // offset 5
    { "savPolicyAction",       1, 0 },  // offset 6
    { "flowStartMilliseconds", 8, 0 },  // 期望 offset 7，但实际是 8！
    // ...
};
```

**结果**：
- libfixbuf 按照 Template spec 从 offset 7 开始读取 `flowStartMilliseconds`
- 但实际数据在 offset 8
- offset 7 的那个字节是 padding，可能包含垃圾数据
- 当 libfixbuf 读取 `subTemplateList.tmplID` 时，offset 错位导致读到错误值（8700 而不是 903）

**✅ 正确的解决方案**：
```c
// ✅ Template spec 中显式声明 padding
fbInfoElementSpec_t spec[] = {
    { "ingressInterface",      4, 0 },
    { "savRuleType",           1, 0 },
    { "savTargetType",         1, 0 },
    { "savPolicyAction",       1, 0 },
    { "paddingOctets",         1, 0 },  // ✅ 显式声明 padding！
    { "flowStartMilliseconds", 8, 0 },
    // ...
};
```

或者使用 `__attribute__((packed))` 强制编译器不添加 padding（但这可能影响性能）。

### 1. fbSubTemplateListInit() 参数说明
```c
void * fbSubTemplateListInit(
    fbSubTemplateList_t *stl,      // 要初始化的 SubTemplateList 指针
    uint8_t semantic,               // 列表语义 (fbListSemantics_t 值)
    uint16_t tmplID,                // **用于编码列表数据的 Template ID**
    const fbTemplate_t *tmpl,       // **用于编码列表数据的 Template 指针**
    uint16_t numElements            // 列表中的元素数量
)
```

**重点**: 
- `tmplID`: 这是用于 **编码/导出** 数据的 Template ID
- `tmpl`: 这是对应 Template 的指针
- 这两个参数必须从 **internal** (内部) template 获取，而不是 external template

### 2. 正确的使用方式

#### Export (导出) 场景
```c
// 1. 注册 internal 和 external templates
fbTemplate_t *internal_tmpl = fbTemplateAlloc(model);
fbTemplateAppendSpecArray(internal_tmpl, spec, flags, &err);
fbSessionAddTemplate(session, TRUE,  901, internal_tmpl, NULL, &err);  // internal
fbSessionAddTemplate(session, FALSE, 901, internal_tmpl, NULL, &err);  // external

// 2. 导出所有 templates 到输出流
fbSessionExportTemplates(session, &err);
fBufEmit(fbuf, &err);  // 确保 templates 写入文件

// 3. 获取 **internal** template
const fbTemplate_t *tmpl = fbSessionGetTemplate(session, TRUE, 901, &err);
                                                        ^^^^
                                                        必须是 TRUE (internal)

// 4. 初始化 SubTemplateList
fbSubTemplateListInit(&record->matchedContentList,
                     FB_LIST_SEM_ALL_OF,
                     901,    // Template ID
                     tmpl,   // Internal template 指针
                     1);     // 元素数量
```

#### Collection (收集) 场景
读取数据时，使用 `fbSubTemplateListCollectorInit()` 而不是 `fbSubTemplateListInit()`。

### 3. Template 注册的正确顺序
```c
// 对于每个 SubTemplate:
// 1. 创建 internal template
fbTemplate_t *tmpl = fbTemplateAlloc(model);
fbTemplateAppendSpecArray(tmpl, spec, 0xffffffff, &err);

// 2. 同时注册为 internal 和 external
fbSessionAddTemplate(session, TRUE,  tid, tmpl, NULL, &err);  // internal
fbSessionAddTemplate(session, FALSE, tid, tmpl, NULL, &err);  // external

// 对于主 Template (包含 SubTemplateList):
// 1. 创建 template
fbTemplate_t *main_tmpl = fbTemplateAlloc(model);
fbTemplateAppendSpecArray(main_tmpl, main_spec, 0xffffffff, &err);

// 2. 同时注册为 internal 和 external
fbSessionAddTemplate(session, TRUE,  400, main_tmpl, NULL, &err);  // internal
fbSessionAddTemplate(session, FALSE, 400, main_tmpl, NULL, &err);  // external

// 3. **关键**: 必须先导出 templates，再导出数据
fbSessionExportTemplates(session, &err);
fBufEmit(fbuf, &err);  // 确保写入

// 4. 然后才能使用 fBufAppend 导出数据记录
```

### 4. C 结构体的 padding 处理

文档明确说明：
> The layout of the template usually matches the C struct that holds the record. 
> **Any gaps (due to alignment) in the C struct must be noted in both the data structure and the template.**

示例：
```c
struct collectRecord_st {
    uint64_t      flowStartMilliseconds;
    uint64_t      flowEndMilliseconds;
    uint32_t      sourceIPv4Address;
    uint32_t      destinationIPv4Address;
    uint16_t      sourceTransportPort;
    uint16_t      destinationTransportPort;
    uint8_t       protocolIdentifier;
    uint8_t       padding[3];  // ⚠️ 显式声明 padding
    // ...
};

fbInfoElementSpec_t collectTemplate[] = {
    {"flowStartMilliseconds",       8, 0 },
    {"flowEndMilliseconds",         8, 0 },
    {"sourceIPv4Address",           4, 0 },
    {"destinationIPv4Address",      4, 0 },
    {"sourceTransportPort",         2, 0 },
    {"destinationTransportPort",    2, 0 },
    {"protocolIdentifier",          1, 0 },
    {"paddingOctets",               3, 0 },  // ⚠️ Template 中也要包含 padding
    // ...
};
```

**避免导出 padding**: 可以在 `fbSessionAddTemplate()` 时使用 `wantedFlags` 参数来排除 padding 字段。

### 5. 常见错误

#### 错误 1: 使用 external template 初始化 SubTemplateList
```c
// ❌ 错误
const fbTemplate_t *tmpl = fbSessionGetTemplate(session, FALSE, 901, &err);
fbSubTemplateListInit(&stl, ..., 901, tmpl, 1);

// ✅ 正确
const fbTemplate_t *tmpl = fbSessionGetTemplate(session, TRUE, 901, &err);
fbSubTemplateListInit(&stl, ..., 901, tmpl, 1);
```

#### 错误 2: 在导出 templates 之前尝试导出数据
```c
// ❌ 错误顺序
fBufAppend(fbuf, &record, sizeof(record), &err);  // 先导出数据
fbSessionExportTemplates(session, &err);          // 再导出 templates

// ✅ 正确顺序
fbSessionExportTemplates(session, &err);          // 先导出 templates
fBufEmit(fbuf, &err);                             // 确保写入
fBufAppend(fbuf, &record, sizeof(record), &err);  // 再导出数据
```

#### 错误 3: 忘记 fBufEmit
```c
// ❌ 可能导致 templates 未实际写入
fbSessionExportTemplates(session, &err);
// 没有 fBufEmit

// ✅ 正确
fbSessionExportTemplates(session, &err);
fBufEmit(fbuf, &err);  // 确保 templates 实际写入
```

## 解决方案总结

1. **所有 SubTemplate 必须同时注册为 internal 和 external**
2. **fbSubTemplateListInit 必须使用 internal template** (TRUE 参数)
3. **先导出 templates (fbSessionExportTemplates + fBufEmit)，再导出数据 (fBufAppend)**
4. **结构体 padding 必须在 template 中声明或使用 flags 排除**
5. **Template 字段顺序可以与结构体成员顺序不同，libfixbuf 会自动映射**

## 测试验证

修复后应该能够：
1. ipfixDump 显示所有 templates（records: 400/410/420/430/440；STL: 900-903）✅
2. ipfixDump 显示 Data Records ✅
3. 程序不再报 "Missing external template" 错误 ✅

---
最后更新: 2025-12-24

---

## 来源：c-implementation/README.md（原文节选：savMatchedContentList 语义 / padding / checklist）


### 3. 验证 IPFIX 文件格式

```bash
ipfixDump --in test_sav_e2e.ipfix --rfc5610
```

## 🔧 核心功能

### SAV Information Elements (Enterprise ID: 6871)

| IE ID | 名称 | 类型 | 长度 | 说明 |
|-------|------|------|------|------|
| 1 | savRuleType | uint8 | 1 | SAV规则类型 (0=allowlist, 1=blocklist) |
| 2 | savTargetType | uint8 | 1 | 目标类型 (0=interface, 1=prefix) |
| 3 | savMatchedContentList | SubTemplateList | var | 匹配内容列表 |
| 4 | savPolicyAction | uint8 | 1 | 策略动作 (0=permit, 1=discard, ...) |

### Template IDs

| ID | 名称 | 说明 |
|----|------|------|
| 400 | T1 (record) | Rule-Outcome 视角（最细） |
| 410 | T2 IPv4 (record) | Interface 视角（包含 sourceIPv4Address） |
| 420 | T2 IPv6 (record) | Interface 视角（包含 sourceIPv6Address） |
| 430 | T3 IPv4 (record) | Prefix / Mode 视角（IPv4 前缀聚合） |
| 440 | T3 IPv6 (record) | Prefix / Mode 视角（IPv6 前缀聚合） |
| 900 | savIPv4InterfacePrefix (subTemplate) | IPv4：接口→前缀映射 |
| 901 | savIPv6InterfacePrefix (subTemplate) | IPv6：接口→前缀映射 |
| 902 | savIPv4PrefixInterface (subTemplate) | IPv4：前缀→接口映射 |
| 903 | savIPv6PrefixInterface (subTemplate) | IPv6：前缀→接口映射 |

## 📊 关键特性

### 1. RFC6313 SubTemplateList 支持

正确实现 SubTemplateList 语义值：
- **Allowlist (savRuleType=0):** `allOf` (0x03)
- **Blocklist (savRuleType=1):** `exactlyOneOf` (0x01)

并强制执行（新架构）：

- allowlist：list length ≥ 1
- blocklist：list length = 1
- 同一条 Data Record 的 subTemplateID 不混用

### 2. 结构体对齐处理

解决了 C 结构体 padding 问题：
```c
typedef struct sav_t1_record_st {
    uint64_t flowStartMilliseconds;
    uint64_t flowEndMilliseconds;
    uint64_t packetDeltaCount;
    uint64_t octetDeltaCount;
    uint32_t ingressInterface;
    uint8_t  savRuleType;
    uint8_t  savTargetType;
    uint8_t  savPolicyAction;
    uint8_t  _padding[9];
    fbSubTemplateList_t savMatchedContentList;  // Must be last!
} sav_t1_record_t;
```

## 📝 开发者注意事项

### 结构体定义顺序

⚠️ **SubTemplateList 必须是结构体的最后一个字段！**

```c
// ✅ 正确
typedef struct {
    uint64_t timestamp;
    uint8_t  field1;
    uint8_t  _padding[7];
    fbSubTemplateList_t list;  // ← 最后
} correct_t;

// ❌ 错误
typedef struct {
    fbSubTemplateList_t list;  // ← 不能在前面
    uint64_t timestamp;
} wrong_t;
```

### Template 定义匹配

模板定义必须与 C 结构体完全匹配（包括 padding）：

```c
fbInfoElementSpec_t spec[] = {
    {"flowStartMilliseconds",       8, 0},
    {"flowEndMilliseconds",         8, 0},
    {"packetDeltaCount",            8, 0},
    {"octetDeltaCount",             8, 0},
    {"ingressInterface",            4, 0},
    {"savRuleType",                 1, 0},
    {"savTargetType",               1, 0},
    {"savPolicyAction",             1, 0},
    {"paddingOctets",               9, 0},  // ← 必须显式定义
    {"subTemplateList", FB_IE_VARLEN, 0},
    FB_IESPEC_NULL
};
```

---


- [ ] **支持至少 100 个并发 SCTP 连接** ← 新增
```
