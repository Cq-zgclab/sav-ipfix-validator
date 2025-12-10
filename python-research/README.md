# pyfixbuf Research - SubTemplateList 研究

## 项目简介

本项目专门研究 **pyfixbuf**（Python IPFIX 库）对 SubTemplateList 的实现方式，为 SAV IPFIX 的 Python 实现提供技术参考。

## 研究目标

1. **SubTemplateList 处理机制**
   - pyfixbuf 如何定义和使用 SubTemplateList
   - 数据结构和 API 设计
   - 与 libfixbuf (C) 的对比

2. **SubTemplateMultiList 高级用法**
   - 嵌套多个不同模板
   - 动态模板识别
   - 迭代和访问机制

3. **SAV 应用场景**
   - 如何用 SubTemplateList 导出 SAV 规则列表
   - 接口-前缀映射的最佳实践

## pyfixbuf 基本信息

### 官方资源
- **主页**: https://tools.netsa.cert.org/pyfixbuf/index.html
- **文档**: https://tools.netsa.cert.org/pyfixbuf/doc/intro.html
- **下载**: https://tools.netsa.cert.org/pyfixbuf/download.html
- **版本**: 0.9.0
- **依赖**: libfixbuf (底层 C 库)

### 核心特性
- Python 3.x 支持
- 完整的 IPFIX 协议支持（RFC 7011）
- SubTemplateList (RFC 6313) 原生支持
- SubTemplateMultiList 支持
- 文件和网络传输

## SubTemplateList 核心概念

### 1. 基本架构

```python
# pyfixbuf 的 SubTemplateList 工作流程

# 步骤1：定义主模板（包含 STL 字段）
main_template = pyfixbuf.Template(infomodel)
main_template.add_spec_list([
    pyfixbuf.InfoElementSpec("sourceIPv4Address"),
    pyfixbuf.InfoElementSpec("destinationIPv4Address"),
    pyfixbuf.InfoElementSpec("subTemplateMultiList")  # STL 字段
])

# 步骤2：定义子模板（STL 内部的结构）
sub_template = pyfixbuf.Template(infomodel)
sub_template.add_spec_list([
    pyfixbuf.InfoElementSpec("tcpSequenceNumber"),
    pyfixbuf.InfoElementSpec("initialTCPFlags")
])

# 步骤3：创建 Record 对象
main_record = pyfixbuf.Record(infomodel, main_template)
sub_record = pyfixbuf.Record(infomodel)
sub_record.add_element_list(["tcpSequenceNumber", "initialTCPFlags"])
```

### 2. 关键发现

#### SubTemplateMultiList 的访问方式

```python
# 读取包含 STML 的记录
for data in buffer:
    # 获取 STML 字段
    stml = data["subTemplateMultiList"]
    
    # 迭代 STML 中的每个 entry
    for entry in stml:
        # 检查 entry 包含哪些字段
        if "tcpSequenceNumber" in entry:
            # 设置对应的 Record 结构
            entry.set_record(tcp_record)
            
            # 迭代 entry 中的所有记录
            for tcp_data in entry:
                print(tcp_data["tcpSequenceNumber"])
```

#### 关键 API 总结

| API | 用途 | 说明 |
|-----|------|------|
| `data["fieldName"]` | 访问字段 | 字典式访问 |
| `for entry in stml:` | 迭代 STML | 每个 entry 是一个子模板实例 |
| `"fieldName" in entry` | 检查字段 | 识别子模板类型 |
| `entry.set_record(rec)` | 设置结构 | 告诉 pyfixbuf 如何解析 entry |
| `for item in entry:` | 迭代记录 | 访问子模板中的所有记录 |

### 3. 与 libfixbuf (C) 的对比

| 特性 | libfixbuf (C) | pyfixbuf (Python) |
|------|---------------|-------------------|
| STL 定义 | 手动构建 `fbSubTemplateList_t` | 自动处理，声明字段即可 |
| 模板查找 | 需要 `fbSessionAddTemplate()` | 自动管理模板 |
| 数据访问 | 指针操作 | 字典和迭代器 |
| 类型检查 | 手动比较 template ID | Python `in` 操作符 |
| 内存管理 | 手动 `fbSubTemplateListInit/Clear` | 自动垃圾回收 |

**结论**: pyfixbuf 对 SubTemplateList 的封装比 C 版本简洁很多！

## SAV IPFIX 应用设计

### savMatchedContentList 的实现方案

基于 pyfixbuf 的特性，SAV 的 `savMatchedContentList` 可以这样实现：

```python
import pyfixbuf

# 1. 定义信息模型
infomodel = pyfixbuf.InfoModel()

# 添加 SAV 扩展 IEs（私有企业号）
infomodel.add_element_list([
    # savRuleType, savTargetType, savPolicyAction 等...
    pyfixbuf.InfoElement("savRuleType", 1, 1, 12345, "uint8"),
    pyfixbuf.InfoElement("savTargetType", 2, 1, 12345, "uint8"),
    pyfixbuf.InfoElement("savPolicyAction", 3, 1, 12345, "uint8"),
    # savMatchedContentList - SubTemplateList 类型
    pyfixbuf.InfoElement("savMatchedContentList", 4, 292, 12345, "sub_tmpl_list")
])

# 2. 定义主模板（SAV 验证记录）
sav_template = pyfixbuf.Template(infomodel)
sav_template.add_spec_list([
    pyfixbuf.InfoElementSpec("flowStartMilliseconds"),
    pyfixbuf.InfoElementSpec("sourceIPv4Address"),
    pyfixbuf.InfoElementSpec("savRuleType"),
    pyfixbuf.InfoElementSpec("savTargetType"),
    pyfixbuf.InfoElementSpec("savPolicyAction"),
    pyfixbuf.InfoElementSpec("savMatchedContentList")  # SubTemplateList
])

# 3. 定义子模板（接口-前缀映射）
mapping_record = pyfixbuf.Record(infomodel)
mapping_record.add_element_list([
    "ingressInterface",      # uint32
    "sourceIPv4Prefix",      # ipv4Address
    "sourceIPv4PrefixLength" # uint8
])

# 4. 导出数据
exporter = pyfixbuf.Exporter()
exporter.init_file("sav_data.ipfix")

session = pyfixbuf.Session(infomodel)
session.add_internal_template(sav_template, 256)

buffer = pyfixbuf.Buffer(sav_template)
buffer.init_export(session, exporter)

# 创建主记录
sav_record = pyfixbuf.Record(infomodel, sav_template)

# 填充数据
sav_record["flowStartMilliseconds"] = int(time.time() * 1000)
sav_record["sourceIPv4Address"] = "192.0.2.1"
sav_record["savRuleType"] = 0  # Allowlist
sav_record["savTargetType"] = 0  # Interface-based
sav_record["savPolicyAction"] = 1  # Discard

# 填充 SubTemplateList
stl = sav_record["savMatchedContentList"]
stl.init(mapping_record)  # 设置子模板结构

# 添加多个映射规则
for interface, prefix, length in [
    (1000, "10.1.1.0", 24),
    (1000, "10.2.2.0", 24),
    (1000, "10.3.3.0", 24)
]:
    mapping = stl.add_record()
    mapping["ingressInterface"] = interface
    mapping["sourceIPv4Prefix"] = prefix
    mapping["sourceIPv4PrefixLength"] = length

# 导出记录
buffer.append(sav_record)
buffer.emit()

# 5. 读取和解析
collector = pyfixbuf.Collector()
collector.init_file("sav_data.ipfix")

read_session = pyfixbuf.Session(infomodel)
read_session.add_internal_template(sav_template, 256)

read_buffer = pyfixbuf.Buffer(sav_template)
read_buffer.init_collection(read_session, collector)

for record in read_buffer:
    print(f"Rule Type: {record['savRuleType']}")
    print(f"Target Type: {record['savTargetType']}")
    print(f"Action: {record['savPolicyAction']}")
    
    # 访问 SubTemplateList
    stl = record["savMatchedContentList"]
    print(f"Matched Rules: {len(stl)}")
    
    for entry in stl:
        entry.set_record(mapping_record)
        for mapping in entry:
            print(f"  Interface {mapping['ingressInterface']}: "
                  f"{mapping['sourceIPv4Prefix']}/{mapping['sourceIPv4PrefixLength']}")
```

## 关键优势

### 1. 简洁性
- **Go 手动实现**: 需要 400+ 行处理 SubTemplateList 编解码
- **libfixbuf (C)**: 需要手动管理模板查找和内存
- **pyfixbuf**: ~50 行代码完成相同功能

### 2. 动态性
```python
# pyfixbuf 支持运行时动态检查子模板类型
for entry in stml:
    if "tcpSequenceNumber" in entry:
        # 这是 TCP 子模板
        handle_tcp(entry)
    elif "httpRequestMethod" in entry:
        # 这是 HTTP 子模板
        handle_http(entry)
```

### 3. Python 生态集成
- 直接用 `dict` 访问字段
- 用 `for...in` 迭代
- 与 pandas、numpy 等数据分析库无缝集成

## 研究结论

### SubTemplateList 最佳实践（基于 pyfixbuf）

1. **定义清晰的子模板结构**
   - 使用独立的 `Record` 对象定义子模板
   - 避免在主模板中混杂子模板字段

2. **利用 Python 的动态特性**
   - 用 `in` 操作符检查字段存在
   - 用 `for` 循环迭代记录和条目

3. **处理多类型 SubTemplateMultiList**
   - 为每种可能的子模板创建对应的 `Record`
   - 根据字段特征动态识别类型
   - 用 `set_record()` 告诉库如何解析

4. **SAV 场景优化**
   - `savMatchedContentList` 只包含一种子模板（接口-前缀映射）
   - 可以简化为单一 `Record` 结构
   - 避免 SubTemplateMultiList 的复杂性

## 下一步工作

1. **安装 pyfixbuf**
   ```bash
   # 需要先安装 libfixbuf
   # 然后安装 pyfixbuf
   pip install pyfixbuf
   ```

2. **实现 SAV IPFIX Python 版本**
   - 基于 pyfixbuf 实现 SAV 导出器
   - 实现 SAV 采集器
   - 与 Go 版本对比性能和复杂度

3. **验证互操作性**
   - Go 导出 → Python 采集
   - Python 导出 → Go 采集
   - 确保符合 RFC 7011/6313 标准

## 参考资料

### pyfixbuf 官方文档
- [pyfixbuf 主页](https://tools.netsa.cert.org/pyfixbuf/index.html)
- [API 文档](https://tools.netsa.cert.org/pyfixbuf/doc/index.html)
- [介绍和示例](https://tools.netsa.cert.org/pyfixbuf/doc/intro.html)

### IPFIX 标准
- [RFC 7011 - IPFIX Protocol](https://tools.ietf.org/html/rfc7011)
- [RFC 6313 - SubTemplateList](https://tools.ietf.org/html/rfc6313)

### 相关项目
- [libfixbuf (C 实现)](https://tools.netsa.cert.org/fixbuf/)
- [go-ipfix (Go 实现)](https://github.com/vmware/go-ipfix)

---

**研究状态**: 文档完成，待实际代码实现验证
**创建时间**: 2025-12-09
