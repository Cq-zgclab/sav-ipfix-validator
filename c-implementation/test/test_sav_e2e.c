#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <fixbuf/public.h>

#define SAV_ENTERPRISE_ID 6871
#define IPFIX_FILE "test_sav_e2e.ipfix"

// 子记录结构
typedef struct {
    uint32_t ingressInterface;
    uint32_t sourceIPv4Prefix;
    uint8_t  sourceIPv4PrefixLength;
} SubRecord;

// 主记录结构 - 关键：SubTemplateList必须在最后，padding显式声明
typedef struct {
    uint64_t observationTimeMilliseconds;
    uint8_t  savRuleType;
    uint8_t  savTargetType;
    uint8_t  savPolicyAction;
    uint8_t  _padding[5];  // 显式padding对齐到16字节
    fbSubTemplateList_t subTemplateList;
} MainRecord;

// ========== EXPORTER ==========
int export_sav_records() {
    fbInfoElementSpec_t subSpec[] = {
        {"ingressInterface",        4, 0},
        {"sourceIPv4Prefix",        4, 0},
        {"sourceIPv4PrefixLength",  1, 0},
        FB_IESPEC_NULL
    };
    
    fbInfoElementSpec_t mainSpec[] = {
        {"observationTimeMilliseconds", 8, 0},
        {"savRuleType",                 1, 0},
        {"savTargetType",               1, 0},
        {"savPolicyAction",             1, 0},
        {"paddingOctets",               5, 0},  // 显式padding
        {"subTemplateList", FB_IE_VARLEN, 0},
        FB_IESPEC_NULL
    };
    
    fbInfoElement_t sav_ies[] = {
        FB_IE_INIT_FULL("savRuleType", SAV_ENTERPRISE_ID, 1, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV rule type"),
        FB_IE_INIT_FULL("savTargetType", SAV_ENTERPRISE_ID, 2, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV target type"),
        FB_IE_INIT_FULL("savPolicyAction", SAV_ENTERPRISE_ID, 4, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV policy action"),
        FB_IE_NULL
    };
    
    GError *err = NULL;
    fbInfoModel_t *model = fbInfoModelAlloc();
    fbInfoModelAddElementArray(model, sav_ies);
    fbSession_t *session = fbSessionAlloc(model);
    fbExporter_t *exporter = fbExporterAllocFile(IPFIX_FILE);
    fBuf_t *fbuf = fBufAllocForExport(session, exporter);
    
    printf("\n╔═════════════════════════════════════════════════════╗\n");
    printf("║              EXPORTER - Writing Records             ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n\n");
    
    // 注册子模板
    fbTemplate_t *subTmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(subTmpl, subSpec, ~0, &err);
    uint16_t subTid = fbSessionAddTemplatesForExport(session, 600, subTmpl, NULL, &err);
    if (!subTid) { fprintf(stderr, "✗ Sub-template: %s\n", err->message); return 1; }
    fbSessionAddTemplatePair(session, 600, 600);
    printf("[Export] Sub-template 600 registered\n");
    
    // 注册主模板
    fbTemplate_t *mainTmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(mainTmpl, mainSpec, ~0, &err);
    uint16_t mainTid = fbSessionAddTemplatesForExport(session, 700, mainTmpl, NULL, &err);
    if (!mainTid) { fprintf(stderr, "✗ Main template: %s\n", err->message); return 1; }
    fbSessionAddTemplatePair(session, 700, 700);
    printf("[Export] Main template 700 registered\n");
    
    if (!fBufSetTemplatesForExport(fbuf, mainTid, &err)) {
        fprintf(stderr, "✗ %s\n", err->message); return 1;
    }
    
    // 写入3条记录（每条包含不同数量的子记录）
    for (int i = 0; i < 3; i++) {
        MainRecord rec;
        memset(&rec, 0, sizeof(rec));
        rec.observationTimeMilliseconds = (time(NULL) + i) * 1000;
        rec.savRuleType = i % 2;
        rec.savTargetType = 0;
        rec.savPolicyAction = 1;
        
        fbTemplate_t *subTmplPtr = fbSessionGetTemplate(session, TRUE, 600, &err);
        
        // 第1条记录：1个子记录，第2条：2个，第3条：3个
        int num_sub = i + 1;
        SubRecord *subRec = (SubRecord *)fbSubTemplateListInit(
            &rec.subTemplateList, 3, 600, subTmplPtr, 1);
        
        if (!subRec) {
            fprintf(stderr, "✗ fbSubTemplateListInit failed\n");
            return 1;
        }
        
        // 填充第1个子记录
        subRec->ingressInterface = htonl(10 + i);
        uint32_t prefix = 0xC0000200 + (i * 0x100);
        subRec->sourceIPv4Prefix = htonl(prefix);
        subRec->sourceIPv4PrefixLength = 24;
        
        printf("[Export] Record %d: ruleType=%u, %d sub-record(s)\n",
               i+1, rec.savRuleType, num_sub);
        
        char ip_str[INET_ADDRSTRLEN];
        struct in_addr addr;
        addr.s_addr = htonl(prefix);
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        printf("           [0] Interface=%u, Prefix=%s/24\n", 10+i, ip_str);
        
        // 添加额外的子记录
        for (int j = 1; j < num_sub; j++) {
            subRec = (SubRecord *)fbSubTemplateListAddNewElements(&rec.subTemplateList, 1);
            if (!subRec) {
                fprintf(stderr, "✗ fbSubTemplateListAddNewElements failed\n");
                fbSubTemplateListClear(&rec.subTemplateList);
                return 1;
            }
            
            subRec->ingressInterface = htonl(20 + i + j);
            prefix = 0xC6000000 + (j * 0x10000);  // 198.0.x.0
            subRec->sourceIPv4Prefix = htonl(prefix);
            subRec->sourceIPv4PrefixLength = 22 + j;
            
            addr.s_addr = htonl(prefix);
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            printf("           [%d] Interface=%u, Prefix=%s/%u\n",
                   j, 20+i+j, ip_str, 22+j);
        }
        
        if (!fBufAppend(fbuf, (uint8_t *)&rec, sizeof(rec), &err)) {
            fprintf(stderr, "✗ fBufAppend: %s\n", err->message);
            fbSubTemplateListClear(&rec.subTemplateList);
            return 1;
        }
        
        fbSubTemplateListClear(&rec.subTemplateList);
    }
    
    if (!fBufEmit(fbuf, &err)) {
        fprintf(stderr, "✗ fBufEmit: %s\n", err->message);
        return 1;
    }
    
    fBufFree(fbuf);
    fbInfoModelFree(model);
    
    printf("\n✅ Exported 3 records to %s\n", IPFIX_FILE);
    return 0;
}

// ========== COLLECTOR ==========
int collect_sav_records() {
    GError *err = NULL;
    
    // 创建InfoModel（必须包含SAV IEs）
    fbInfoElement_t sav_ies[] = {
        FB_IE_INIT_FULL("savRuleType", SAV_ENTERPRISE_ID, 1, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV rule type"),
        FB_IE_INIT_FULL("savTargetType", SAV_ENTERPRISE_ID, 2, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV target type"),
        FB_IE_INIT_FULL("savPolicyAction", SAV_ENTERPRISE_ID, 4, 1,
                        FB_IE_F_ENDIAN | FB_IE_QUANTITY, 0, 0, FB_UINT_8, "SAV policy action"),
        FB_IE_NULL
    };
    
    fbInfoModel_t *model = fbInfoModelAlloc();
    fbInfoModelAddElementArray(model, sav_ies);
    
    fbSession_t *session = fbSessionAlloc(model);
    fbCollector_t *collector = fbCollectorAllocFile(NULL, IPFIX_FILE, &err);
    if (!collector) {
        fprintf(stderr, "✗ Collector alloc: %s\n", err->message);
        return 1;
    }
    
    fBuf_t *fbuf = fBufAllocForCollection(session, collector);
    
    printf("\n╔═════════════════════════════════════════════════════╗\n");
    printf("║             COLLECTOR - Reading Records             ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n\n");
    
    // 定义内部模板（与exporter相同）
    fbInfoElementSpec_t subSpec[] = {
        {"ingressInterface",        4, 0},
        {"sourceIPv4Prefix",        4, 0},
        {"sourceIPv4PrefixLength",  1, 0},
        FB_IESPEC_NULL
    };
    
    fbInfoElementSpec_t mainSpec[] = {
        {"observationTimeMilliseconds", 8, 0},
        {"savRuleType",                 1, 0},
        {"savTargetType",               1, 0},
        {"savPolicyAction",             1, 0},
        {"paddingOctets",               5, 0},
        {"subTemplateList", FB_IE_VARLEN, 0},
        FB_IESPEC_NULL
    };
    
    // 注册内部模板
    fbTemplate_t *subTmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(subTmpl, subSpec, ~0, &err);
    fbSessionAddTemplate(session, TRUE, 600, subTmpl, NULL, &err);
    
    fbTemplate_t *mainTmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(mainTmpl, mainSpec, ~0, &err);
    uint16_t mainTid = fbSessionAddTemplate(session, TRUE, 700, mainTmpl, NULL, &err);
    
    if (!fBufSetInternalTemplate(fbuf, mainTid, &err)) {
        fprintf(stderr, "✗ Set internal template: %s\n", err->message);
        return 1;
    }
    
    printf("[Collect] Internal templates registered and set\n\n");
    
    // 读取记录
    MainRecord rec;
    int count = 0;
    
    while (1) {
        memset(&rec, 0, sizeof(rec));
        size_t len = sizeof(rec);
        
        gboolean ret = fBufNext(fbuf, (uint8_t *)&rec, &len, &err);
        
        if (!ret) {
            if (g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
                g_clear_error(&err);
                break;
            }
            fprintf(stderr, "✗ fBufNext error: %s\n", err->message);
            return 1;
        }
        
        count++;
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Record #%d:\n", count);
        printf("  Timestamp: %lu\n", rec.observationTimeMilliseconds);
        printf("  SAV Rule Type: %u\n", rec.savRuleType);
        printf("  SAV Target Type: %u\n", rec.savTargetType);
        printf("  SAV Policy Action: %u\n", rec.savPolicyAction);
        printf("  SubTemplateList:\n");
        printf("    Template ID: %u\n", rec.subTemplateList.tmplID);
        printf("    Num Elements: %u\n", rec.subTemplateList.numElements);
        
        // 遍历子记录
        SubRecord *subRec = NULL;
        int subIdx = 0;
        while ((subRec = (SubRecord *)fbSubTemplateListGetNextPtr(
                   &rec.subTemplateList, subRec))) {
            char ip_str[INET_ADDRSTRLEN];
            struct in_addr addr;
            addr.s_addr = subRec->sourceIPv4Prefix;
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            
            printf("      [%d] Interface=%u, Prefix=%s/%u\n",
                   subIdx++,
                   ntohl(subRec->ingressInterface),
                   ip_str,
                   subRec->sourceIPv4PrefixLength);
        }
        
        fbSubTemplateListClear(&rec.subTemplateList);
    }
    
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("\n✅ Collected %d records from %s\n", count, IPFIX_FILE);
    
    fBufFree(fbuf);
    fbInfoModelFree(model);
    return 0;
}

// ========== MAIN ==========
int main() {
    printf("\n");
    printf("╔═════════════════════════════════════════════════════╗\n");
    printf("║      SAV IPFIX End-to-End Test (libfixbuf)         ║\n");
    printf("║                                                     ║\n");
    printf("║  Phase 1: Export SAV records → IPFIX file          ║\n");
    printf("║  Phase 2: Collect SAV records ← IPFIX file         ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n");
    
    // Phase 1: Export
    if (export_sav_records() != 0) {
        fprintf(stderr, "\n❌ Export failed!\n");
        return 1;
    }
    
    // Phase 2: Collect
    if (collect_sav_records() != 0) {
        fprintf(stderr, "\n❌ Collection failed!\n");
        return 1;
    }
    
    printf("\n");
    printf("╔═════════════════════════════════════════════════════╗\n");
    printf("║        ✅ END-TO-END TEST PASSED!                   ║\n");
    printf("╚═════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}
