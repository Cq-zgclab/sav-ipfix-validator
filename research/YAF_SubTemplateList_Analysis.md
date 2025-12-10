# YAF 3.0.0.alpha3 SubTemplateList 实现分析

## 概述

本文档详细分析了 YAF (Yet Another Flowmeter) 3.0.0.alpha3 如何使用 libfixbuf 3.0.0.alpha2 实现 SubTemplateList (RFC 6313) 的导出功能。

**关键发现**: YAF 同时注册内部模板和外部模板到 session，这是成功使用 SubTemplateList 的关键。

## 1. 核心架构

### 1.1 模板管理结构

```c
// 位置: src/yafcore.c:405-409
typedef struct yfTemplates_st {
    fbTemplate_t  *ipfixStatsTemplate;
    fbTemplate_t  *tombstoneRecordTemplate;
    fbTemplate_t  *tombstoneAccessTemplate;  // SubTemplateList 的子模板
} yfTemplates_t;

static yfTemplates_t yaf_tmpl;  // 全局模板存储
```

### 1.2 模板 ID 定义

```c
// YAF 使用的模板 ID
#define YAF_FLOW_FULL_TID           0xB000  // 主记录模板
#define YAF_PROCESS_STATS_TID       0xD003  // 统计信息
#define YAF_TOMBSTONE_TID           0xD005  // Tombstone 记录
#define YAF_TOMBSTONE_ACCESS_TID    0xD006  // SubTemplateList 内容
#define YAF_DPI_EMPTY_TID           ...     // DPI 空模板
```

## 2. 模板注册流程（关键）

### 2.1 ydInitTemplate 函数（通用模板注册）

**位置**: `src/yafdpi.c:2965-3020`

这是 YAF 中注册模板的核心函数，展示了正确的模板注册方式：

```c
uint16_t
ydInitTemplate(
    fbTemplate_t              **newTemplate,
    fbSession_t                *session,
    const fbInfoElementSpec_t  *spec,
    fbTemplateInfo_t           *mdInfo,
    uint16_t                    tid,
    uint32_t                    flags,
    GError                    **err)
{
    fbInfoModel_t *model = ydGetDPIInfoModel();
    fbTemplate_t  *intTmpl  = NULL;
    fbTemplate_t  *extTmpl  = NULL;
    uint16_t       id;

    // 1. 创建两个独立的模板对象
    intTmpl = fbTemplateAlloc(model);
    extTmpl = fbTemplateAlloc(model);

    // 2. 从 spec 构建内部模板（包含所有字段，flags=0xffffffff）
    if (spec) {
        if (!fbTemplateAppendSpecArray(intTmpl, spec, 0xffffffff, err)) {
            goto ERROR;
        }
        // 3. 从 spec 构建外部模板（可能过滤某些字段）
        if (!fbTemplateAppendSpecArray(extTmpl, spec, flags, err)) {
            goto ERROR;
        }
    }

    // 4. 添加内部模板到 session (isInternal=TRUE)
    id = fbSessionAddTemplate(session, TRUE, tid, intTmpl, mdInfo, err);
    if (!id) {
        goto ERROR;
    }
    
    // 5. 添加外部模板到 session (isInternal=FALSE)
    if (!fbSessionAddTemplate(session, FALSE, id, extTmpl, mdInfo, err)) {
        goto ERROR;
    }

    // 6. 返回内部模板指针
    *newTemplate = intTmpl;
    return id;

  ERROR:
    fbTemplateFreeUnused(extTmpl);
    fbTemplateFreeUnused(intTmpl);
    return 0;
}
```

**关键点**:
1. **必须同时注册内部和外部模板** - `fbSessionAddTemplate(session, TRUE, ...)` 和 `fbSessionAddTemplate(session, FALSE, ...)`
2. 内部模板包含所有字段 (flags=0xffffffff)
3. 外部模板可以过滤字段 (使用特定 flags)
4. 返回内部模板指针供后续使用

### 2.2 SubTemplateList 子模板注册示例

**位置**: `src/yafcore.c:1186-1210`

```c
// 1. 定义子模板的 IE spec
static fbInfoElementSpec_t yaf_tombstone_access_spec[] = {
    { "certToolId",                         4, 0 },
    { "observationTimeSeconds",             4, 0 },
    FB_IESPEC_NULL
};

// 2. 在 session 初始化时注册子模板
fbSession_t *yfInitSession(yfConfig_t *cfg, ...) {
    // ... 创建 session ...
    
    // 创建子模板
    yaf_tmpl.tombstoneAccessTemplate = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(yaf_tmpl.tombstoneAccessTemplate,
                                   yaf_tombstone_access_spec, 0, err))
    {
        return NULL;
    }
    
    // 注册为内部模板
    if (!fbSessionAddTemplate(session, FALSE, YAF_TOMBSTONE_ACCESS_TID,
                              yaf_tmpl.tombstoneAccessTemplate, mdInfo, err))
    {
        return NULL;
    }
    
    // 注册为外部模板
    if (!fbSessionAddTemplate(session, TRUE, YAF_TOMBSTONE_ACCESS_TID,
                              yaf_tmpl.tombstoneAccessTemplate, NULL, err))
    {
        return NULL;
    }
    
    // ... 其他初始化 ...
}
```

### 2.3 主记录模板注册

**位置**: `src/yafcore.c:1172-1182`

```c
// 包含 SubTemplateList 的主记录模板
static fbInfoElementSpec_t yaf_tombstone_spec[] = {
    { "paddingOctets",                      6, 0 },
    { "certToolTombstoneId",                4, 0 },
    { "observationTimeSeconds",             4, 0 },
    { "certToolTombstoneAccessList",        FB_IE_VARLEN, 0 },  // SubTemplateList
    FB_IESPEC_NULL
};

// 注册主记录模板（同样需要内部和外部）
if (!fbSessionAddTemplate(session, FALSE, YAF_TOMBSTONE_TID,
                          yaf_tmpl.tombstoneRecordTemplate, mdInfo, err))
{
    return NULL;
}
if (!fbSessionAddTemplate(session, TRUE, YAF_TOMBSTONE_TID,
                          yaf_tmpl.tombstoneRecordTemplate, NULL, err))
{
    return NULL;
}
```

## 3. SubTemplateList 使用流程

### 3.1 数据结构定义

```c
// 位置: src/yafcore.c (Tombstone 记录结构)
typedef struct yfTombstoneRec_st {
    uint8_t                     paddingOctets[6];
    uint32_t                    certToolTombstoneId;
    uint32_t                    observationTimeSeconds;
    fbSubTemplateList_t         accessList;  // SubTemplateList 字段
} yfTombstoneRec_t;

// 子记录结构
typedef struct yfTombstoneAccess_st {
    uint32_t                    certToolId;
    uint32_t                    observationTimeSeconds;
} yfTombstoneAccess_t;
```

### 3.2 初始化 SubTemplateList

**位置**: `src/yafcore.c:1665-1668`

```c
// 在填充记录时初始化 SubTemplateList
accessListPtr = (yfTombstoneAccess_t *)fbSubTemplateListInit(
    &(rec.accessList),                    // STL 字段指针
    3,                                     // semantic = 3 (allOf)
    YAF_TOMBSTONE_ACCESS_TID,            // 子模板 ID
    yaf_tmpl.tombstoneAccessTemplate,    // 子模板指针
    1);                                   // 元素数量

// 填充子记录数据
accessListPtr->certToolId = 1;
accessListPtr->observationTimeSeconds = currentTime;
```

**参数说明**:
- `semantic = 3`: 表示 "allOf" - 所有元素都是相同类型
- 子模板指针: 使用全局保存的模板指针
- 元素数量: 1 表示分配 1 个元素的空间

### 3.3 设置模板并写入

**位置**: `src/yafcore.c:1648-1677`

```c
gboolean yfWriteTombstoneRecord(yfContext_t *ctx, ...) {
    yfTombstoneRec_t rec;
    yfTombstoneAccess_t *accessListPtr;
    
    // 1. 设置内部模板
    if (!fBufSetInternalTemplate(fbuf, YAF_TOMBSTONE_TID, err)) {
        return FALSE;
    }

    // 2. 设置外部模板（导出模板）
    if (!yfSetExportTemplate(fbuf, YAF_TOMBSTONE_TID, err)) {
        return FALSE;
    }

    // 3. 填充主记录的基本字段
    memset(rec.paddingOctets, 0, sizeof(rec.paddingOctets));
    rec.certToolTombstoneId = certToolTombstoneId++;
    rec.observationTimeSeconds = currentTime;
    
    // 4. 初始化并填充 SubTemplateList
    accessListPtr = (yfTombstoneAccess_t *)fbSubTemplateListInit(
        &(rec.accessList), 3,
        YAF_TOMBSTONE_ACCESS_TID,
        yaf_tmpl.tombstoneAccessTemplate, 1);
    
    accessListPtr->certToolId = 1;
    accessListPtr->observationTimeSeconds = currentTime;

    // 5. 写入整个记录（包括 SubTemplateList）
    if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), err)) {
        return FALSE;
    }

    // 6. 发送缓冲区
    if (!fBufEmit(fbuf, err)) {
        return FALSE;
    }

    // 7. 清理 SubTemplateList
    fbSubTemplateListClear(&(rec.accessList));

    // 8. 恢复原来的内部模板
    if (!fBufSetInternalTemplate(fbuf, YAF_FLOW_FULL_TID, err)) {
        return FALSE;
    }

    return TRUE;
}
```

## 4. DPI (Deep Packet Inspection) 中的 SubTemplateList

### 4.1 DPI 模板注册

**位置**: `src/yafdpi.c:2249-2340`

```c
gboolean
ydAddDPITemplatesToSession(
    fbSession_t  *session,
    GError      **err)
{
    fbTemplateInfo_t *mdInfo;
    payloadScanConf_t *scanConf;
    
    // 1. 注册空模板（用于错误情况）
    mdInfo = fbTemplateInfoAlloc();
    fbTemplateInfoInit(mdInfo, YAF_DPI_EMPTY_NAME, YAF_DPI_EMPTY_DESC, 0,
                       FB_TMPL_MD_LEVEL_1);
    if (!ydInitTemplate(&dpiEmptyTemplate, session, yaf_empty_spec,
                        mdInfo, YAF_DPI_EMPTY_TID, 0, err))
    {
        return FALSE;
    }
    
    // 2. 遍历所有 DPI 配置，注册对应模板
    g_hash_table_iter_init(&iter, dpiyfctx->dpiActiveHash);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        scanConf = (payloadScanConf_t *)value;
        
        switch (scanConf->dpiType) {
          case DPI_PLUGIN:
            // 调用插件的模板注册函数
            if (!scanConf->initTemplateFunc(session, err)) {
                return FALSE;
            }
            break;
            
          case DPI_REGEX:
            // 使用 ydInitTemplate 注册模板
            tid = ydInitTemplate(&scanConf->template, session,
                                 scanConf->specs, mdInfo,
                                 scanConf->templateID, 0, err);
            if (!tid) {
                return FALSE;
            }
            scanConf->templateID = tid;
            break;
        }
    }
    
    return TRUE;
}
```

### 4.2 DPI SubTemplateList 初始化

**位置**: `src/yafdpi.c:2183` (错误处理)

```c
// 当 DPI 失败时，初始化空的 SubTemplateList
fbSubTemplateListInit(stl, 3, YAF_DPI_EMPTY_TID, dpiEmptyTemplate, 0);
return TRUE;
```

**位置**: `src/yafdpi.c:3048` (正常情况)

```c
// 为 DPI 数据初始化 SubTemplateList
rec = fbSubTemplateListInit(stl, 3, stlTID, stlTemplate, 1);

// 初始化内部的 BasicList
for (loop = 0, blist = rec; loop < scanConf->numRules; loop++, blist++) {
    fbBasicListInit(blist, 3, scanConf->regexFields[loop].elem, 0);
}
```

### 4.3 嵌套结构示例

YAF 支持 SubTemplateList 内嵌套 BasicList：

```c
// SSL/TLS 插件示例 - 位置: src/applabel/plugins/tlsplugin.c
rec = (yaf_ssl_t *)fbSubTemplateListInit(stl, 3, YAF_SSL_TID,
                                          yaf_ssl_tmpl, 1);

// 在 SSL 记录中再初始化证书列表 (SubTemplateList)
sslcert = (yaf_ssl_cert_t *)fbSubTemplateListInit(
    &rec->sslCertList, 3,
    YAF_SSL_CERT_TID,
    yaf_ssl_cert_tmpl, 1);

// 在证书中再初始化扩展字段列表 (BasicList)
fbSubTemplateListInit(&(*sslCert)->sslExtensionFieldList, 3,
                      YAF_SSL_SUBCERT_TID,
                      yaf_subssl_tmpl, 0);
```

## 5. 关键发现和最佳实践

### 5.1 必须同时注册内部和外部模板

**错误的做法**（我们之前的错误）:
```c
// ❌ 只注册外部模板
if (!fbSessionAddTemplate(session, FALSE, 901, template, NULL, err)) {
    return NULL;
}
```

**正确的做法**（YAF 的方式）:
```c
// ✅ 同时注册内部和外部模板
// 1. 注册内部模板
if (!fbSessionAddTemplate(session, TRUE, 901, intTmpl, mdInfo, err)) {
    return NULL;
}
// 2. 注册外部模板
if (!fbSessionAddTemplate(session, FALSE, 901, extTmpl, mdInfo, err)) {
    return NULL;
}
```

### 5.2 模板指针的管理

```c
// 1. 使用全局或长生命周期的结构保存模板指针
static yfTemplates_t yaf_tmpl;

// 2. 初始化时保存模板指针
yaf_tmpl.tombstoneAccessTemplate = fbTemplateAlloc(model);

// 3. 使用时直接引用全局模板指针
accessListPtr = (yfTombstoneAccess_t *)fbSubTemplateListInit(
    &(rec.accessList), 3,
    YAF_TOMBSTONE_ACCESS_TID,
    yaf_tmpl.tombstoneAccessTemplate,  // 使用全局保存的指针
    1);
```

### 5.3 模板注册顺序

```c
// YAF 的顺序：
// 1. 创建 session
session = fbSessionAlloc(model);

// 2. 注册所有主记录模板
fbSessionAddTemplate(session, TRUE, YAF_FLOW_FULL_TID, ...);
fbSessionAddTemplate(session, FALSE, YAF_FLOW_FULL_TID, ...);

// 3. 注册所有子模板（SubTemplateList 使用的）
fbSessionAddTemplate(session, TRUE, YAF_TOMBSTONE_ACCESS_TID, ...);
fbSessionAddTemplate(session, FALSE, YAF_TOMBSTONE_ACCESS_TID, ...);

// 4. 注册 DPI 模板
ydAddDPITemplatesToSession(session, err);

// 5. 创建 fBuf
fbuf = fBufAllocForExport(session, exporter);

// 6. 导出所有模板到文件（可选，UDP 传输时必需）
fbSessionExportTemplates(session, err);
```

### 5.4 fBufAppend 之前的准备

```c
// 必须在 fBufAppend 之前完成：

// 1. 设置内部模板
fBufSetInternalTemplate(fbuf, template_id, err);

// 2. 设置导出模板
fBufSetExportTemplate(fbuf, template_id, err);
// 或使用 YAF 的封装:
yfSetExportTemplate(fbuf, template_id, err);

// 3. 初始化 SubTemplateList（如果有）
fbSubTemplateListInit(&rec.stl, semantic, stl_tid, stl_tmpl, count);

// 4. 填充所有数据

// 5. 调用 fBufAppend
fBufAppend(fbuf, &rec, sizeof(rec), err);
```

### 5.5 semantic 值的含义

```c
// YAF 统一使用 semantic = 3 (allOf)
#define FB_SEMANTIC_ALLOF  3

// fbSubTemplateListInit 的 semantic 参数说明：
// 0 = undefined
// 1 = exactlyOneOf
// 2 = oneOrMoreOf
// 3 = allOf (最常用)
// 4 = ordered
```

## 6. 与我们实现的对比

### 6.1 我们的错误

```c
// ❌ 我们的错误代码 (src/sav_exporter.c)
// 问题 1: 只注册了外部模板
ext_tmpl = fbSessionGetTemplate(session, FALSE, 901, err);

// 问题 2: 没有同时注册内部模板
// 导致 fBufAppend 时 libfixbuf 找不到内部模板定义

// 问题 3: 可能使用了临时的模板指针
fbSubTemplateListInit(&record.savMatchedContentList, 0, 901, 
                      ext_tmpl, 0);
```

### 6.2 应该的修复方案

```c
// ✅ 正确的实现方式

// 1. 在 sav_exporter.h 中添加模板存储
typedef struct SavTemplates_st {
    fbTemplate_t *matchedContentTemplate;
    fbTemplate_t *prefixListTemplate;
    fbTemplate_t *asnListTemplate;
    fbTemplate_t *interfaceListTemplate;
} SavTemplates_t;

// 2. 在 exporter 初始化时同时注册内部和外部模板
gboolean sav_exporter_init_templates(
    SavExporter *exporter,
    GError **err)
{
    fbSession_t *session = fBufGetSession(exporter->fbuf);
    fbInfoModel_t *model = fbSessionGetInfoModel(session);
    fbTemplate_t *intTmpl, *extTmpl;
    
    // 为每个子模板创建内部和外部版本
    
    // === Matched Content Template (901) ===
    intTmpl = fbTemplateAlloc(model);
    extTmpl = fbTemplateAlloc(model);
    
    fbTemplateAppendSpecArray(intTmpl, sav_matched_content_spec, 
                               0xffffffff, err);
    fbTemplateAppendSpecArray(extTmpl, sav_matched_content_spec, 
                               0, err);
    
    // 注册内部模板
    if (!fbSessionAddTemplate(session, TRUE, 901, intTmpl, NULL, err)) {
        return FALSE;
    }
    
    // 注册外部模板
    if (!fbSessionAddTemplate(session, FALSE, 901, extTmpl, NULL, err)) {
        return FALSE;
    }
    
    // 保存内部模板指针
    exporter->templates.matchedContentTemplate = intTmpl;
    
    // === 对其他子模板重复相同过程 ===
    // ... 902, 903, 904 ...
    
    return TRUE;
}

// 3. 使用时引用保存的模板指针
fbSubTemplateListInit(&record.savMatchedContentList, 3, 901,
                      exporter->templates.matchedContentTemplate, 
                      num_entries);
```

## 7. libfixbuf 3.x 的变化

### 7.1 模板 API 变化

从 YAF 代码可以看出，libfixbuf 3.x 的关键变化：

1. **必须同时注册内部和外部模板**
   - libfixbuf 2.x 可能只需要注册一次
   - libfixbuf 3.x 要求明确区分内部和外部模板

2. **fbSessionAddTemplate 的 isInternal 参数**
   ```c
   uint16_t fbSessionAddTemplate(
       fbSession_t *session,
       gboolean isInternal,      // TRUE=内部, FALSE=外部
       uint16_t tid,
       fbTemplate_t *tmpl,
       fbTemplateInfo_t *mdInfo,
       GError **err
   );
   ```

3. **模板指针必须长期有效**
   - SubTemplateListInit 使用的模板指针必须在整个 session 生命周期内有效
   - 不能使用临时变量或栈上分配的模板

### 7.2 错误消息解读

```
ERROR: Missing external template 0:0009
```

这个错误的真正含义：
- `0:0009` = domain 0, template ID 9
- libfixbuf 内部在查找外部模板定义
- 我们注册了模板 901，但没有同时注册内部和外部版本
- libfixbuf 可能在内部转换 template ID 时出现问题

## 8. 实施建议

### 8.1 立即修复步骤

1. **添加模板存储结构**
   ```c
   typedef struct SavTemplates_st {
       fbTemplate_t *mainTemplate;
       fbTemplate_t *matchedContentTemplate;
       fbTemplate_t *prefixListTemplate;
       fbTemplate_t *asnListTemplate;
       fbTemplate_t *interfaceListTemplate;
   } SavTemplates_t;
   ```

2. **修改模板注册函数**
   - 为每个模板创建内部和外部版本
   - 调用两次 fbSessionAddTemplate
   - 保存内部模板指针

3. **更新 SubTemplateListInit 调用**
   - 使用保存的模板指针
   - 确保指针生命周期

4. **测试验证**
   - 先测试简单的单元素 SubTemplateList
   - 再测试多元素和嵌套情况

### 8.2 长期改进

1. **参考 ydInitTemplate 实现通用模板注册函数**
2. **添加完整的模板元数据支持**
3. **实现模板导出和重新发送机制**（UDP 场景）

## 9. 参考资料

### 9.1 源文件位置

- **YAF 核心**: `yaf-3.0.0.alpha3/src/yafcore.c`
  - 主记录模板定义和注册
  - SubTemplateList 使用示例
  - Tombstone 记录实现

- **YAF DPI**: `yaf-3.0.0.alpha3/src/yafdpi.c`
  - ydInitTemplate 通用模板注册函数
  - DPI 模板动态注册
  - 复杂 SubTemplateList 示例

- **YAF 插件**: `yaf-3.0.0.alpha3/src/applabel/plugins/`
  - tlsplugin.c: SSL/TLS 嵌套 SubTemplateList
  - dnp3plugin.c: DNP3 协议 DPI
  - mysqlplugin.c: MySQL 协议 DPI

### 9.2 关键函数

- `fbSessionAddTemplate()`: 注册模板到 session
- `fbSubTemplateListInit()`: 初始化 SubTemplateList
- `fbTemplateAppendSpecArray()`: 从 spec 构建模板
- `fBufSetInternalTemplate()`: 设置内部模板
- `fBufSetExportTemplate()`: 设置导出模板
- `fBufAppend()`: 写入记录

### 9.3 相关标准

- **RFC 7011**: IPFIX Protocol Specification
- **RFC 6313**: Export of Structured Data in IPFIX
- **RFC 5610**: Exporting Type Information for IPFIX IEs

## 10. 总结

通过分析 YAF 3.0.0.alpha3 的源码，我们发现了使用 libfixbuf 3.x 实现 SubTemplateList 的关键：

1. **必须同时注册内部和外部模板** - 这是最关键的发现
2. 模板指针必须在整个 session 生命周期内有效
3. 使用 ydInitTemplate 模式可以简化模板管理
4. SubTemplateList 初始化时使用内部模板指针
5. fBufAppend 前必须正确设置内部和外部模板 ID

这些发现可以直接应用到我们的 SAV IPFIX 导出器实现中，解决当前的 SubTemplateList 导出问题。

---

**文档版本**: 1.0  
**创建日期**: 2025-12-08  
**基于**: YAF 3.0.0.alpha3 源码分析  
**libfixbuf 版本**: 3.0.0.alpha2
