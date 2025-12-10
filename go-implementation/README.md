# SAV IPFIX Validator - Go Implementation

## 概述

这是 **纯 Go 语言** 实现的 SAV IPFIX 验证器，不依赖 libfixbuf，手动实现 IPFIX 二进制编码和解码。

## 🎯 Hackathon Demo

本实现专为 **黑客松演示** 设计，展示完整的 IPFIX 数据流：

```
SAV 路由器 → IPFIX 导出器 → 二进制文件 → 采集器 → JSON → Web 仪表板
```

## 目录结构

```
go-implementation/
├── cmd/                          # 命令行工具
│   ├── exporter/                # IPFIX 导出器
│   │   └── main.go
│   ├── collector/               # IPFIX 采集器（控制台输出）
│   │   └── main.go
│   └── collector_json/          # IPFIX 采集器（JSON 输出）
│       └── main.go
├── pkg/                         # 核心库
│   └── sav/
│       ├── constants.go         # SAV IEs 定义
│       ├── writer.go            # IPFIX 二进制编码器
│       ├── reader.go            # IPFIX 二进制解码器
│       └── scenarios.go         # 黑客松场景数据
├── data/                        # IPFIX 数据文件
│   ├── scenario1.ipfix         # 场景1：攻击检测
│   ├── scenario2.ipfix         # 场景2：接口分布
│   ├── scenario3.ipfix         # 场景3：策略动作
│   └── all_scenarios.ipfix     # 所有场景
├── web/                         # Web 仪表板
│   ├── index.html              # 主仪表板（动态加载）
│   └── data.json               # IPFIX 转换的 JSON 数据
└── bin/                         # 编译输出
    ├── exporter
    ├── collector
    └── collector_json
```

## 快速开始

### 1. 编译所有工具

```bash
cd go-implementation

# 编译导出器
go build -o bin/exporter ./cmd/exporter

# 编译采集器（控制台）
go build -o bin/collector ./cmd/collector

# 编译采集器（JSON）
go build -o bin/collector_json ./cmd/collector_json
```

### 2. 生成 IPFIX 数据

```bash
# 生成所有场景（27 条记录）
./bin/exporter --scenario all --output data/all_scenarios.ipfix

# 或生成单个场景
./bin/exporter --scenario attack --output data/scenario1.ipfix
```

### 3. 查看 IPFIX 数据（控制台）

```bash
./bin/collector --input data/all_scenarios.ipfix
```

### 4. 转换为 JSON（用于 Web 仪表板）

```bash
./bin/collector_json --input data/all_scenarios.ipfix --output web/data.json
```

### 5. 启动 Web 仪表板

```bash
# 启动简单 HTTP 服务器
cd web
python3 -m http.server 8000

# 打开浏览器访问
# 本地: http://localhost:8000/index.html
# Codespace: https://xxx-8000.app.github.dev/web/index.html
```

## 🎭 黑客松场景

实现了 3 个完整场景，展示 **宏观统计 + 微观细节 + 结合分析**：

### 场景 1：攻击检测（13 条记录）

- **宏观**: 时间序列分析，攻击流量趋势
- **微观**: 每个 IPFIX 记录包含时间戳、规则类型、动作
- **数据**: 模拟 1 小时内伪造流量从 500 pps → 10000 pps → 800 pps

### 场景 2：接口分布（7 条记录）

- **宏观**: 空间分析，5 个接口的流量分布
- **微观**: SubTemplateList 包含接口-前缀映射
- **数据**: eth0 (45000 pps) 是主要攻击入口

### 场景 3：策略有效性（7 条记录）

- **宏观**: 策略动作分布统计
- **微观**: 每条记录的 savPolicyAction (Discard/RateLimit/Redirect)
- **数据**: Discard=4, RateLimit=2, Redirect=1

## SAV 信息元素（IEs）

完整实现 **draft-cao-opsawg-ipfix-sav-01** 规范：

| IE 名称 | PEN | ID | 类型 | 长度 | 语义 |
|---------|-----|----|----- |------|------|
| savRuleType | 50000 | 1 | uint8 | 1 | 0=Allowlist, 1=Blocklist |
| savTargetType | 50000 | 2 | uint8 | 1 | 0=InterfaceBased, 1=PrefixBased |
| savPolicyAction | 50000 | 3 | uint8 | 1 | 0=Permit, 1=Discard, 2=RateLimit, 3=Redirect |
| savMatchedContentList | 50000 | 4 | subTemplateList | var | 接口-前缀映射列表 |
| savInterfaceID | 50000 | 5 | uint32 | 4 | 接口 ID |
| savPrefix | 50000 | 6 | octetArray | var | IP 前缀（4/16 字节） |
| savPrefixLength | 50000 | 7 | uint8 | 1 | 前缀长度 |

## RFC 标准合规性

- ✅ **RFC 7011**: IPFIX Protocol Specification
  - Message Header (16 bytes)
  - Set Header (4 bytes)
  - Template Record / Data Record
  
- ✅ **RFC 6313**: Export of Structured Data in IPFIX
  - SubTemplateList (255 semantic)
  - Nested template structure
  
- ✅ **draft-cao-opsawg-ipfix-sav-01**: SAV Information Elements
  - Enterprise ID: 50000
  - 7 个 SAV-specific IEs

## 技术亮点

### 1. 纯 Go 实现，无需 libfixbuf

```go
// 手动编码 IPFIX 消息头
func (w *IPFIXWriter) writeMessageHeader() error {
    binary.Write(w.buf, binary.BigEndian, uint16(10))    // Version
    binary.Write(w.buf, binary.BigEndian, uint16(0))     // Length (占位)
    binary.Write(w.buf, binary.BigEndian, uint32(time.Now().Unix()))
    binary.Write(w.buf, binary.BigEndian, w.sequenceNumber)
    binary.Write(w.buf, binary.BigEndian, w.observationDomainID)
}
```

### 2. SubTemplateList 手动实现

```go
// 写入 SubTemplateList
func (w *IPFIXWriter) writeSubTemplateList(mappings []InterfacePrefixMapping) error {
    stlHeader := []byte{
        255,                              // Semantic (allOf)
        byte(contentTemplateID >> 8),     // Template ID (high byte)
        byte(contentTemplateID & 0xFF),   // Template ID (low byte)
    }
    w.buf.Write(stlHeader)
    
    // 写入每个映射
    for _, mapping := range mappings {
        w.writeContentRecord(mapping)
    }
}
```

### 3. JSON 转换器（Web 可视化）

```go
type JSONOutput struct {
    Metadata struct {
        InputFile     string    `json:"input_file"`
        ProcessedAt   time.Time `json:"processed_at"`
        TotalRecords  int       `json:"total_records"`
        RFCCompliance []string  `json:"rfc_compliance"`
    } `json:"metadata"`
    Records []JSONRecord `json:"records"`
}
```

## Web 仪表板特性

- 📊 **数据流程图**: 6 步可视化（路由器 → IPFIX → JSON → Charts）
- 📁 **数据来源证明**: 显示 `data/all_scenarios.ipfix` 和 RFC 合规性
- 📈 **3 个 Chart.js 图表**: 
  - 攻击时间线（折线图）
  - 接口分布（柱状图）
  - 策略动作（饼图）
- 💡 **IPFIX 技术细节**: 每个场景解释 IEs 和 SubTemplateList

## 开发路线图

详见 `../GO_IMPLEMENTATION_TODO.md` - 9 阶段完整计划：

- ✅ Phase 1: 基础框架
- ✅ Phase 2: 模板系统
- ✅ Phase 3: SubTemplateList
- ✅ Phase 4: 数据记录
- ✅ Phase 5: 二进制读取
- ✅ Phase 6: 命令行工具
- ✅ Phase 7: 测试验证
- ✅ Phase 8: 黑客松场景
- ✅ Phase 9: Web 仪表板

## 相关文档

- `../HACKATHON_PLAN.md` - 黑客松完整计划
- `../REVIEW_REPORT.md` - SAV IEs 语义验证报告
- `../docs/STL_IMPLEMENTATION_COMPARISON.md` - Go vs C 实现对比

## 性能特点

- **快速编译**: 无需 C 依赖，`go build` 3 秒完成
- **跨平台**: Linux/macOS/Windows 零修改
- **内存安全**: Go GC 自动管理内存
- **易调试**: 纯 Go 代码，可直接 `fmt.Printf` 调试

## License

本实现遵循项目根目录的 LICENSE 文件。
