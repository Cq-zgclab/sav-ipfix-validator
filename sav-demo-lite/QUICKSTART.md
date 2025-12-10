# 🚀 SAV IPFIX Demo - Quick Start Guide

## 一键启动

```bash
cd sav-demo-lite
./sav-demo-lite
```

默认端口：`8888`  
访问地址：`http://localhost:8888`

---

## GitHub Codespace 使用

如果在 GitHub Codespace 中运行：

1. **启动服务器**：
   ```bash
   cd sav-demo-lite
   ./sav-demo-lite --port 8888
   ```

2. **获取访问地址**：
   - Codespace 会自动转发端口 8888
   - 在 VS Code 的 "PORTS" 面板中找到 8888 端口
   - 点击地球图标或复制转发的 URL
   - 格式类似：`https://xxx-8888.app.github.dev`

3. **访问 Dashboard**：
   打开上述 URL 即可

---

## 演示步骤

### 1. 启动系统
```bash
./sav-demo-lite
```

看到以下输出表示成功：
```
╔═══════════════════════════════════════════════════════════╗
║      SAV IPFIX Real-Time Streaming Demo System           ║
╚═══════════════════════════════════════════════════════════╝

✅ Loaded 27 SAV IPFIX records from 3 scenarios
🚀 SAV IPFIX Demo Server starting on http://localhost:8888
```

### 2. 打开浏览器
访问：`http://localhost:8888`

### 3. 开始演示
1. **点击 Start** - 开始播放 IPFIX 数据流
2. **观察实时事件** - 记录逐条推送并展示
3. **查看时间轴** - 左侧显示记录时序
4. **切换分析面板** - 底部三个标签页

### 4. 播放控制
- **Pause** - 暂停讲解
- **Resume** - 继续播放
- **Reset** - 重新开始
- **Speed** - 调整播放速度（0.5x ~ 4x）

---

## 测试验证

运行自动化测试：
```bash
./test.sh
```

测试内容包括：
- ✓ 服务器运行状态
- ✓ 5 个控制 API
- ✓ SSE 流式连接

---

## 常见问题

### Q1: 端口被占用
```bash
# 使用其他端口
./sav-demo-lite --port 9999
```

### Q2: 无法访问 Dashboard
```bash
# 检查服务器是否运行
ps aux | grep sav-demo-lite

# 检查端口是否监听
lsof -i :8888
```

### Q3: SSE 连接失败
- 检查浏览器控制台是否有错误
- 确认 `/api/stream/events` 端点可访问
- 尝试刷新页面

---

## 系统要求

- **操作系统**：Linux / macOS / Windows
- **浏览器**：Chrome / Firefox / Edge（最新版本）
- **内存**：至少 100MB 可用
- **网络**：本地运行，无需外网

---

## 架构说明

```
┌─────────────────┐
│   Browser       │
│   (Frontend)    │
└────────┬────────┘
         │ SSE
┌────────▼────────┐
│  Go Server      │
│  (Port 8888)    │
└────────┬────────┘
         │
┌────────▼────────┐
│  27 Records     │
│  (3 Scenarios)  │
└─────────────────┘
```

---

## 演示时长

| 速度 | 总时长 | 每条记录间隔 |
|------|--------|--------------|
| 0.5x | 108秒  | 4秒 |
| 1.0x | 54秒   | 2秒 |
| 2.0x | 27秒   | 1秒 |
| 4.0x | 13.5秒 | 0.5秒 |

**推荐**：演示时使用 **1.0x 速度**（总时长 54 秒），便于讲解。

---

## 演示技巧

### 快速完整演示（1分钟）
```
速度：4x
目标：快速展示全部 27 条记录和最终统计
```

### 详细讲解演示（5分钟）
```
速度：1x
暂停关键记录，详细讲解 SAV 扩展字段
切换三个分析面板，展示运维价值
```

### 循环播放模式
```
播放完成后自动重置，支持连续演示
适合展位或会议室循环播放
```

---

## 文件说明

```
sav-demo-lite/
├── sav-demo-lite          # 可执行文件
├── main.go                # 主程序
├── internal/
│   └── stream_server.go   # SSE 服务器核心
├── web/
│   ├── index.html         # 前端界面
│   └── app.js             # 前端逻辑
├── README.md              # 完整文档
├── DEMO_SCRIPT.md         # 演示脚本
├── QUICKSTART.md          # 本文件
└── test.sh                # 测试脚本
```

---

## 下一步

- 📖 阅读 `README.md` 了解完整功能
- 🎬 阅读 `DEMO_SCRIPT.md` 准备演示
- 🔧 运行 `./test.sh` 验证系统

---

**准备好了？开始演示吧！** 🚀
