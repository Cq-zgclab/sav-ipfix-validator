# SAV IPFIX Real-Time Streaming Demo

**实时流式 SAV IPFIX 遥测演示系统**

## 🎯 项目简介

这是一个基于 **draft-cao-opsawg-ipfix-sav-01** 规范的 SAV IPFIX 实时流式演示系统，通过 Server-Sent Events (SSE) 技术模拟真实的 IPFIX 数据流推送，动态展示源地址验证（SAV）遥测数据的实时采集、解析和分析过程。

## ✨ 核心特性

### 基础功能
- 🔄 **实时流式推送**：通过 SSE 按时间序列推送 27 条 SAV IPFIX 记录
- 🎮 **播放控制**：支持开始、暂停、继续、重置播放
- ⚡ **速度调节**：0.5x、1x、2x、4x 四档播放速度
- 📊 **实时解析**：动态解析并展示 4 个 SAV 扩展信息元素

### 进阶分析
1. **根本原因分析**
   - 实时高亮验证失败记录
   - 展开显示 `savMatchedContentList` 详情
   - 分析源地址与规则列表的匹配情况

2. **跨模式统计分析**
   - 接口基础 vs 前缀基础验证模式分布
   - 允许列表 vs 阻止列表规则类型统计
   - 动态更新饼图和柱状图

3. **策略执行跟踪**
   - Permit、Discard、Rate-limit、Redirect 四种动作分布
   - 实时累计统计
   - 策略效果可视化

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────┐
│  前端 (HTML + Vanilla JS + Chart.js)                │
│  ├── 实时事件流展示                                 │
│  ├── 时间轴可视化                                   │
│  └── 三个进阶分析面板                               │
└──────────────────┬──────────────────────────────────┘
                   │ SSE (Server-Sent Events)
┌──────────────────▼──────────────────────────────────┐
│  Go 后端 (单一可执行文件)                           │
│  ├── SSE 流式推送服务                               │
│  ├── 播放控制 API                                   │
│  └── 流式数据管理器                                 │
└──────────────────┬──────────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────────┐
│  数据源 (复用现有 pkg/sav 库)                       │
│  ├── Scenario 1: 攻击检测 (13 条记录)              │
│  ├── Scenario 2: 接口分布 (7 条记录)               │
│  └── Scenario 3: 策略动作 (7 条记录)               │
└─────────────────────────────────────────────────────┘
```

## 📁 目录结构

```
sav-demo-lite/
├── main.go                    # 主程序入口
├── internal/
│   └── stream_server.go      # SSE 流式服务器核心逻辑
├── web/
│   ├── index.html            # 前端界面
│   └── app.js                # 前端逻辑
├── go.mod                     # Go 模块配置
└── README.md                  # 本文档
```

## 🚀 快速开始

### 1. 编译

```bash
cd sav-demo-lite
go build -o sav-demo-lite
```

### 2. 运行

```bash
./sav-demo-lite
```

默认在 `http://localhost:8080` 启动服务。

### 3. 访问

在浏览器中打开：
```
http://localhost:8080
```

### 4. 演示流程

1. 点击 **Start** 按钮开始播放
2. 观察实时事件流中的 SAV 记录推送
3. 查看时间轴上的记录顺序
4. 切换到三个分析面板查看统计数据
5. 可随时 **Pause** 暂停讲解
6. 使用 **Speed** 调节播放速度
7. 点击 **Reset** 重新开始

## 📊 SAV IPFIX 扩展字段

本系统展示以下 4 个 SAV 扩展信息元素（IEs）：

| 字段名 | IE ID | 取值 | 含义 |
|--------|-------|------|------|
| `savRuleType` | TBD | 0 | Allowlist（允许列表） |
|  |  | 1 | Blocklist（阻止列表） |
| `savTargetType` | TBD | 0 | Interface-Based（接口基础） |
|  |  | 1 | Prefix-Based（前缀基础） |
| `savPolicyAction` | TBD | 0 | Permit（允许） |
|  |  | 1 | Discard（丢弃） |
|  |  | 2 | Rate-limit（限速） |
|  |  | 3 | Redirect（重定向） |
| `savMatchedContentList` | TBD | SubTemplateList | 接口-前缀映射列表 |

## 🎬 三个演示场景

### Scenario 1: Spoofing-Attack-Detection
**13 条记录** - 展示攻击检测的时序分析

- **宏观**：过去 1 小时的攻击趋势（5 分钟间隔）
- **微观**：攻击峰值时刻的详细规则匹配情况
- **价值**：从趋势发现异常 → 规则级故障排查

### Scenario 2: Multi-Interface-Distribution
**7 条记录** - 展示多接口流量分布

- **宏观**：5 个网络接口的流量分布统计
- **微观**：问题接口的详细规则配置
- **价值**：空间分析 → 配置审计 → 优化建议

### Scenario 3: Policy-Action-Effectiveness
**7 条记录** - 展示策略动作效果

- **宏观**：4 种策略动作的分布统计
- **微观**：每种动作的触发详情
- **价值**：量化执行效果 → 策略优化决策

## 🛠️ API 接口

### SSE 流式端点
```
GET /api/stream/events
```
持续推送 SAV IPFIX 记录事件。

### 控制接口
```
POST /api/control/start      # 开始播放
POST /api/control/pause      # 暂停播放
POST /api/control/resume     # 继续播放
POST /api/control/reset      # 重置播放
POST /api/control/speed?value=1.0  # 设置速度
GET  /api/status             # 获取状态
```

## 📋 技术规格

| 项目 | 规格 |
|------|------|
| 开发语言 | Go 1.21+ |
| 前端技术 | HTML5 + Vanilla JavaScript + Chart.js |
| 通信协议 | Server-Sent Events (SSE) |
| 数据源 | 复用 `go-implementation/pkg/sav` 库 |
| 记录数量 | 27 条 IPFIX 记录（3 个场景） |
| 演示时长 | 54 秒（默认 1x 速度，每 2 秒 1 条） |
| 依赖库 | 仅标准库（无外部依赖） |

## 🎯 演示价值

1. **IPFIX 流式特性**：展示 IPFIX 作为遥测协议的实时推送能力
2. **SAV 扩展字段**：完整展示 4 个 SAV IEs 的实际应用
3. **多维度分析**：从宏观统计到微观详情的完整分析链路
4. **RFC 合规性**：基于 RFC 7011、RFC 6313 和 draft-cao-opsawg-ipfix-sav-01

## 🔗 相关规范

- [RFC 7011 - IPFIX Protocol Specification](https://www.rfc-editor.org/rfc/rfc7011)
- [RFC 6313 - Export of Structured Data in IPFIX](https://www.rfc-editor.org/rfc/rfc6313)
- [draft-cao-opsawg-ipfix-sav-01](https://datatracker.ietf.org/doc/draft-cao-opsawg-ipfix-sav/)

## 📝 许可证

Copyright © 2025 Cq-zgclab

---

**开发时间线**：
- Day 1: SSE 服务器 + 基础前端 ✅
- Day 2: 三个分析面板 + 实时图表
- Day 3: 优化 + 演示脚本 + 文档
