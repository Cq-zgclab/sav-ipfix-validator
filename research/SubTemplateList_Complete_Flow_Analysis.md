# SubTemplateList 完整流程分析

## 文档目标
通过系统分析 libfixbuf、YAF 和 Super Mediator 的源码，完整理解 SubTemplateList 的正确使用方法。

## 1. libfixbuf 3.0.0.alpha2 核心 API

### 1.1 模板注册 API

#### fbSessionAddTemplate() - 基础 API
```c
uint16_t fbSessionAddTemplate(
    fbSession_t *session,
    gboolean isInternal,    // TRUE=内部模板, FALSE=外部模板
    uint16_t tid,
    fbTemplate_t *tmpl,
    fbTemplateInfo_t *mdInfo,
    GError **err
);
```

#### fbSessionAddTemplatesForExport() - 3.0新增便利API
```c
// 官方sample_exporter.c中使用的方法
// 自动注册内部和外部模板
uint16_t fbSessionAddTemplatesForExport(
    fbSession_t *session,
    uint16_t tid,
    fbTemplate_t *tmpl,
    fbTemplateInfo_t *tmplInfo,
    GError **err
);
```

**说明**：
- 这个函数会自动处理 paddingOctets
- 先注册外部模板（去除padding）
- 再注册内部模板（完整版本）
- **libfixbuf 3.x 推荐使用此方法**

#### fbSessionAddTemplatePair() - SubTemplateList关键API
```c
void fbSessionAddTemplatePair(
    fbSession_t *session,
    uint16_t     external_tid,
    uint16_t     internal_tid
);
```

**作用**：告诉 libfixbuf 当读取 SubTemplateList 中的外部模板时，应该使用哪个内部模板来解析数据。

**重要性**：
- YAF 使用此API（在collector端）
- Super Mediator 使用此API（在DPI模板处理中）
- **可能是 SubTemplateList 导出的关键！**

### 1.2 SubTemplateList API

#### fbSubTemplateListInit()
```c
void* fbSubTemplateListInit(
    fbSubTemplateList_t *stl,
    uint8_t semantic,        // 3 = allOf (YAF统一使用)
    uint16_t tmpl_id,        // 子模板ID
    fbTemplate_t *tmpl,      // 子模板指针 (必须是内部模板)
    uint16_t count           // 元素数量
);
```

**返回值**：指向预分配数据缓冲区的指针（如果count > 0）

### 1.3 Buffer 设置 API

#### fBufSetTemplatesForExport() - 3.0新增
```c
gboolean fBufSetTemplatesForExport(
    fBuf_t *fbuf,
    uint16_t tid,
    GError **err
);
```

**作用**：同时设置内部和外部模板（3.0新API）

## 2. YAF 3.0.0.alpha3 实现分析

### 2.1 模板注册流程（yafcore.c）

#### Tombstone 模板注册示例（行号：1186-1210）
```c
// 1. 创建子模板（accessList）
yaf_tmpl.tombstoneAccessTemplate = fbTemplateAlloc(model);
fbTemplateAppendSpecArray(yaf_tmpl.tombstoneAccessTemplate,
                         yaf_tombstone_access_spec, 0, err);

// 2. 注册为外部模板（FALSE）
fbSessionAddTemplate(session, FALSE, YAF_TOMBSTONE_ACCESS_TID,
                    yaf_tmpl.tombstoneAccessTemplate, mdInfo, err);

// 3. 注册为内部模板（TRUE）- 使用同一个模板对象！
fbSessionAddTemplate(session, TRUE, YAF_TOMBSTONE_ACCESS_TID,
                    yaf_tmpl.tombstoneAccessTemplate, NULL, err);

// 4. 注册主模板（包含SubTemplateList字段）
yaf_tmpl.tombstoneRecordTemplate = fbTemplateAlloc(model);
fbTemplateAppendSpecArray(yaf_tmpl.tombstoneRecordTemplate,
                         yaf_tombstone_spec, 0, err);
fbSessionAddTemplate(session, FALSE, YAF_TOMBSTONE_TID,
                    yaf_tmpl.tombstoneRecordTemplate, mdInfo, err);
fbSessionAddTemplate(session, TRUE, YAF_TOMBSTONE_TID,
                    yaf_tmpl.tombstoneRecordTemplate, NULL, err);
```

**关键观察**：
1. YAF 使用 **同一个模板对象** 注册两次（内部和外部）
2. 先注册 FALSE（外部），再注册 TRUE（内部）
3. 保存模板指针到全局结构 `yaf_tmpl`

### 2.2 SubTemplateList 使用流程（yafcore.c: 1640-1680）

```c
gboolean yfWriteTombstoneRecord(yfContext_t *ctx, ...)
{
    yfTombstoneRec_t rec;
    yfTombstoneAccess_t *accessListPtr;
    
    // 1. 设置内部模板
    fBufSetInternalTemplate(fbuf, YAF_TOMBSTONE_TID, err);
    
    // 2. 设置导出模板
    yfSetExportTemplate(fbuf, YAF_TOMBSTONE_TID, err);
    
    // 3. 填充基本字段
    rec.certToolTombstoneId = certToolTombstoneId++;
    rec.observationTimeSeconds = currentTime;
    
    // 4. 初始化 SubTemplateList
    accessListPtr = (yfTombstoneAccess_t *)fbSubTemplateListInit(
        &(rec.accessList), 
        3,                                      // semantic = allOf
        YAF_TOMBSTONE_ACCESS_TID,              // 子模板ID
        yaf_tmpl.tombstoneAccessTemplate,      // 使用全局保存的模板指针
        1);                                    // 1个元素
    
    // 5. 填充子记录数据
    accessListPtr->certToolId = 1;
    accessListPtr->observationTimeSeconds = currentTime;
    
    // 6. 导出记录
    fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err);
    
    // 7. Emit
    fBufEmit(fbuf, err);
    
    // 8. 清理
    fbSubTemplateListClear(&(rec.accessList));
    
    // 9. 恢复原模板
    fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err);
    
    return TRUE;
}
```

### 2.3 DPI 模板注册（yafdpi.c: 2965-3020）

#### ydInitTemplate() - YAF的通用模板注册函数
```c
uint16_t ydInitTemplate(
    fbTemplate_t **newTemplate,
    fbSession_t *session,
    const fbInfoElementSpec_t *spec,
    fbTemplateInfo_t *mdInfo,
    uint16_t tid,
    uint32_t flags,
    GError **err)
{
    fbTemplate_t *intTmpl = NULL;
    fbTemplate_t *extTmpl = NULL;
    uint16_t id;
    
    // 1. 创建两个独立的模板对象
    intTmpl = fbTemplateAlloc(model);
    extTmpl = fbTemplateAlloc(model);
    
    // 2. 从spec构建内部模板（包含所有字段）
    fbTemplateAppendSpecArray(intTmpl, spec, 0xffffffff, err);
    
    // 3. 从spec构建外部模板（可能过滤某些字段）
    fbTemplateAppendSpecArray(extTmpl, spec, flags, err);
    
    // 4. 注册内部模板
    id = fbSessionAddTemplate(session, TRUE, tid, intTmpl, mdInfo, err);
    
    // 5. 注册外部模板
    fbSessionAddTemplate(session, FALSE, id, extTmpl, mdInfo, err);
    
    // 6. 返回内部模板指针
    *newTemplate = intTmpl;
    return id;
}
```

**观察**：
- DPI 使用了 **两个独立的模板对象**（与 Tombstone 不同！）
- 内部模板：flags = 0xffffffff（所有字段）
- 外部模板：flags = 0 或特定值（可能过滤字段）

### 2.4 Collector端的Template Pair（yafcore.c: 2203-2221）

```c
static void yfTemplateCallback(
    fbSession_t *session,
    uint16_t tid,
    fbTemplate_t *tmpl,
    void *app_ctx,
    void **tmpl_ctx,
    fbTemplateCtxFree_fn *fn)
{
    // 如果是flow基础模板
    if (YAF_FLOW_BASE_TID == (tid & 0xF000)) {
        fbSessionAddTemplatePair(session, tid, tid);
    }
    
    // 对所有模板设置pair (external -> internal=0)
    fbSessionAddTemplatePair(session, tid, 0);
}
```

**说明**：
- 这是在 **collector端** 的回调函数
- `fbSessionAddTemplatePair(session, tid, 0)` 意味着外部模板tid映射到内部模板0？
- 可能是告诉libfixbuf使用默认的内部模板

## 3. Super Mediator 2.0.0.alpha1 实现分析

### 3.1 模板配对使用（mediator_core.c）

#### DPI模板处理（行号：748-757）
```c
case TC_DPI:
    // 1. 复制并添加模板到session
    copyTemplateAddToSesssion(
        session, &intTmpl, &intTid, extTmpl, extTid, msgPref);
    
    tmplForExp = intTmpl;
    
    // 2. DPI模板需要添加template pair！
    fbSessionAddTemplatePair(session, extTid, intTid);
    
    // 3. 设置上下文
    defIntTmplCtx = g_slice_new0(mdDefaultTmplCtx_t);
    // ...
```

**关键发现**：
- Super Mediator 在处理 DPI（包含SubTemplateList）时 **明确调用** `fbSessionAddTemplatePair`
- 将外部模板ID映射到内部模板ID

### 3.2 SubTemplateList 初始化（mediator_export.c: 4889）

```c
fbSubTemplateListInit(recStl, 
                     fbSubTemplateListGetSemantic(tmpStl),
                     MD_SSL_CERTIFICATE_TID, 
                     level2State->sslFlatTmpl,
                     fbSubTemplateListCountElements(tmpStl));
```

## 4. 关键差异总结

### 4.1 模板注册模式对比

| 项目 | YAF Tombstone | YAF DPI | Super Mediator | libfixbuf sample |
|------|--------------|---------|----------------|------------------|
| 内部模板对象 | 1个共享 | 2个独立 | ? | 1个 |
| 外部模板对象 | 同上 | 独立 | ? | 自动处理 |
| 注册顺序 | FALSE->TRUE | TRUE->FALSE | ? | 自动 |
| Template Pair | 否（exporter） | 否（exporter） | 是（DPI） | 否 |
| 使用API | fbSessionAddTemplate | fbSessionAddTemplate | fbSessionAddTemplate + fbSessionAddTemplatePair | fbSessionAddTemplatesForExport |

### 4.2 我们的实现对比

| 特性 | 当前实现 | YAF | libfixbuf sample |
|------|---------|-----|------------------|
| 模板注册API | fbSessionAddTemplatesForExport ✓ | fbSessionAddTemplate | fbSessionAddTemplatesForExport |
| Template Pair | **未使用** ❌ | collector端使用 | 不需要 |
| 模板指针管理 | 临时（每次调用fbSessionGetTemplate） ❌ | 全局保存 ✓ | ? |
| semantic值 | 3 ✓ | 3 ✓ | - |
| fBufSetTemplates | fBufSetTemplatesForExport ✓ | 分别设置 | fBufSetTemplatesForExport |

## 5. 可能的问题分析

### 5.1 "Missing external template 0:0009" 错误

**错误格式**：`%#10x:%04hx` = `domain:tid`
- `0:0009` = domain 0, template ID 9
- 我们注册的是 901 (0x0385)，不是 9 (0x0009)

**可能原因**：
1. ❌ 模板ID混淆 - 但我们明确使用901
2. ❌ 域(domain)问题 - 默认应该是0
3. ⚠️ **Template Pair 缺失？** - Super Mediator在DPI中使用了这个！
4. ⚠️ **模板指针生命周期** - 我们用临时指针，YAF用全局保存的指针
5. ⚠️ **SubTemplateList内部引用** - libfixbuf可能需要某种映射

### 5.2 数据记录未导出

观察：
- ✅ 文件生成（144字节）
- ✅ 模板都存在（901-904, 400）
- ❌ 0 Data Records
- ❌ fBufAppend报错

**分析**：
- fBufAppend **在写入前** 就失败了
- 说明在序列化或验证阶段出错
- 可能是SubTemplateList引用的模板无法解析

## 6. 待验证的假设

### 假设1：需要 Template Pair（高优先级）
```c
// 在注册模板后添加：
fbSessionAddTemplatePair(session, 901, 901);  // external=901 -> internal=901
fbSessionAddTemplatePair(session, 902, 902);
fbSessionAddTemplatePair(session, 903, 903);
fbSessionAddTemplatePair(session, 904, 904);
```

**依据**：
- Super Mediator 在DPI模板中使用
- YAF collector端使用
- 可能告诉libfixbuf如何解析SubTemplateList中的引用

### 假设2：模板指针必须长期有效
```c
// 当前问题：每次都临时获取
ctx->sub_tmpl = fbSessionGetTemplate(session, TRUE, tmpl_id, err);

// YAF方式：全局保存
static yfTemplates_t yaf_tmpl;  // 全局
yaf_tmpl.tombstoneAccessTemplate = fbTemplateAlloc(model);
// ... 注册后保存指针，后续直接使用
```

### 假设3：需要先导出子模板到文件
```c
// 可能需要确保子模板已经在文件中
// 在导出数据记录之前
```

## 7. 下一步行动计划

### 优先级1：添加 Template Pair
1. 在 `sav_add_templates` 注册后添加 `fbSessionAddTemplatePair`
2. 测试是否解决 "0:0009" 错误

### 优先级2：修改模板指针管理
1. 在 SavExporter 中保存模板指针
2. 不要每次都临时获取

### 优先级3：研究错误"0:0009"的真正来源
1. 在libfixbuf源码中追踪这个错误
2. 理解为什么会查找template ID 9

### 优先级4：检查文件内容
1. 用hexdump详细分析生成的144字节
2. 对比YAF生成的有效文件

## 8. 参考文件位置

- libfixbuf: `/workspaces/sav-ipfix-validator/external-sources/libfixbuf-3.0.0.alpha2/`
- YAF: `/workspaces/sav-ipfix-validator/yaf-3.0.0.alpha3/`
- Super Mediator: `/workspaces/sav-ipfix-validator/external-sources/super_mediator-2.0.0.alpha1/`
- 研究笔记: `/workspaces/sav-ipfix-validator/research/`

---

**文档版本**: 1.0  
**创建时间**: 2025-12-08  
**状态**: 进行中 - 需要验证假设
