# T1 IPFIX Export 实施进展报告
> ⚠️ 说明：本文档为历史调试记录，反映早期 T1-only 阶段的问题定位过程。当前仓库默认演示路径已切换到 Template A/B（500/501/502/503），并由 `demo/run_demo.sh` 驱动。
## 当前状态

### ✅ 已完成
1. Template 注册成功
   - Template 400 (T1 record): ✅
   - Template 900 (IPv4 Interface-to-Prefix): ✅
   - Template 901 (IPv6 Interface-to-Prefix): ✅
   - Template 902 (IPv4 Prefix-to-Interface): ✅
   - Template 903 (IPv6 Prefix-to-Interface): ✅

2. Template 导出成功
   - ipfixDump 可以看到 3 个 template definitions
   - 字段定义正确（除了 SAV IEs 显示为 _alienInformationElement，需要 XML 定义）

3. 核心原则正确
   - ✅ 一对一映射：1 个 T1 Flow = 1 个 IPFIX Record
   - ✅ Flow Key 直拷贝
   - ✅ 统计字段直导出
   - ✅ 不在 exporter 内推断或修正

### ❌ 遇到问题

**症状**：
```
Error: Cannot export flow 0: Missing external template 0:8700
Error: Cannot export flow 1: Missing external template 0:8500
Segmentation fault (core dumped)
```

**分析**：
- Template 定义成功（ipfixDump 显示 Templates 400, 900-903）
- Templates 已导出到文件
- 问题出在 `fBufAppend` 时，SubTemplateList 引用的 external template
- 8700 (0x21FC) 和 8500 (0x2134) 不是我们的 template IDs (900-903)

**可能原因**：
1. SubTemplateList 初始化时 template pointer 不正确
2. fBufSetTemplatesForExport 调用时机或方式不对
3. SubTemplateList 的 semantic 参数问题
4. 数据结构对齐问题（尽管已移除 packed 属性）

## 与已验证代码的差异

### 现有工作代码 (src/sav_exporter.c)
- 使用 `sav_record_ctx_t` 管理上下文
- 预分配 STL buffer
- 分步添加 entries
- 使用 `memcpy` 复制到 STL data pointer

### 当前实现 (test_sav_t1_ipfix_minimal.c)
- 直接在 export 函数内初始化 STL
- 即时创建单个 entry
- 直接写入 STL entry data

### 关键差异点
可能需要：
1. 在每次 fBufAppend 前重新调用 fBufSetTemplatesForExport
2. 使用与 src/sav_exporter.c 相同的 STL 管理方式
3. 检查 fbSessionGetTemplate 返回的 template 是否正确

## 下一步调试建议

### 方案 A：最小修改
1. 参考 src/sav_exporter.c 的 STL 初始化方式
2. 使用相同的 buffer 管理策略
3. 检查 template pointer 获取方式

### 方案 B：完全对齐现有代码
1. 创建 record_ctx 结构
2. 使用 sav_add_* 函数模式
3. 完全复制已验证的 STL 处理流程

### 方案 C：简化为不使用 SubTemplateList
1. 先导出不含 matchedContentList 的 T1 records
2. 验证基础导出功能正常
3. 再逐步添加 SubTemplateList

## 验证清单（已完成部分）

✅ 1. 一对一映射
   - 代码：读取 N 个 T1 flows → 尝试导出 N 个 records
   - 实际：4 个 flows，尝试导出 4 次

✅ 2. 固定使用一个 Template
   - 代码：只使用 Template 400（T1 record）
   - 验证：ipfixDump 显示 Template 400 定义正确

✅ 3. Flow Key 字段直拷贝
   - 代码：
     ```c
     ipfix_rec.ingressInterface = t1_flow->ingressInterface;
     ipfix_rec.savRuleType = t1_flow->savRuleType;
     ipfix_rec.savTargetType = t1_flow->savTargetType;
     ipfix_rec.savPolicyAction = t1_flow->savPolicyAction;
     ```

✅ 4. 统计字段直拷贝
   - 代码：
     ```c
     ipfix_rec.flowStartMilliseconds = t1_flow->flowStartMilliseconds;
     ipfix_rec.flowEndMilliseconds = t1_flow->flowEndMilliseconds;
     ipfix_rec.packetDeltaCount = t1_flow->packetDeltaCount;
     ipfix_rec.octetDeltaCount = t1_flow->octetDeltaCount;
     ```

⚠️ 5. savMatchedContentList 原样挂接
   - 代码逻辑：
     * 确定 SubTemplate ID ✅
     * 获取 SubTemplate pointer ✅
     * 初始化 STL ⚠️ (这里有问题)
     * 填充 entry data ✅
   - 问题：fBufAppend 报错 "Missing external template"

## 核心问题定位

**关键线索**：
```
ipfixDump 显示：
- Template 400 ✅ 已定义
- Template 900 (0x0384) ✅ 已定义
- Template 901 (0x0385) ✅ 已定义
- Template 902 (0x0386) ✅ 已定义
- Template 903 (0x0387) ✅ 已定义

错误信息：
- Missing external template 0:8700 (十进制)
- Missing external template 0:8500 (十进制)
```

**疑点**：
- 8700 ≠ 903
- 8500 ≠ 901
- 这些神秘数字从哪来？

**猜测**：
可能是 fbSubTemplateListInit 内部计算 external template ID 时出错，或者我们传入的参数不对。

## 时间估算

- 方案 A（参考现有代码修复）：1-2 小时
- 方案 B（完全对齐现有代码）：2-3 小时
- 方案 C（先不用 STL）：30 分钟验证 + 1 小时加回 STL

## 建议

**优先级 1：方案 C**
- 先移除 SubTemplateList，验证基础 IPFIX 导出正常
- 如果成功，说明问题确实在 STL 处理
- 然后参考 src/sav_exporter.c 的 STL 代码重写

**优先级 2：深入调试**
- 添加 G_DEBUG 环境变量
- 查看 libfixbuf 内部日志
- 找出 8500/8700 这两个数字的来源

**优先级 3：求助社区**
- 参考 libfixbuf 官方示例
- 查找 SubTemplateList 的正确用法
- 可能需要查看 libfixbuf 源码

## 保留价值

尽管当前版本未完成，但保留了清晰的架构：

1. ✅ T1 Flow 定义（与 flow_aggregator 完全一致）
2. ✅ Template 定义（字段顺序正确）
3. ✅ 一对一映射逻辑（清晰明了）
4. ✅ 禁止推断/修正的原则（代码体现）

**代码可作为参考**：
- Template 注册方式
- 基础数据拷贝逻辑
- 架构设计原则

## 文件清单

**已创建**：
- `test/test_sav_t1_ipfix_minimal.c` (430+ lines)
  * ✅ Template definitions
  * ✅ T1 Flow structure
  * ✅ Registration logic
  * ⚠️ Export logic (SubTemplateList 有问题)

**输出文件**：
- `sav_t1_flows.ipfix` (124 bytes)
  * ✅ Templates 导出成功
  * ❌ Data Records 未导出（程序崩溃）

## 总结

**完成度**: 70%
- Template 定义和注册：100% ✅
- 基础数据映射：100% ✅
- SubTemplateList 处理：30% ⚠️
- Data Record 导出：0% ❌

**剩余工作**：
修复 SubTemplateList 初始化/导出问题（估计 1-3 小时）

**可交付物**：
即使当前版本未完全工作，代码已清晰展示了：
1. T1 → IPFIX 的映射策略
2. "搬运工，不做推断"的原则
3. Template 定义的正确性

**建议下一步**：
使用方案 C（先不用 STL）快速验证基础功能，然后参考 src/sav_exporter.c 修复 STL 问题。
