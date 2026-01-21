# ipfix2json 工具使用指南

## 工具概述

`ipfix2json` 是 libfixbuf 3.0.0 提供的命令行工具，用于将 IPFIX 二进制文件转换为 JSON 格式。

**官方文档**: https://tools.netsa.cert.org/fixbuf/ipfix2json.html

**工具路径**: `/usr/local/bin/ipfix2json`

## 命令行选项

### 基本选项

```bash
ipfix2json [OPTION...] -i <input.ipfix> -o <output.json>
```

| 选项 | 长格式 | 说明 |
|------|--------|------|
| `-h` | `--help` | 显示帮助信息 |
| `-i path` | `--in=path` | 指定输入 IPFIX 文件（默认: stdin `-`） |
| `-o path` | `--out=path` | 指定输出 JSON 文件（默认: stdout `-`） |
| `-V` | `--version` | 打印版本信息到 stderr |

### 内容过滤

| 选项 | 长格式 | 说明 |
|------|--------|------|
| `-t` | `--templates` | **仅**打印 IPFIX 模板记录 |
| `-d` | `--data` | **仅**打印 IPFIX 数据记录 |

### Information Element 定义

| 选项 | 长格式 | 说明 |
|------|--------|------|
| | `--rfc5610` | 从 element type options records 中读取 IE 定义 |
| `-e path` | `--element-file=path` | 从 XML 文件加载自定义 IE 定义（可重复使用） |
| `-c path` | `--cert-element-path=path` | 添加 CERT IE XML 文件搜索路径 |

### 输出格式控制

| 选项 | 长格式 | 说明 |
|------|--------|------|
| | `--full-structure` | 禁用中间模板结构压缩 |
| | `--allow-duplicates` | 允许对象中存在重复键（当元素或模板出现多次时） |
| | `--octet-format=format` | 设置 octetArray 值的输出格式（默认: `base64`） |

#### `--octet-format` 可选值

- `base64`: Base64 编码（默认）
- `string`: 字符串格式
- `hexadecimal`: 十六进制格式
- `empty`: 空值

## 输出格式

### JSON Lines 格式

ipfix2json 输出 **JSON Lines** 格式，每行一个完整的 JSON 对象：

```json
{"template_record:0x0258()":["field1","field2","field3"]}
{"template:0x0258()": {"field1": value1, "field2": value2}}
{"template:0x0258()": {"field1": value3, "field2": value4}}
```

### 模板记录

**格式**: `{"template_record:0xTID(scope_count)": [field_names]}`

**示例**:
```json
{"template_record:0x0258()": [
  "ingressInterface",
  "sourceIPv4Prefix",
  "sourceIPv4PrefixLength"
]}

{"template_record:0x02bc()": [
  "observationTimeMilliseconds",
  "_alienInformationElement",
  "_alienInformationElement",
  "_alienInformationElement",
  "subTemplateList"
]}
```

- `0xTID`: 模板 ID（十六进制）
- `(scope_count)`: Scope 字段数量（Options Template）
- `field_names`: Information Element 名称数组

### 数据记录

**格式**: `{"template:0xTID(scope_count)": {field_name: value, ...}}`

**示例**:
```json
{
  "template:0x02bc()": {
    "observationTimeMilliseconds": "2025-12-10 12:28:38.000Z",
    "_alienInformationElement": "AA==",
    "_alienInformationElement": "AA==",
    "_alienInformationElement": "AQ==",
    "template:0x0258()": [
      {
        "ingressInterface": 167772160,
        "sourceIPv4Prefix": "0.2.0.192",
        "sourceIPv4PrefixLength": 24
      }
    ]
  }
}
```

### 嵌套 SubTemplateList

SubTemplateList 字段以**嵌套模板对象数组**形式表示：

```json
{
  "template:0x02bc()": {
    "someField": "value",
    "template:0x0258()": [  ← SubTemplateList 作为嵌套数组
      {"subField1": val1, "subField2": val2},
      {"subField1": val3, "subField2": val4}
    ]
  }
}
```

## 数据类型映射

| IPFIX 类型 | JSON 类型 | 示例 |
|------------|----------|------|
| unsigned8/16/32/64 | number | `42` |
| signed8/16/32/64 | number | `-42` |
| float32/64 | number | `3.14` |
| boolean | boolean | `true` |
| macAddress | string | `"00:11:22:33:44:55"` |
| string | string | `"example"` |
| ipv4Address | string | `"192.0.2.1"` |
| ipv6Address | string | `"2001:db8::1"` |
| dateTimeSeconds | string (ISO 8601) | `"2025-12-10 12:28:38Z"` |
| dateTimeMilliseconds | string (ISO 8601) | `"2025-12-10 12:28:38.000Z"` |
| octetArray | string (base64) | `"AA=="` |
| basicList | array | `[val1, val2]` |
| subTemplateList | array of objects | `[{...}, {...}]` |
| subTemplateMultiList | object | `{"template:TID": [...]}` |

## 使用示例

### 1. 基本转换

```bash
ipfix2json --in input.ipfix --out output.json
```

### 2. 仅查看模板

```bash
ipfix2json --in input.ipfix --templates
```

**输出示例**:
```json
{"template_record:0x0100()":["sourceIPv4Address","destinationIPv4Address"]}
{"template_record:0x0200()":["flowStartMilliseconds","flowEndMilliseconds"]}
```

### 3. 仅查看数据（不含模板）

```bash
ipfix2json --in input.ipfix --data
```

### 4. 自定义 octetArray 格式

```bash
# 十六进制格式
ipfix2json --in input.ipfix --octet-format=hexadecimal

# 字符串格式（仅适用于可打印字符）
ipfix2json --in input.ipfix --octet-format=string

# 空值（忽略 octetArray 内容）
ipfix2json --in input.ipfix --octet-format=empty
```

### 5. 加载自定义 IE 定义

```bash
ipfix2json --in input.ipfix \
           --element-file custom_ies.xml \
           --out output.json
```

**custom_ies.xml 格式**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<fieldDefinitions>
  <field name="customField" 
         dataType="unsigned32" 
         group="" 
         elementId="1001" 
         applicability="data" 
         status="current" 
         description="Custom field description">
    <enterprise enterpriseId="9999"/>
  </field>
</fieldDefinitions>
```

### 6. 管道处理

```bash
# 从 stdin 读取，输出到 stdout
cat input.ipfix | ipfix2json | jq '.'

# 链式处理
ipfixDump --in data.ipfix | grep "template" | ipfix2json --templates
```

### 7. 过滤和查询（配合 jq）

```bash
# 提取所有模板 ID
ipfix2json --in input.ipfix --templates | \
  jq -r 'keys[] | match("0x([0-9a-f]+)").captures[0].string'

# 统计数据记录数
ipfix2json --in input.ipfix --data | wc -l

# 提取特定字段
ipfix2json --in input.ipfix --data | \
  jq -r '.["template:0x0100()"] | .sourceIPv4Address'
```

## SAV IPFIX 特殊处理

### Alien Information Element

**问题**: 企业私有 IE (6871/xxx) 不被识别，显示为 `_alienInformationElement`

**示例**:
```json
{
  "template:0x02bc()": {
    "_alienInformationElement": "AA==",  ← SAV ruleType (6871/1)
    "_alienInformationElement": "AA==",  ← SAV targetType (6871/2)
    "_alienInformationElement": "AQ=="   ← SAV policyAction (6871/4)
  }
}
```

**解决方案**:

#### 方案1: 提供自定义 IE XML 文件

```xml
<?xml version="1.0" encoding="UTF-8"?>
<fieldDefinitions>
  <field name="savRuleType" dataType="unsigned8" elementId="1">
    <enterprise enterpriseId="6871"/>
  </field>
  <field name="savTargetType" dataType="unsigned8" elementId="2">
    <enterprise enterpriseId="6871"/>
  </field>
  <field name="savPolicyAction" dataType="unsigned8" elementId="4">
    <enterprise enterpriseId="6871"/>
  </field>
</fieldDefinitions>
```

```bash
ipfix2json --in sav.ipfix \
           --element-file sav_ies.xml \
           --out sav.json
```

#### 方案2: 后处理脚本解码

```python
import base64
import struct

def decode_alien_ie(base64_str):
    data = base64.b64decode(base64_str)
    if len(data) == 1:
        return struct.unpack('B', data)[0]
    return None

# "AA==" → 0x00 → 0
# "AQ==" → 0x01 → 1
```

### IP 地址字节序

**问题**: IPv4 地址显示为反序

**示例**: `"0.2.0.192"` 实际应为 `"192.0.2.0"`

**原因**: libfixbuf 内部字节序处理

**解决方案**:
```python
def fix_ip_address(ip_str):
    parts = ip_str.split('.')
    return '.'.join(reversed(parts))
```

### 接口 ID 字节序

**问题**: uint16 接口 ID 显示为大数值

**示例**: `167772160` 实际应为 `10`

**解决方案**:
```python
def fix_interface_id(interface_val):
    # 提取最高字节
    return (interface_val >> 24) & 0xFF
```

## 性能考虑

### 大文件处理

对于大型 IPFIX 文件，ipfix2json 输出 JSON Lines 格式便于**流式处理**：

```bash
# 逐行处理，避免加载整个文件到内存
ipfix2json --in large.ipfix --data | while read line; do
  echo "$line" | jq '...'
done
```

### Python 流式解析

```python
import json

with open('output.json', 'r') as f:
    for line in f:
        if not line.strip():
            continue
        obj = json.loads(line)
        # 处理每条记录
        process_record(obj)
```

## 常见问题

### Q1: 为什么模板和数据混在一起？

**A**: ipfix2json 按照 IPFIX 文件顺序输出。使用 `--templates` 或 `--data` 选项分别查看。

### Q2: 如何处理重复字段名？

**A**: 默认情况下，重复字段会覆盖前值。使用 `--allow-duplicates` 允许重复（但这会导致无效 JSON）。

### Q3: octetArray 为什么是 base64？

**A**: 二进制数据无法直接表示为 JSON 字符串。使用 `--octet-format=hexadecimal` 改为十六进制。

### Q4: 如何提取嵌套 SubTemplateList？

**A**: 使用 jq 递归查询：
```bash
ipfix2json --in input.ipfix --data | jq '.["template:0x02bc()"]["template:0x0258()"][]'
```

## 参考资料

- **官方文档**: https://tools.netsa.cert.org/fixbuf/ipfix2json.html
- **libfixbuf 主页**: https://tools.netsa.cert.org/fixbuf/
- **RFC 7011**: IPFIX Protocol Specification
- **RFC 7012**: IPFIX Information Elements
- **RFC 6313**: Export of Structured Data in IPFIX
- **JSON Lines 格式**: http://jsonlines.org/

## 版本信息

本文档基于 libfixbuf 3.0.0.alpha2 版本编写。

最后更新: 2025-12-10
