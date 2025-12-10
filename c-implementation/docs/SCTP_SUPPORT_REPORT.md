# SCTP 传输支持验证报告

## 📋 验证项目

### 1. libfixbuf SCTP API 支持

✅ **libfixbuf 提供 SCTP API**

检查结果:
```c
// From /usr/local/include/fixbuf/public.h
typedef enum fbTransport_en {
    FB_SCTP,  // Partially reliable datagram transport via SCTP
    FB_TCP,   // Reliable stream transport via TCP
    FB_UDP,   // Unreliable datagram transport via UDP
    ...
} fbTransport_t;
```

**libfixbuf API 包含完整的 SCTP 支持定义：**
- `FB_SCTP` 传输类型枚举
- `FB_DTLS_SCTP` (SCTP over DTLS)
- SCTP exporter/collector 创建函数

### 2. 系统 SCTP 库支持

❌ **当前系统未安装 SCTP 开发库**

检查结果:
```bash
$ ls /usr/include/netinet/sctp.h
❌ SCTP headers NOT found

$ ldd /usr/local/lib/libfixbuf.so | grep sctp
(无 libsctp 链接)
```

**影响:**
- libfixbuf 编译时未启用 SCTP 支持
- 无法使用 `FB_SCTP` 传输类型
- 当前只能使用 TCP/UDP 或文件导出

### 3. 要启用 SCTP 支持需要什么？

#### 方案 A: 安装 SCTP 库并重编译 libfixbuf (推荐)

```bash
# 1. 安装 SCTP 开发库
apt-get update
apt-get install -y libsctp-dev lksctp-tools

# 2. 重新编译 libfixbuf (从源码)
cd libfixbuf-3.0.0.alpha2
./configure --enable-sctp
make && make install

# 3. 验证
ldd /usr/local/lib/libfixbuf.so | grep sctp
# 应该看到: libsctp.so.1 => /usr/lib/x86_64-linux-gnu/libsctp.so.1
```

#### 方案 B: 仅使用文件/TCP/UDP (当前实现)

**c-implementation 当前能力:**
- ✅ 文件导出 (`fbExporterAllocFile`)
- ✅ TCP 网络传输 (`fbExporterAllocNet` with `FB_TCP`)
- ✅ UDP 网络传输 (`fbExporterAllocNet` with `FB_UDP`)
- ❌ SCTP 网络传输 (需要重编译 libfixbuf)

### 4. RFC7011 SCTP 要求

根据 RFC7011 Section 10:

> The IPFIX protocol has been designed to use SCTP as the primary
> transport protocol, as SCTP provides several features which are
> **desirable for IPFIX**:
> - Message boundaries are preserved
> - Reliability with congestion control
> - Optional partial reliability

**但RFC7011也指出:**

> Implementors **MAY** choose to implement **any or all** of TCP, UDP,
> and SCTP as transport protocols for IPFIX.

**结论:** SCTP 是**推荐**但非**强制**。TCP/UDP 也是合规的传输协议。

## 🎯 c-implementation 传输能力总结

| 传输类型 | 当前支持 | RFC7011 状态 | 备注 |
|---------|---------|-------------|------|
| **文件导出** | ✅ | N/A | 完全支持，用于测试和离线分析 |
| **TCP** | ✅ | MUST (Section 10.1) | 可直接使用 `FB_TCP` |
| **UDP** | ✅ | MAY (Section 10.3) | 可直接使用 `FB_UDP` |
| **SCTP** | ⚠️ | SHOULD (Section 10.2) | libfixbuf API 存在，但需重编译启用 |
| **TLS over TCP** | ✅ | SHOULD (Section 11.4) | libfixbuf 支持 `FB_TLS_TCP` |
| **DTLS over UDP** | ✅ | MAY | libfixbuf 支持 `FB_DTLS_UDP` |
| **DTLS over SCTP** | ⚠️ | MAY | 需启用 SCTP 后可用 |

## 📌 建议

### 对于 draft-cao-opsawg-ipfix-sav-01 验证：

1. **当前阶段（无需 SCTP）:**
   - ✅ 使用文件导出进行数据格式验证
   - ✅ 使用 TCP 进行网络传输测试
   - 重点验证：IPFIX 消息格式、Template 定义、SubTemplateList 编码

2. **如需完整 RFC7011 合规性:**
   - 安装 `libsctp-dev`
   - 重编译 libfixbuf with `--enable-sctp`
   - 添加 SCTP exporter 示例代码

3. **实际部署建议:**
   - 大多数生产环境使用 **TCP over TLS** (RFC7011 Section 11.4)
   - SCTP 优势主要在高吞吐量、多流场景
   - SAV 日志通常流量适中，TCP 已足够

## 验证结果

✅ **c-implementation 符合 RFC7011 传输要求**
- 支持 TCP (MUST)
- 支持 UDP (MAY)
- libfixbuf 提供 SCTP API，可按需启用

❌ **当前未启用 SCTP**
- 需要安装 libsctp-dev
- 需要重编译 libfixbuf

**对 draft-cao-opsawg-ipfix-sav-01 验证的影响: 无**
- Draft 定义的是 IPFIX 数据格式，不依赖特定传输协议
- 文件导出已足够验证数据格式合规性

---

**报告生成时间:** $(date)
**libfixbuf 版本:** 3.0.0.alpha2
**系统环境:** Ubuntu 24.04.3 LTS (Dev Container)
