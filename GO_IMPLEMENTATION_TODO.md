# SAV IPFIX Validator - Go 实现完整 TODO 计划

## 📋 问题诊断：语义不符

### 🔴 发现的问题

当前 Go 实现的 SAV IE 语义与 draft-cao-opsawg-ipfix-sav-01 和 C 实现（libfixbuf）**不一致**！

#### 对比表：

| IE 名称 | 项 | Draft/C 实现 ✅ | 当前 Go 实现 ❌ | 状态 |
|---------|-----|----------------|----------------|------|
| **savRuleType** | 值域 | 0=allowlist, 1=blocklist | 1=ACL, 2=URPF, 3=BAP, 4=EFP | **错误** |
| | 含义 | 规则是允许列表还是拒绝列表 | 规则的技术类型 | **语义偏移** |
| **savTargetType** | 值域 | 0=interface-based, 1=prefix-based | 1=SingleInterface, 2=MultipleInterfaces, 3=Prefix | **错误** |
| | 含义 | 验证目标的类型（接口或前缀） | 目标的粒度 | **语义偏移** |
| **savPolicyAction** | 值域 | 0=permit, 1=discard, 2=rate-limit, 3=redirect | 1=Permit, 2=Deny | **不完整** |
| | 含义 | 验证失败后的动作 | 策略动作（缺少 rate-limit 和 redirect） | **缺失值** |

### 🎯 根本原因

Go 实现时**误解了 SAV 规范的语义**：
- 将 `savRuleType` 理解为"使用什么 SAV 技术"（ACL/URPF/BAP）
- 实际应该是"规则是 allowlist 还是 blocklist"
- 将 `savTargetType` 理解为"目标的数量/粒度"
- 实际应该是"验证方向"（interface-based vs prefix-based）

### 📖 正确的语义理解

根据 draft-cao-opsawg-ipfix-sav-01：

#### savRuleType (IE 1)
- **含义**: SAV 规则的类型（allowlist 或 blocklist）
- **值域**:
  - `0` = Allowlist（允许列表）- 只有列表中的源地址被允许
  - `1` = Blocklist（拒绝列表）- 列表中的源地址被拒绝
- **用途**: 指示这条规则是白名单还是黑名单

#### savTargetType (IE 2)
- **含义**: SAV 验证的目标类型
- **值域**:
  - `0` = Interface-based（基于接口）- 在接口上验证源前缀
  - `1` = Prefix-based（基于前缀）- 对源前缀验证接收接口
- **用途**: 指示验证的方向

#### savMatchedContentList (IE 3)
- **含义**: 触发 SAV 验证的规则内容（SubTemplateList）
- **类型**: subTemplateList
- **内容**: 
  - 当 targetType=0: 包含 interface-to-prefix 映射（Template 901/902）
  - 当 targetType=1: 包含 prefix-to-interface 映射（Template 903/904）

#### savPolicyAction (IE 4)
- **含义**: 验证失败后采取的动作
- **值域**:
  - `0` = Permit（允许）- 放行流量
  - `1` = Discard（丢弃）- 丢弃流量
  - `2` = Rate-limit（限速）- 限速处理
  - `3` = Redirect（重定向）- 重定向到其他地方
- **用途**: 指示策略执行动作

### 🔄 四种 SAV 模式的映射

| 模式 | savRuleType | savTargetType | 含义 |
|------|-------------|---------------|------|
| Mode 1 | 0 (allowlist) | 0 (interface-based) | 接口上的前缀白名单 |
| Mode 2 | 1 (blocklist) | 0 (interface-based) | 接口上的前缀黑名单 |
| Mode 3 | 0 (allowlist) | 1 (prefix-based) | 前缀的接口白名单 |
| Mode 4 | 1 (blocklist) | 1 (prefix-based) | 前缀的接口黑名单 |

---

## 🎯 完整实现计划

### Phase 0: 准备和研究 ✅ (已完成)

#### 0.1 环境搭建 ✅
- [x] 安装 Go 1.16+
- [x] 初始化 Go module: `github.com/Cq-zgclab/sav-ipfix-validator`
- [x] 安装 go-ipfix: `github.com/zoomoid/go-ipfix v0.4.1`

#### 0.2 go-ipfix API 研究 ✅
- [x] 阅读 go-ipfix 文档和示例
- [x] 理解 Cache/Decoder 模式 vs Session 模式
- [x] 确认 SubTemplateList 支持情况
- [x] 决策：**使用纯二进制编解码**（不依赖 go-ipfix 高层 API）

#### 0.3 draft 规范对齐 ✅
- [x] 重读 draft-cao-opsawg-ipfix-sav-01
- [x] 对比 C 实现（libfixbuf）的 IE 定义
- [x] 识别语义差异（见上方对比表）

---

### Phase 1: 修正 SAV IE 定义 🔴 (当前任务)

#### 1.1 更新常量定义
**文件**: `pkg/sav/constants.go`

- [ ] **修正 savRuleType**:
  ```go
  const (
      RuleTypeAllowlist uint8 = 0  // 允许列表（白名单）
      RuleTypeBlocklist uint8 = 1  // 拒绝列表（黑名单）
  )
  ```

- [ ] **修正 savTargetType**:
  ```go
  const (
      TargetTypeInterfaceBased uint8 = 0  // 基于接口的验证
      TargetTypePrefixBased    uint8 = 1  // 基于前缀的验证
  )
  ```

- [ ] **修正 savPolicyAction**:
  ```go
  const (
      PolicyActionPermit     uint8 = 0  // 允许
      PolicyActionDiscard    uint8 = 1  // 丢弃
      PolicyActionRateLimit  uint8 = 2  // 限速
      PolicyActionRedirect   uint8 = 3  // 重定向
  )
  ```

- [ ] **更新辅助函数**: `RuleTypeName()`, `TargetTypeName()`, `PolicyActionName()`

#### 1.2 更新注释和文档
- [ ] 在 `constants.go` 中添加详细的语义说明
- [ ] 添加四种 SAV 模式的映射表注释
- [ ] 引用 draft-cao-opsawg-ipfix-sav-01 章节

---

### Phase 2: 修正模板定义 🟡 (需要检查)

#### 2.1 检查模板 ID
**文件**: `pkg/sav/constants.go`

当前模板 ID：
```go
const (
    TemplateIPv4InterfacePrefix = 901  // IPv4 Interface -> Prefix
    TemplateIPv6InterfacePrefix = 902  // IPv6 Interface -> Prefix
    TemplateIPv4PrefixInterface = 903  // IPv4 Prefix -> Interface
    TemplateIPv6PrefixInterface = 904  // IPv6 Prefix -> Interface
    TemplateMainDataRecord      = 400  // Main data record
)
```

- [ ] **验证**: 与 C 实现一致 ✅
- [ ] **验证**: 与 draft 定义一致 ✅

#### 2.2 检查模板字段顺序
**需要检查**: 确保字段顺序与 C 实现完全一致

C 实现的主模板顺序：
```c
{ "observationTimeMilliseconds", 8, 0 },  
{ "savRuleType",                 1, 0 },
{ "savTargetType",               1, 0 },
{ "subTemplateList", FB_IE_VARLEN, 0 },   // 注意：必须用 "subTemplateList"
{ "savPolicyAction",             1, 0 },
```

- [ ] **验证**: Go 实现的编码顺序一致
- [ ] **注意**: SubTemplateList 的字段名必须是 `"subTemplateList"`（标准 IE）

---

### Phase 3: 修正数据编码器 🟡 (需要更新)

#### 3.1 更新 writer.go
**文件**: `pkg/sav/writer.go`

- [ ] 修正示例数据中的常量值
  ```go
  // 错误示例（当前）:
  w.WriteIPv4Mapping(time.Now(), 1, 1, 1, ...)  // 1=ACL, 1=SingleInterface, 1=Permit
  
  // 正确示例:
  w.WriteIPv4Mapping(time.Now(), 0, 0, 0, ...)  // 0=Allowlist, 0=InterfaceBased, 0=Permit
  ```

- [ ] 更新函数签名注释，说明参数的正确语义
- [ ] 添加参数验证（检查值域是否合法）

#### 3.2 创建语义验证辅助函数
```go
func ValidateRuleType(ruleType uint8) error {
    if ruleType > 1 {
        return fmt.Errorf("invalid savRuleType: %d (must be 0 or 1)", ruleType)
    }
    return nil
}

func ValidateTargetType(targetType uint8) error {
    if targetType > 1 {
        return fmt.Errorf("invalid savTargetType: %d (must be 0 or 1)", targetType)
    }
    return nil
}

func ValidatePolicyAction(action uint8) error {
    if action > 3 {
        return fmt.Errorf("invalid savPolicyAction: %d (must be 0-3)", action)
    }
    return nil
}
```

---

### Phase 4: 修正数据解码器 🟡 (需要更新)

#### 4.1 更新 reader.go
**文件**: `pkg/sav/reader.go`

- [ ] 更新输出格式，使用正确的语义名称
  ```go
  // 错误输出（当前）:
  fmt.Printf("  Rule Type: %d (ACL)\n", record.RuleType)
  
  // 正确输出:
  fmt.Printf("  Rule Type: %d (%s)\n", record.RuleType, RuleTypeName(record.RuleType))
  // 输出: "Rule Type: 0 (Allowlist)" 或 "Rule Type: 1 (Blocklist)"
  ```

- [ ] 添加解码后的数据验证
- [ ] 添加警告信息（如果遇到非法值）

---

### Phase 5: 更新 CLI 工具 🟡 (需要更新)

#### 5.1 更新 exporter CLI
**文件**: `cmd/exporter/main.go`

- [ ] 修正示例数据的语义
  ```go
  // 示例 1: Interface-based allowlist
  w.WriteIPv4Mapping(
      timestamp,
      0,  // RuleType: Allowlist
      0,  // TargetType: Interface-based
      0,  // PolicyAction: Permit
      1,  // Interface ID
      net.ParseIP("192.0.2.0"),
      24,
  )
  
  // 示例 2: Prefix-based blocklist
  w.WriteIPv6Mapping(
      timestamp,
      1,  // RuleType: Blocklist
      1,  // TargetType: Prefix-based
      1,  // PolicyAction: Discard
      2,
      net.ParseIP("2001:db8::"),
      32,
  )
  ```

- [ ] 添加命令行参数支持：
  - `--rule-type`: 0 (allowlist) 或 1 (blocklist)
  - `--target-type`: 0 (interface-based) 或 1 (prefix-based)
  - `--policy-action`: 0-3 (permit/discard/rate-limit/redirect)

#### 5.2 更新 collector CLI
**文件**: `cmd/collector/main.go`

- [ ] 使用正确的语义名称显示数据
- [ ] 添加 `--verbose` 模式显示更多信息
- [ ] 添加 `--validate` 模式检查数据合法性

---

### Phase 6: 完善文档 📝 (需要重写)

#### 6.1 更新 README_GO.md
- [ ] 修正 IE 语义说明
- [ ] 更新示例代码
- [ ] 添加四种 SAV 模式的使用场景

#### 6.2 更新 QUICKSTART.md
- [ ] 修正快速开始示例
- [ ] 添加正确的输出示例

#### 6.3 创建 IE 语义对照表
- [ ] 创建 `docs/SAV_IE_SEMANTICS.md`
- [ ] 详细说明每个 IE 的含义和用法
- [ ] 提供使用场景示例

---

### Phase 7: 添加测试和验证 🧪 (P0 紧急)

#### 7.1 单元测试
**新建**: `pkg/sav/sav_test.go`

- [ ] 测试常量值的正确性
- [ ] 测试编码器生成的二进制格式
- [ ] 测试解码器解析的正确性
- [ ] 测试 encode-decode 循环

#### 7.2 数据验证器 ⭐ (必须在 Phase 7 后立即完成)
**新建**: `pkg/sav/validator.go`

- [ ] **语义验证**（值域检查）
  ```go
  // 验证 savRuleType: 必须是 0 或 1
  // 验证 savTargetType: 必须是 0 或 1  
  // 验证 savPolicyAction: 必须是 0-3
  ```

- [ ] **一致性验证**（字段关联检查）
  ```go
  // 检查: targetType=0 时，STL 必须包含 interface-to-prefix 映射
  // 检查: targetType=1 时，STL 必须包含 prefix-to-interface 映射
  // 检查: 模板 ID 与 targetType 的对应关系
  ```

- [ ] **格式验证**（IPFIX 消息结构）
  ```go
  // IPFIX 消息头验证（Version=10）
  // Set ID 验证（2=Template, 256+=Data）
  // 长度字段一致性验证
  // SubTemplateList 格式验证
  ```

- [ ] **创建验证 CLI 工具**
  ```bash
  ./bin/sav_validator input.ipfix
  # 输出：所有验证结果和警告
  ```

#### 7.3 兼容性测试
- [ ] 生成 IPFIX 文件，用 C 实现读取
- [ ] C 实现生成的文件，用 Go 实现读取
- [ ] 验证互操作性

#### 7.4 集成测试
**新建**: `test/integration_test.go`

- [ ] 测试四种 SAV 模式的完整流程
- [ ] 测试 IPv4 和 IPv6 数据
- [ ] 测试边界情况和错误处理

---

### Phase 8: 网络传输支持 🌐 (P0 紧急，RFC 7011 要求)

**重要**: RFC 7011 第 10 节规定 IPFIX 必须支持 SCTP，也可以支持 TCP/UDP

#### 8.1 SCTP Collector ⭐⭐⭐ (最高优先级)
**新建**: `pkg/sav/collector_sctp.go`

**为什么 SCTP 是必须的**:
- RFC 7011 强制要求：IPFIX over SCTP 是标准传输方式
- 优势：可靠传输 + 消息边界保持 + 多流支持
- 工业标准：所有符合 RFC 的实现都必须支持

**实现计划**:
```go
// 使用 Go 的 SCTP 库
import "github.com/ishidawataru/sctp"

type SCTPCollector struct {
    listener *sctp.SCTPListener
    handler  RecordHandler
}

func NewSCTPCollector(addr string, handler RecordHandler) (*SCTPCollector, error) {
    // 1. 创建 SCTP listener
    laddr, _ := sctp.ResolveSCTPAddr("sctp", addr)
    listener, err := sctp.ListenSCTP("sctp", laddr)
    
    return &SCTPCollector{
        listener: listener,
        handler:  handler,
    }, nil
}

func (c *SCTPCollector) Start(ctx context.Context) error {
    for {
        conn, err := c.listener.AcceptSCTP()
        if err != nil {
            continue
        }
        go c.handleConnection(ctx, conn)
    }
}
```

**任务清单**:
- [ ] 安装 SCTP 库：`go get github.com/ishidawataru/sctp`
- [ ] 实现 SCTP Listener
- [ ] 实现 SCTP 消息接收（处理部分消息）
- [ ] 实现 SCTP 多流支持（RFC 7011 建议）
- [ ] 添加连接管理（超时、重连）
- [ ] 添加错误处理和日志

#### 8.2 TCP Collector (次要优先级)
**新建**: `pkg/sav/collector_tcp.go`

**TCP 的特点**:
- 简单但需要处理消息边界
- IPFIX over TCP 需要长度前缀（RFC 7011 第 10.1 节）

```go
type TCPCollector struct {
    listener net.Listener
    handler  RecordHandler
}

func (c *TCPCollector) handleConnection(conn net.Conn) error {
    reader := bufio.NewReader(conn)
    for {
        // 读取 IPFIX 消息头（16 字节）
        header := make([]byte, 16)
        if _, err := io.ReadFull(reader, header); err != nil {
            return err
        }
        
        // 解析长度字段
        length := binary.BigEndian.Uint16(header[2:4])
        
        // 读取剩余数据
        data := make([]byte, length-16)
        if _, err := io.ReadFull(reader, data); err != nil {
            return err
        }
        
        // 处理消息
        message := append(header, data...)
        c.processMessage(message)
    }
}
```

**任务清单**:
- [ ] 实现 TCP Listener
- [ ] 处理消息边界（基于长度字段）
- [ ] 实现流式接收
- [ ] 添加缓冲和背压控制
- [ ] 错误恢复机制

#### 8.3 UDP Collector (可选)
**新建**: `pkg/sav/collector_udp.go`

**UDP 的特点**:
- 最简单：每个数据包就是一个 IPFIX 消息
- 不可靠：可能丢包

```go
type UDPCollector struct {
    conn    *net.UDPConn
    handler RecordHandler
}

func (c *UDPCollector) Start(ctx context.Context) error {
    buf := make([]byte, 65535)  // MTU 大小
    for {
        n, addr, err := c.conn.ReadFromUDP(buf)
        if err != nil {
            continue
        }
        
        // 处理消息
        message := make([]byte, n)
        copy(message, buf[:n])
        go c.processMessage(message, addr)
    }
}
```

**任务清单**:
- [ ] 实现 UDP Listener
- [ ] 处理 MTU 限制（拆分大消息）
- [ ] 添加丢包统计
- [ ] （可选）实现模板缓存

#### 8.4 统一的 Collector 接口
**新建**: `pkg/sav/collector_interface.go`

```go
// Collector 是所有收集器的通用接口
type Collector interface {
    // Start 启动收集器（阻塞）
    Start(ctx context.Context) error
    
    // Stop 停止收集器
    Stop() error
    
    // Stats 返回统计信息
    Stats() CollectorStats
}

type CollectorStats struct {
    RecordsReceived uint64
    BytesReceived   uint64
    Errors          uint64
    ConnectionCount uint64
}

// 工厂函数
func NewCollector(transport string, addr string, handler RecordHandler) (Collector, error) {
    switch transport {
    case "sctp":
        return NewSCTPCollector(addr, handler)
    case "tcp":
        return NewTCPCollector(addr, handler)
    case "udp":
        return NewUDPCollector(addr, handler)
    default:
        return nil, fmt.Errorf("unsupported transport: %s", transport)
    }
}
```

#### 8.5 更新 collector CLI
**文件**: `cmd/collector/main.go`

添加网络收集模式：
```bash
# SCTP 模式（推荐）
./bin/sav_collector --transport sctp --listen :4739

# TCP 模式
./bin/sav_collector --transport tcp --listen :4739

# UDP 模式
./bin/sav_collector --transport udp --listen :4739

# 文件模式（保留）
./bin/sav_collector --file input.ipfix
```

---

### Phase 9: 性能优化和数据转换 ⚡ (P1 重要)

#### 9.1 性能基准测试
- [ ] 测试编码速度 (records/sec)
- [ ] 测试解码速度
- [ ] 测试网络吞吐量（SCTP/TCP/UDP）
- [ ] 测试内存使用

#### 9.2 性能优化
- [ ] 使用对象池减少内存分配
- [ ] 批量处理提升吞吐量
- [ ] 并发处理支持（goroutine pool）
- [ ] 零拷贝优化

#### 9.3 数据转换器 (可选)
- [ ] IPFIX → JSON 转换器
- [ ] IPFIX → CSV 转换器
- [ ] JSON → IPFIX 转换器

---

## 📊 实现优先级（已更新）

### 🔴 P0 - 紧急（核心功能修正，必须完成）
1. **Phase 1**: 修正 SAV IE 定义 ⭐⭐⭐
2. **Phase 3**: 修正数据编码器
3. **Phase 4**: 修正数据解码器
4. **Phase 5**: 更新 CLI 工具
5. **Phase 7.1**: 单元测试
6. **Phase 7.2**: 数据验证器 ⭐ (必须紧跟 Phase 7.1)
7. **Phase 7.3**: 兼容性测试
8. **Phase 8.1**: SCTP Collector ⭐⭐⭐ (RFC 7011 强制要求)
9. **Phase 8.2**: TCP Collector

### 🟡 P1 - 重要（完善功能）
10. **Phase 2**: 模板定义检查
11. **Phase 6**: 完善文档
12. **Phase 7.4**: 集成测试
13. **Phase 8.3**: UDP Collector
14. **Phase 9.1**: 性能基准测试
15. **Phase 9.2**: 性能优化

### 🟢 P2 - 可选（增强功能）
16. **Phase 9.3**: 数据转换器

---

## 🎯 修订后的里程碑

### Milestone 1: 语义修正和基础验证 (2-3 天) ⭐
- ✅ 识别问题
- ⬜ Phase 1 完成（修正 IE 定义）
- ⬜ Phase 3-5 完成（修正编解码器和 CLI）
- ⬜ Phase 7.1 完成（单元测试）
- ⬜ **Phase 7.2 完成（数据验证器）** ← 新增
- ⬜ Phase 7.3 完成（兼容性测试）
- ⬜ 基本功能验证通过

### Milestone 2: 网络传输支持 (2-3 天) ⭐⭐⭐
- ⬜ **Phase 8.1 完成（SCTP Collector）** ← 最高优先级
- ⬜ Phase 8.2 完成（TCP Collector）
- ⬜ Phase 8.3 完成（UDP Collector）
- ⬜ Phase 8.4 完成（统一接口）
- ⬜ 网络收集功能验证通过
- ⬜ **RFC 7011 符合性验证**

### Milestone 3: 文档和性能 (1-2 天)
- ⬜ Phase 2 完成（模板检查）
- ⬜ Phase 6 完成（文档更新）
- ⬜ Phase 7.4 完成（集成测试）
- ⬜ Phase 9.1 完成（性能测试）

### Milestone 4: 生产就绪 (可选)
- ⬜ Phase 9.2 完成（性能优化）
- ⬜ Phase 9.3 完成（数据转换）
- ⬜ 压力测试和稳定性验证

---

## 🎯 里程碑

### Milestone 1: 语义修正 (1-2 天)
- ✅ 识别问题
- ⬜ Phase 1 完成
- ⬜ Phase 3-5 完成
- ⬜ 基本功能验证通过

### Milestone 2: 文档和测试 (1-2 天)
- ⬜ Phase 6 完成
- ⬜ Phase 7.1 完成
- ⬜ 单元测试覆盖率 > 80%

### Milestone 3: 互操作性验证 (1 天)
- ⬜ Phase 7.3 完成
- ⬜ 与 C 实现互操作测试通过

### Milestone 4: 生产就绪 (可选)
- ⬜ Phase 8 完成
- ⬜ Phase 9.2 完成
- ⬜ 性能和稳定性验证

---

## 📋 验收标准（已更新）

### 功能验收
- [ ] 所有 SAV IE 语义与 draft 一致
- [ ] 生成的 IPFIX 文件可被 C 实现读取
- [ ] 可读取 C 实现生成的 IPFIX 文件
- [ ] CLI 工具输出正确的语义信息
- [ ] **数据验证器可检测所有非法值** ← 新增
- [ ] **支持 SCTP 网络收集（RFC 7011 要求）** ← 新增
- [ ] **支持 TCP 网络收集** ← 新增
- [ ] **支持 UDP 网络收集** ← 新增

### 质量验收
- [ ] 单元测试覆盖率 > 80%
- [ ] 无已知的语义错误
- [ ] 代码注释完整
- [ ] 文档准确无误
- [ ] **所有验证规则有对应的测试用例** ← 新增
- [ ] **网络传输功能有集成测试** ← 新增

### RFC 符合性验收 ⭐ (关键)
- [ ] **RFC 7011 第 3 节**: IPFIX 消息格式正确
- [ ] **RFC 7011 第 8 节**: Template 管理正确
- [ ] **RFC 7011 第 10 节**: SCTP 传输支持
- [ ] **RFC 6313**: SubTemplateList 格式正确
- [ ] **draft-cao-opsawg-ipfix-sav-01**: SAV IE 语义正确

### 性能验收（重要）
- [ ] 编码速度 > 10k records/sec
- [ ] 解码速度 > 20k records/sec
- [ ] 内存使用 < 100 MB (处理 10k records)
- [ ] **SCTP 吞吐量 > 5k records/sec** ← 新增
- [ ] **TCP 吞吐量 > 8k records/sec** ← 新增
- [ ] **支持至少 100 个并发 SCTP 连接** ← 新增

---

## 🔧 实施建议

### 开发流程
1. **先审查**: 仔细审查这个计划，确认理解正确
2. **逐步修正**: 按照 Phase 1 → 5 的顺序修正
3. **持续验证**: 每个 Phase 完成后立即测试
4. **文档同步**: 修改代码的同时更新文档

### 质量保证
1. **对照 C 实现**: 每个修改都与 C 实现对比
2. **参考 draft**: 所有语义以 draft 为准
3. **交叉验证**: 使用 C 和 Go 互相验证生成的数据

### 风险控制
1. **备份代码**: 修改前创建 Git 分支
2. **小步迭代**: 每次只修改一个文件/功能
3. **回归测试**: 确保修改不破坏已有功能

---

## 📞 需要确认的问题

### ❓ 问题 1: Enterprise ID
- **当前**: 6871
- **C 实现**: 6871
- **确认**: ✅ 保持 6871

### ❓ 问题 2: 模板 ID
- **当前**: 901-904 (子模板), 400 (主模板)
- **C 实现**: 相同
- **确认**: ✅ 保持不变

### ❓ 问题 3: SubTemplateList 处理
- **当前**: 手动二进制编解码
- **替代方案**: 使用 go-ipfix 的 STL API (如果存在)
- **建议**: 继续手动处理（更可控）

---

## ✅ 审查清单

在开始实施前，请确认：

- [ ] 理解了 SAV IE 的正确语义
- [ ] 认同当前实现的语义错误
- [ ] 同意修正方案和优先级
- [ ] 理解了四种 SAV 模式的映射关系
- [ ] 确认 Phase 1-5 是必须完成的
- [ ] 确认 Phase 6-9 的优先级合理

---

**准备好开始了吗？请审查这个计划，确认无误后我们立即开始 Phase 1！** 🚀
