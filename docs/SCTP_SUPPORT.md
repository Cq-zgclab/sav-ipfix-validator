# SCTP 支持说明 - IPFIX 网络传输

**项目**: SAV IPFIX Validator  
**日期**: 2025-12-09  
**基于**: RFC 7011 Section 10

---

## 🎯 为什么必须支持 SCTP？

### RFC 7011 的明确要求

> **RFC 7011, Section 10**: "IPFIX has one Exporting Process and Collecting
> Process protocol, IPFIX over SCTP (Section 10.2.2), which **MUST** be
> implemented."

**关键词**: **MUST** (RFC 2119 术语，表示强制要求)

### SCTP 的优势

| 特性 | TCP | UDP | SCTP |
|------|-----|-----|------|
| **可靠传输** | ✅ | ❌ | ✅ |
| **有序传输** | ✅ | ❌ | ✅ (可选) |
| **消息边界** | ❌ | ✅ | ✅ |
| **多流支持** | ❌ | ❌ | ✅ |
| **部分可靠性** | ❌ | ✅ | ✅ (PR-SCTP) |
| **多宿主** | ❌ | ❌ | ✅ |
| **拥塞控制** | ✅ | ❌ | ✅ |

---

## 📖 SCTP 基础知识

### 什么是 SCTP？

**SCTP** (Stream Control Transmission Protocol, RFC 4960) 是一种传输层协议，结合了 TCP 和 UDP 的优点：

- **面向消息**: 像 UDP 一样保持消息边界
- **可靠传输**: 像 TCP 一样保证数据到达
- **多流**: 一个连接可以有多个独立的流
- **多宿主**: 支持多个 IP 地址（容错）

### SCTP 与 IPFIX 的完美结合

```
IPFIX 消息特点:
  - 完整的消息单元（Message Header + Sets）
  - 需要按顺序处理模板
  - 可能有多个数据流（不同 Observation Domain）

SCTP 特性匹配:
  ✅ 保持消息边界 → 一个 SCTP 消息 = 一个 IPFIX 消息
  ✅ 可靠传输 → 不丢失模板定义
  ✅ 多流支持 → 不同 Domain 用不同流
  ✅ 有序传输 → 模板先于数据到达
```

---

## 🔧 Go 语言中的 SCTP 支持

### 可用的库

#### 1. `github.com/ishidawataru/sctp` (推荐)

```bash
go get github.com/ishidawataru/sctp
```

**特点**:
- ✅ 纯 Go 实现
- ✅ 支持 Linux, FreeBSD, Darwin
- ✅ API 类似 net.Conn
- ✅ 支持多流
- ⚠️ 需要系统支持 SCTP

**基本用法**:
```go
import "github.com/ishidawataru/sctp"

// Server 端
laddr, _ := sctp.ResolveSCTPAddr("sctp", "0.0.0.0:4739")
listener, err := sctp.ListenSCTP("sctp", laddr)
if err != nil {
    log.Fatal(err)
}

for {
    conn, err := listener.AcceptSCTP()
    if err != nil {
        continue
    }
    go handleConnection(conn)
}

// Client 端
raddr, _ := sctp.ResolveSCTPAddr("sctp", "192.0.2.1:4739")
conn, err := sctp.DialSCTP("sctp", nil, raddr)
if err != nil {
    log.Fatal(err)
}
defer conn.Close()

// 发送 IPFIX 消息
n, err := conn.Write(ipfixMessage)

// 接收 IPFIX 消息
buf := make([]byte, 65535)
n, info, err := conn.SCTPRead(buf)
// info.Stream - 流 ID
// info.PPID - Payload Protocol Identifier
```

#### 2. `github.com/pion/sctp` (备选)

```bash
go get github.com/pion/sctp
```

**特点**:
- ✅ 纯 Go 实现
- ✅ 用户空间实现（不依赖内核 SCTP）
- ✅ 跨平台
- ⚠️ 性能可能不如内核实现

---

## 📐 IPFIX over SCTP 架构设计

### RFC 7011 的 SCTP 规范

#### Payload Protocol Identifier (PPID)

```
IPFIX over SCTP 使用 PPID = 0 (unspecified)
```

#### 端口

```
默认端口: 4739 (IANA 注册)
```

#### 多流使用建议

RFC 7011 Section 10.2.2:
```
- Stream 0: 模板和选项模板
- Stream 1-N: 数据记录

原因: 确保模板先于数据到达
```

### 我们的实现架构

```
┌─────────────────────────────────────────────┐
│         SCTP Collector (Server)             │
├─────────────────────────────────────────────┤
│  1. SCTP Listener (port 4739)               │
│     ↓                                        │
│  2. Accept Connections                      │
│     ↓                                        │
│  3. For each connection:                    │
│     - Goroutine per connection              │
│     - Multi-stream support                  │
│     - Template cache (per connection)       │
│     ↓                                        │
│  4. Read SCTP Messages                      │
│     - Stream 0: Templates → Cache           │
│     - Stream 1-N: Data → Process            │
│     ↓                                        │
│  5. Parse IPFIX Messages                    │
│     - Binary decoder (我们已有)              │
│     ↓                                        │
│  6. Call RecordHandler                      │
│     - 用户提供的处理函数                      │
└─────────────────────────────────────────────┘
```

---

## 💻 实现代码框架

### collector_sctp.go

```go
package sav

import (
    "context"
    "fmt"
    "net"
    "sync"
    "time"
    
    "github.com/ishidawataru/sctp"
)

// SCTPCollector collects IPFIX messages over SCTP
type SCTPCollector struct {
    listener *sctp.SCTPListener
    handler  RecordHandler
    
    // 连接管理
    connections sync.Map  // map[string]*sctpConn
    
    // 统计信息
    stats CollectorStats
    mu    sync.RWMutex
    
    // 配置
    config SCTPConfig
}

type SCTPConfig struct {
    // 监听地址
    ListenAddr string  // 默认: ":4739"
    
    // 流配置
    UseMultiStream bool  // 是否使用多流
    TemplateStream uint16 // 模板流 ID (默认: 0)
    
    // 连接管理
    MaxConnections int           // 最大连接数
    IdleTimeout    time.Duration // 空闲超时
    
    // 缓冲区大小
    ReadBufferSize int  // 默认: 65535
}

// NewSCTPCollector creates a new SCTP collector
func NewSCTPCollector(config SCTPConfig, handler RecordHandler) (*SCTPCollector, error) {
    // 设置默认值
    if config.ListenAddr == "" {
        config.ListenAddr = ":4739"
    }
    if config.ReadBufferSize == 0 {
        config.ReadBufferSize = 65535
    }
    if config.MaxConnections == 0 {
        config.MaxConnections = 100
    }
    if config.IdleTimeout == 0 {
        config.IdleTimeout = 30 * time.Minute
    }
    
    // 解析监听地址
    laddr, err := sctp.ResolveSCTPAddr("sctp", config.ListenAddr)
    if err != nil {
        return nil, fmt.Errorf("invalid listen address: %w", err)
    }
    
    // 创建 SCTP listener
    listener, err := sctp.ListenSCTP("sctp", laddr)
    if err != nil {
        return nil, fmt.Errorf("failed to create SCTP listener: %w", err)
    }
    
    return &SCTPCollector{
        listener: listener,
        handler:  handler,
        config:   config,
    }, nil
}

// Start starts the SCTP collector (blocking)
func (c *SCTPCollector) Start(ctx context.Context) error {
    defer c.listener.Close()
    
    // Accept loop
    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        default:
        }
        
        // Accept connection with timeout
        c.listener.SetDeadline(time.Now().Add(1 * time.Second))
        conn, err := c.listener.AcceptSCTP()
        if err != nil {
            if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
                continue  // Timeout, check context
            }
            return fmt.Errorf("accept error: %w", err)
        }
        
        // Check connection limit
        connCount := c.getConnectionCount()
        if connCount >= c.config.MaxConnections {
            conn.Close()
            c.incrementStat("rejected_connections")
            continue
        }
        
        // Handle connection in goroutine
        go c.handleConnection(ctx, conn)
    }
}

// handleConnection handles a single SCTP connection
func (c *SCTPCollector) handleConnection(ctx context.Context, conn *sctp.SCTPConn) {
    defer conn.Close()
    
    remoteAddr := conn.RemoteAddr().String()
    c.addConnection(remoteAddr, conn)
    defer c.removeConnection(remoteAddr)
    
    // Template cache for this connection
    // (每个连接可能发送不同的模板)
    templateCache := make(map[uint16]*Template)
    
    // Read buffer
    buf := make([]byte, c.config.ReadBufferSize)
    
    // Read loop
    for {
        select {
        case <-ctx.Done():
            return
        default:
        }
        
        // Set read timeout
        conn.SetReadDeadline(time.Now().Add(c.config.IdleTimeout))
        
        // Read SCTP message
        n, info, err := conn.SCTPRead(buf)
        if err != nil {
            if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
                // Idle timeout, close connection
                return
            }
            c.incrementStat("read_errors")
            return
        }
        
        // Update stats
        c.incrementStat("messages_received")
        c.addBytesReceived(uint64(n))
        
        // IPFIX message
        message := make([]byte, n)
        copy(message, buf[:n])
        
        // Process based on stream
        if c.config.UseMultiStream && info.Stream == c.config.TemplateStream {
            // Stream 0: Templates
            if err := c.processTemplate(message, templateCache); err != nil {
                c.incrementStat("template_errors")
                // Log error but continue
            }
        } else {
            // Other streams: Data
            if err := c.processData(message, templateCache); err != nil {
                c.incrementStat("data_errors")
                // Log error but continue
            }
        }
    }
}

// processTemplate processes template messages
func (c *SCTPCollector) processTemplate(message []byte, cache map[uint16]*Template) error {
    // TODO: Parse template sets and update cache
    // 使用我们已有的二进制解析器
    return nil
}

// processData processes data messages
func (c *SCTPCollector) processData(message []byte, cache map[uint16]*Template) error {
    // TODO: Parse data sets using template cache
    // 使用我们已有的 SAVRecordReader
    return nil
}

// Stop stops the collector
func (c *SCTPCollector) Stop() error {
    return c.listener.Close()
}

// Stats returns collector statistics
func (c *SCTPCollector) Stats() CollectorStats {
    c.mu.RLock()
    defer c.mu.RUnlock()
    return c.stats
}

// Helper methods for connection management
func (c *SCTPCollector) addConnection(addr string, conn *sctp.SCTPConn) {
    c.connections.Store(addr, conn)
}

func (c *SCTPCollector) removeConnection(addr string) {
    c.connections.Delete(addr)
}

func (c *SCTPCollector) getConnectionCount() int {
    count := 0
    c.connections.Range(func(_, _ interface{}) bool {
        count++
        return true
    })
    return count
}

func (c *SCTPCollector) incrementStat(name string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    // Update stats...
}

func (c *SCTPCollector) addBytesReceived(bytes uint64) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.stats.BytesReceived += bytes
}
```

### 使用示例

```go
package main

import (
    "context"
    "fmt"
    "log"
    
    "github.com/Cq-zgclab/sav-ipfix-validator/pkg/sav"
)

func main() {
    // 配置 SCTP collector
    config := sav.SCTPConfig{
        ListenAddr:     ":4739",
        UseMultiStream: true,
        TemplateStream: 0,
        MaxConnections: 100,
        IdleTimeout:    30 * time.Minute,
    }
    
    // 创建 handler
    handler := func(record *sav.SAVRecord) error {
        fmt.Printf("Received SAV record: %+v\n", record)
        // 存入数据库、发送告警等...
        return nil
    }
    
    // 创建 collector
    collector, err := sav.NewSCTPCollector(config, handler)
    if err != nil {
        log.Fatal(err)
    }
    
    // 启动 collector
    ctx := context.Background()
    log.Println("Starting SCTP collector on :4739")
    if err := collector.Start(ctx); err != nil {
        log.Fatal(err)
    }
}
```

---

## 🧪 测试计划

### 单元测试

```go
// pkg/sav/collector_sctp_test.go

func TestSCTPCollector_Basic(t *testing.T) {
    // 1. 创建 collector
    // 2. 在 goroutine 中启动
    // 3. 创建 SCTP 客户端连接
    // 4. 发送 IPFIX 消息
    // 5. 验证 handler 被调用
    // 6. 停止 collector
}

func TestSCTPCollector_MultiStream(t *testing.T) {
    // 测试多流功能
    // Stream 0: 发送模板
    // Stream 1: 发送数据
    // 验证正确处理
}

func TestSCTPCollector_MaxConnections(t *testing.T) {
    // 测试连接数限制
    // 尝试创建超过限制的连接
    // 验证拒绝新连接
}

func TestSCTPCollector_IdleTimeout(t *testing.T) {
    // 测试空闲超时
    // 创建连接但不发送数据
    // 验证连接被关闭
}
```

### 集成测试

```bash
# 1. 启动 SCTP collector
./bin/sav_collector --transport sctp --listen :4739 &

# 2. 使用 sctp_test 工具发送数据
sctp_test -H 127.0.0.1 -P 4739 -l < test_data.ipfix

# 3. 验证收到的数据
```

---

## 📋 实施清单

### Phase 8.1: SCTP Collector 实现

#### 8.1.1 环境准备
- [ ] 检查系统 SCTP 支持: `lsmod | grep sctp`
- [ ] 安装 SCTP 工具: `apt-get install lksctp-tools`
- [ ] 安装 Go SCTP 库: `go get github.com/ishidawataru/sctp`
- [ ] 测试 SCTP 连接: `sctp_test`

#### 8.1.2 核心实现
- [ ] 创建 `pkg/sav/collector_sctp.go`
- [ ] 实现 `SCTPCollector` 结构体
- [ ] 实现 `Start()` 方法（accept loop）
- [ ] 实现 `handleConnection()` 方法
- [ ] 实现模板缓存机制
- [ ] 实现多流支持（可选但推荐）

#### 8.1.3 集成到现有代码
- [ ] 统一 Collector 接口
- [ ] 更新 `cmd/collector/main.go` 添加 SCTP 模式
- [ ] 添加配置文件支持

#### 8.1.4 测试
- [ ] 单元测试
- [ ] 集成测试
- [ ] 性能测试（吞吐量、并发连接）
- [ ] 与 C 实现互操作测试

#### 8.1.5 文档
- [ ] 更新 README_GO.md
- [ ] 添加 SCTP 使用示例
- [ ] 添加故障排除指南

---

## 🔧 故障排除

### 问题 1: SCTP 模块未加载

```bash
# 检查
lsmod | grep sctp

# 如果没有输出，加载模块
sudo modprobe sctp

# 开机自动加载
echo "sctp" | sudo tee -a /etc/modules
```

### 问题 2: 防火墙阻止 SCTP

```bash
# 检查防火墙
sudo iptables -L -n | grep 4739

# 允许 SCTP 4739 端口
sudo iptables -A INPUT -p sctp --dport 4739 -j ACCEPT
```

### 问题 3: "protocol not supported" 错误

可能原因：
1. 系统不支持 SCTP
2. SCTP 模块未加载
3. 使用了不正确的地址族

解决方案：
```go
// 确保使用 "sctp" 网络类型
laddr, err := sctp.ResolveSCTPAddr("sctp", ":4739")
```

---

## 📚 参考资料

### RFC 文档
- **RFC 4960**: SCTP 协议规范
- **RFC 7011**: IPFIX Protocol Specification (Section 10)
- **RFC 3758**: PR-SCTP (Partial Reliability)

### Go 库文档
- https://github.com/ishidawataru/sctp
- https://pkg.go.dev/github.com/ishidawataru/sctp

### 工具
- `sctp_test`: SCTP 测试工具（lksctp-tools 包）
- `sctp_darn`: SCTP 数据生成工具
- `wireshark`: 抓包分析 SCTP 流量

---

**总结**: SCTP 是 IPFIX 的标准传输协议，必须实现以符合 RFC 7011！** 🎯
