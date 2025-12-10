# C Hackathon SAV IPFIX Project

## 项目概述

基于 C 的 SAV IPFIX Hackathon 实验项目，实现 SAV 数据包 record 的解析和 Web 可视化展示。

**核心特性**：
- ✅ 动态 IPv4/IPv6 支持
- ✅ 五元组信息显示（源IP、目的IP、源端口、目的端口、协议）
- ✅ SubTemplateList 解析（allowlist/blocklist/prefix/aspath）
- ✅ Web 可视化界面

## 项目结构

```
c-hackathon-sav/
├── src/
│   ├── generate_test_data.c       # (计划) 生成包含五元组的测试数据
│   ├── process_ipfix_json.py      # 后处理 ipfix2json 输出
│   └── ipfix_to_json.py           # (废弃) 早期解析脚本
├── test_data/
│   └── sample_sav.ipfix           # 测试数据（来自 c-implementation）
├── web/
│   ├── data_raw.json              # ipfix2json 原始输出
│   ├── data.json                  # 最终 Web 数据
│   ├── index.html                 # (待开发) Web 前端
│   └── app.js                     # (待开发) 前端逻辑
├── Makefile                       # 构建配置
├── run.sh                         # (待开发) 一键启动脚本
└── README.md                      # 本文件
```

## 关键工具使用

### 1. ipfix2json 工具

**官方文档**: https://tools.netsa.cert.org/fixbuf/ipfix2json.html

**工具位置**: `/usr/local/bin/ipfix2json` (libfixbuf 3.0.0.alpha2 自带)

**基本用法**:
```bash
ipfix2json --in <input.ipfix> --out <output.json>
```

**常用选项**:
- `-i, --in=path`: 输入 IPFIX 文件
- `-o, --out=path`: 输出 JSON 文件
- `-t, --templates`: 仅打印模板记录
- `-d, --data`: 仅打印数据记录
- `--octet-format=format`: octetArray 值格式 [base64/string/hexadecimal/empty]
- `--full-structure`: 禁用中间模板结构压缩
- `--allow-duplicates`: 允许重复键

**输出格式**:
- JSON Lines 格式（每行一个 JSON 对象）
- 模板记录: `{"template_record:0xTID()": [field_names]}`
- 数据记录: `{"template:0xTID()": {field_name: value, ...}}`

**示例输出**:
```json
{"template_record:0x0258()":["ingressInterface","sourceIPv4Prefix","sourceIPv4PrefixLength"]}
{"template:0x02bc()": {"observationTimeMilliseconds": "2025-12-10 12:28:38.000Z","_alienInformationElement": "AA==","subTemplateList": [...]}}
```

### 2. 后处理脚本

**脚本**: `src/process_ipfix_json.py`

**功能**:
1. 修复 IP 地址字节序（`0.2.0.192` → `192.0.2.0`）
2. 解码 base64 编码的 `_alienInformationElement` 为 SAV 字段
3. 修复接口 ID 字节序
4. 转换为 Web 友好的 JSON 格式

**用法**:
```bash
python3 src/process_ipfix_json.py <input.json> <output.json>
```

**示例**:
```bash
# 完整流程
ipfix2json --in test_data/sample_sav.ipfix --out web/data_raw.json
python3 src/process_ipfix_json.py web/data_raw.json web/data.json
```

**输出格式**:
```json
{
  "totalRecords": 3,
  "ipVersion": 4,
  "generatedAt": 1765370191,
  "records": [
    {
      "recordId": 1,
      "timestamp": 1765369718000,
      "ruleType": 0,
      "ruleTypeName": "allowlist",
      "targetType": 0,
      "policyAction": 1,
      "rules": [
        {
          "interfaceId": 10,
          "sourcePrefix": "192.0.2.0",
          "prefixLength": 24
        }
      ]
    }
  ]
}
```

## 快速开始

### 1. 生成测试数据

```bash
# 使用 c-implementation 的测试程序生成数据
cd ../c-implementation
make tests
./build/bin/test_test_sav_e2e
cp test_sav_e2e.ipfix ../c-hackathon-sav/test_data/sample_sav.ipfix
```

### 2. 转换为 JSON

```bash
cd c-hackathon-sav

# 方法1: 使用 ipfix2json + 后处理脚本
ipfix2json --in test_data/sample_sav.ipfix --out web/data_raw.json
python3 src/process_ipfix_json.py web/data_raw.json web/data.json

# 方法2: 查看原始 IPFIX 内容
ipfixDump --in test_data/sample_sav.ipfix
```

### 3. 启动 Web 服务（待实现）

```bash
# 启动简单 HTTP 服务器
python3 -m http.server 8000 --directory web

# 访问 http://localhost:8000
```

## 数据结构

### SAV Record 字段

**通用字段**:
- `timestamp`: 观测时间（毫秒时间戳）
- `ruleType`: 规则类型（0=allowlist, 1=blocklist, 2=prefix, 3=aspath）
- `targetType`: 目标类型
- `policyAction`: 策略动作

**五元组** (计划支持):
- `sourceIP`: 源 IP 地址（IPv4/IPv6）
- `destinationIP`: 目的 IP 地址（IPv4/IPv6）
- `sourcePort`: 源端口
- `destinationPort`: 目的端口
- `protocol`: 协议号

**SubTemplateList 规则**:
- `interfaceId`: 接口 ID
- `sourcePrefix`: 源 IP 前缀
- `prefixLength`: 前缀长度

### IPFIX 模板

**主模板 (TID=700)**:
- observationTimeMilliseconds (323)
- _alienInformationElement (6871/1) - ruleType
- _alienInformationElement (6871/2) - targetType  
- _alienInformationElement (6871/4) - policyAction
- subTemplateList (292)

**子模板 (TID=600)**:
- ingressInterface (10)
- sourceIPv4Prefix (44)
- sourceIPv4PrefixLength (9)

## 已知问题

### 1. Alien Information Element 未解码

**现象**: `_alienInformationElement` 显示为 base64 编码（如 "AA=="）

**原因**: ipfix2json 不识别企业私有 IE (6871/xxx)，将其视为 alien IE

**解决方案**: 
- 选项1: 使用 `--element-file` 加载自定义 IE XML 定义
- 选项2: 后处理脚本解码 base64 值
- **当前使用**: 选项2

### 2. IP 地址字节序

**现象**: IP 显示为反序（`0.2.0.192` 而非 `192.0.2.0`）

**原因**: libfixbuf 内部字节序与显示格式不一致

**解决方案**: 后处理脚本反转字节序

### 3. 接口 ID 显示

**现象**: 接口 ID 显示为大数值（如 167772160 而非 10）

**原因**: uint16 被解释为 uint32 网络字节序

**解决方案**: 提取高字节 `(value >> 24) & 0xFF`

## 技术细节

### ipfix2json 输出特点

1. **JSON Lines 格式**: 每行一个 JSON 对象，便于流式处理
2. **模板优先**: 先输出模板定义，再输出数据记录
3. **嵌套结构**: SubTemplateList 以嵌套数组形式存在
4. **base64 编码**: octetArray 默认使用 base64 编码
5. **时间格式**: ISO 8601 格式（`YYYY-MM-DD HH:MM:SS.sssZ`）

### 字节序问题处理

**IP 地址**:
```python
def fix_ip_address(ip_str):
    parts = ip_str.split('.')
    return '.'.join(reversed(parts))
```

**接口 ID**:
```python
def fix_interface_id(interface_val):
    return (interface_val >> 24) & 0xFF
```

### Alien IE 解码

```python
def decode_alien_ie(base64_str):
    data = base64.b64decode(base64_str)
    if len(data) == 1:
        return struct.unpack('B', data)[0]
    return None
```

## 下一步计划

- [ ] 实现 Web 可视化前端（index.html + app.js）
- [ ] 添加五元组支持（需修改 c-implementation 测试数据生成）
- [ ] 支持 IPv6 数据包和规则
- [ ] 实现动态 IP 版本切换显示
- [ ] 添加搜索和过滤功能
- [ ] 生成实验报告

## 参考资料

- **ipfix2json 文档**: https://tools.netsa.cert.org/fixbuf/ipfix2json.html
- **libfixbuf 文档**: https://tools.netsa.cert.org/fixbuf/
- **RFC 7011**: IPFIX Protocol Specification
- **RFC 6313**: Export of Structured Data in IPFIX
- **draft-cao-opsawg-ipfix-sav-01**: SAV IPFIX 规范

## 许可证

本项目遵循与主项目相同的许可证。
