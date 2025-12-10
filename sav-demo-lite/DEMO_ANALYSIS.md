# SAV Demo 数据展示分析

## 问题 1: 为什么有些 Record 没有显示 savMatchedContent 字段？

### 原因：Macro vs Micro 记录设计

展示系统中的 27 条记录分为两种类型：

#### 1️⃣ **Macro Records（宏观统计记录）**
- **特征**：`Rules = nil`（没有 savMatchedContentList）
- **用途**：展示聚合统计数据（时间序列、接口分布、策略效果）
- **数量**：21 条
- **示例**：
  ```
  Record #1-12: 攻击时间序列（每 5 分钟一条，共 12 条）
  Record #14-18: 多接口流量分布（每个接口一条，共 5 条）
  Record #20-23: 策略动作分布（每种动作一条，共 4 条）
  ```
- **显示逻辑**：
  ```javascript
  // app.js 第 148 行
  if (record.mappings && record.mappings.length > 0) {
      // 显示 savMatchedContentList
  }
  // mappings 为空时，不显示该字段
  ```

#### 2️⃣ **Micro Records（微观详细记录）**
- **特征**：包含完整的 `savMatchedContentList`（Rules 数组）
- **用途**：展示具体验证失败的规则细节
- **数量**：6 条
- **示例**：
  ```
  Record #13: 攻击峰值时刻的详细规则（3 条 allowlist 规则）
  Record #19: eth0 接口详细规则（3 条 allowlist 规则）
  Record #24-26: 各种策略动作的详细规则
  ```

### 完整记录分布表

| 记录编号 | 类型 | savMatchedContentList | 场景 |
|---------|------|---------------------|------|
| #1-12 | Macro | ❌ 无 | 时间序列统计（5分钟聚合） |
| #13 | **Micro** | ✅ 3条规则 | 攻击峰值时刻详细规则 |
| #14-18 | Macro | ❌ 无 | 接口流量分布统计 |
| #19 | **Micro** | ✅ 3条规则 | eth0 接口详细规则 |
| #19 | **Micro** | ✅ 2条规则 | eth1 前缀验证规则 |
| #20-23 | Macro | ❌ 无 | 策略动作效果统计 |
| #24 | **Micro** | ✅ 1条规则 | Discard 动作详细事件 |
| #25 | **Micro** | ✅ 2条规则 | Rate-limit 动作详细事件 |
| #26 | **Micro** | ✅ 1条规则 | Redirect 动作详细事件 |

### 设计意图

这是 **"宏观统计有微观细节，还要有两个结合"** 的实现：

1. **Macro（宏观）**：通过 PacketCount/ByteCount 字段展示聚合数据
   - 时间维度：过去1小时的攻击趋势
   - 空间维度：不同接口的流量分布
   - 策略维度：各种动作的效果对比

2. **Micro（微观）**：通过 savMatchedContentList 展示具体规则
   - 哪条 allowlist 规则被检查过
   - 具体哪个 interface 和 prefix 匹配/不匹配
   - 精确的故障定位依据

3. **Combined（结合）**：
   - Macro 发现异常窗口（如 Record #7 是攻击峰值）
   - Micro 提供该时刻的规则级别取证（Record #13）
   - 完整的分析闭环

---

## 问题 2: "SAV Verification Failure Root Cause Analysis" 区域的分类计数逻辑

### 当前逻辑（app.js 第 284-289 行）

```javascript
// Root cause analysis
document.getElementById('totalRecords').textContent = stats.total;
document.getElementById('discardCount').textContent = stats.discard;
document.getElementById('allowlistFailures').textContent = 
    stats.allowlist > 0 && stats.discard > 0 ? stats.discard : 0;
```

### 逻辑问题分析

#### ⚠️ **当前逻辑不正确**

```javascript
stats.allowlist > 0 && stats.discard > 0 ? stats.discard : 0
```

**问题**：
1. `stats.allowlist` 是全局 Allowlist 记录总数（包括所有动作）
2. `stats.discard` 是全局 Discard 动作总数（包括 Allowlist 和 Blocklist）
3. 当前逻辑只是简单判断：如果存在 Allowlist 记录且存在 Discard 记录，就把所有 Discard 计为 "Allowlist Failures"

**实际数据**：
- Allowlist 记录：24 条（包含 Permit、Discard、Rate-limit 等多种动作）
- Discard 动作：12 条（来自 Allowlist 也来自 Blocklist）
- 当前显示：Allowlist Failures = 12（错误！）

**正确应该是**：
- **Allowlist Failures** = RuleType=Allowlist **且** Action=Discard 的记录数
- 根据数据源统计：
  - Scenario 1: 12条时间序列 + 1条微观 = 13条 Allowlist+Discard
  - Scenario 2: 5条接口分布 + 1条微观 = 6条 Allowlist+Discard
  - Scenario 3: 0条（Discard来自Blocklist）
  - **正确答案：19 条**

---

## 修复方案

### 方案 1：修改统计逻辑（推荐）

```javascript
// 在 updateStatistics() 函数中添加组合统计
function updateStatistics(record) {
    stats.total++;
    
    // Target type
    if (record.target_type === 0) stats.interface_based++;
    else if (record.target_type === 1) stats.prefix_based++;
    
    // Rule type
    if (record.rule_type === 0) stats.allowlist++;
    else if (record.rule_type === 1) stats.blocklist++;
    
    // Policy action
    if (record.policy_action === 0) stats.permit++;
    else if (record.policy_action === 1) stats.discard++;
    else if (record.policy_action === 2) stats.ratelimit++;
    else if (record.policy_action === 3) stats.redirect++;
    
    // ✅ 新增：组合统计
    if (record.rule_type === 0 && record.policy_action === 1) {
        stats.allowlist_failures++;  // Allowlist + Discard
    }
    if (record.rule_type === 1 && record.policy_action === 1) {
        stats.blocklist_hits++;  // Blocklist + Discard
    }
}

// 在 stats 初始化时添加字段
const stats = {
    total: 0,
    interface_based: 0,
    prefix_based: 0,
    allowlist: 0,
    blocklist: 0,
    permit: 0,
    discard: 0,
    ratelimit: 0,
    redirect: 0,
    allowlist_failures: 0,  // ✅ 新增
    blocklist_hits: 0       // ✅ 新增
};

// 修改显示逻辑
document.getElementById('allowlistFailures').textContent = stats.allowlist_failures;
```

### 方案 2：更详细的 Root Cause 分析

```html
<div class="stats-grid" id="rootCauseStats">
    <div class="stat-card">
        <div class="stat-label">Total Records</div>
        <div class="stat-number" id="totalRecords">0</div>
    </div>
    <div class="stat-card">
        <div class="stat-label">Allowlist Failures</div>
        <div class="stat-number" id="allowlistFailures">0</div>
        <div class="stat-sublabel">Allowlist + Discard</div>
    </div>
    <div class="stat-card">
        <div class="stat-label">Blocklist Hits</div>
        <div class="stat-number" id="blocklistHits">0</div>
        <div class="stat-sublabel">Blocklist + Discard</div>
    </div>
    <div class="stat-card">
        <div class="stat-label">Other Actions</div>
        <div class="stat-number" id="otherActions">0</div>
        <div class="stat-sublabel">Permit/Rate-limit/Redirect</div>
    </div>
</div>
```

---

## 当前数据实际统计

根据 `scenarios.go` 数据源：

### Scenario 1: Spoofing-Attack-Detection (13 条)
- **Macro**: 12 条时间序列 (RuleType=0, Action=1, Rules=nil)
- **Micro**: 1 条详细事件 (RuleType=0, Action=1, Rules=3)
- **统计**: 13 条 Allowlist Failures

### Scenario 2: Multi-Interface-Distribution (7 条)
- **Macro**: 5 条接口分布 (RuleType=0, Action=1, Rules=nil)
- **Micro**: 1 条 eth0 详细 (RuleType=0, Action=1, Rules=3)
- **Micro**: 1 条 eth1 详细 (RuleType=0, Action=2, Rules=2)
- **统计**: 6 条 Allowlist Failures (Discard), 1 条 Rate-limit

### Scenario 3: Policy-Action-Effectiveness (7 条)
- **Macro**: 1 条 Permit (RuleType=1, Action=0)
- **Macro**: 1 条 Discard (RuleType=1, Action=1)
- **Macro**: 1 条 Rate-limit (RuleType=1, Action=2)
- **Macro**: 1 条 Redirect (RuleType=1, Action=3)
- **Micro**: 1 条 Discard 详细 (RuleType=1, Action=1, Rules=1)
- **Micro**: 1 条 Rate-limit 详细 (RuleType=0, Action=2, Rules=2)
- **Micro**: 1 条 Redirect 详细 (RuleType=1, Action=3, Rules=1)
- **统计**: 2 条 Blocklist Hits (Discard), 其他动作 5 条

### 总计
- **Total Records**: 27
- **Allowlist Failures** (RuleType=0 + Action=1): 19
- **Blocklist Hits** (RuleType=1 + Action=1): 2
- **Other Actions**: 6 (Permit=1, Rate-limit=3, Redirect=2)

---

## 演示建议

### 当前设计的优势
✅ **分层设计**：Macro 快速定位异常，Micro 深入分析根因  
✅ **真实场景**：模拟实际网络中的时间序列、空间分布、策略评估  
✅ **完整遥测**：展示 IPFIX SubTemplateList 的实际应用价值  

### 展示时说明
1. **Macro Records（无 savMatchedContentList）**：
   - "这些是聚合统计记录，展示宏观趋势"
   - "PacketCount 和 ByteCount 字段显示流量规模"
   - "快速识别异常窗口和问题接口"

2. **Micro Records（有 savMatchedContentList）**：
   - "这些是详细事件记录，展示具体规则"
   - "savMatchedContentList 显示被检查的完整 allowlist/blocklist"
   - "用于故障定位和策略审计"

3. **Root Cause Analysis 指标**：
   - 建议修复为准确的 Allowlist Failures 统计
   - 或者重新命名为 "Discard Actions" 以匹配当前逻辑
   - 添加更多维度：Blocklist Hits, Rate-limited Traffic 等

---

## 下一步改进

- [ ] 修复 Root Cause Analysis 统计逻辑
- [ ] 添加 Blocklist Hits 统计
- [ ] 为 Macro/Micro 记录添加视觉标识（如徽章）
- [ ] 添加数据过滤功能（仅显示 Macro 或仅显示 Micro）
- [ ] 添加 savMatchedContentList 的悬浮提示说明
