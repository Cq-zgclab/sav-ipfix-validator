/**
 * @file test_simple_export.c
 * @brief Minimal export test without SubTemplateList complexity
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sav_ie_definitions.h"

int main(int argc, char **argv)
{
    GError *err = NULL;
    const char *output_file = argc > 1 ? argv[1] : "minimal.ipfix";
    
    printf("=== Minimal SAV IPFIX Export Test ===\n\n");
    
    /* Initialize info model */
    printf("Step 1: Info model...\n");
    fbInfoModel_t *model = fbInfoModelAlloc();
    if (!sav_init_info_model(model)) {
        fprintf(stderr, "ERROR: Failed to init info model\n");
        return 1;
    }
    printf("✓ OK\n");
    
    /* Create session */
    printf("\nStep 2: Session and templates...\n");
    fbSession_t *session = fbSessionAlloc(model);
    
    if (!sav_add_templates(session, &err)) {
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "Unknown");
        if (err) g_error_free(err);
        return 1;
    }
    printf("✓ OK\n");
    
    /* Create exporter */
    printf("\nStep 3: File exporter: %s\n", output_file);
    fbExporter_t *exporter = fbExporterAllocFile(output_file);
    if (!exporter) {
        fprintf(stderr, "ERROR: Failed to create exporter\n");
        return 1;
    }
    
    fBuf_t *fbuf = fBufAllocForExport(session, exporter);
    if (!fbuf) {
        fprintf(stderr, "ERROR: Failed to create fbuf\n");
        return 1;
    }
    printf("✓ OK\n");
    
    /* Set templates */
    printf("\nStep 4: Setting templates...\n");
    if (!fBufSetExportTemplate(fbuf, SAV_MAIN_TEMPLATE_ID, &err)) {
        fprintf(stderr, "ERROR: Main export template: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    if (!fBufSetInternalTemplate(fbuf, SAV_MAIN_TEMPLATE_ID, &err)) {
        fprintf(stderr, "ERROR: Main internal template: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    /* Also set sub-template as export template */
    if (!fBufSetExportTemplate(fbuf, SAV_TMPL_IPV4_INTERFACE_PREFIX, &err)) {
        fprintf(stderr, "ERROR: Sub export template: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("✓ OK\n");
    
    /* Get sub-template pointer */
    printf("\nStep 5: Getting sub-template...\n");
    fbTemplate_t *sub_tmpl = fbSessionGetTemplate(session, TRUE, 
                                                    SAV_TMPL_IPV4_INTERFACE_PREFIX, 
                                                    &err);
    if (!sub_tmpl) {
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "Unknown");
        if (err) g_error_free(err);
        return 1;
    }
    printf("✓ Got template\n");
    
    /* Create a minimal record WITH EMPTY SubTemplateList */
    printf("\nStep 6: Creating minimal record (empty SubTemplateList)...\n");
    sav_data_record_t record;
    memset(&record, 0, sizeof(record));
    
    record.observationTimeMilliseconds = (uint64_t)time(NULL) * 1000;
    record.savRuleType = SAV_RULE_TYPE_ALLOWLIST;
    record.savTargetType = SAV_TARGET_TYPE_INTERFACE_BASED;
    record.savPolicyAction = SAV_POLICY_ACTION_PERMIT;
    
    /* Initialize SubTemplateList as EMPTY with valid template pointer */
    fbSubTemplateListInit(&record.savMatchedContentList, 
                          0,  /* semantic */
                          SAV_TMPL_IPV4_INTERFACE_PREFIX, /* template ID */
                          sub_tmpl, /* template pointer - MUST NOT be NULL! */
                          0);   /* 0 elements */
    
    printf("✓ Record created\n");
    
    /* Export all templates first */
    printf("\nStep 7: Exporting templates to file...\n");
    if (!fbSessionExportTemplates(session, &err)) {
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "Unknown");
        if (err) g_error_free(err);
        return 1;
    }
    printf("✓ Templates exported\n");
    
    /* Try to export data */
    printf("\nStep 8: Exporting record...\n");
    if (!fBufAppend(fbuf, (uint8_t *)&record, sizeof(record), &err)) {
        fprintf(stderr, "ERROR: %s\n", err ? err->message : "Unknown");
        if (err) g_error_free(err);
        fbSubTemplateListClear(&record.savMatchedContentList);
        return 1;
    }
    printf("✓ Export successful!\n");
    
    /* Clean up - proper order */
    printf("\nStep 9: Cleaning up...\n");
    
    /* Clear STL BEFORE freeing buffer */
    fbSubTemplateListClear(&record.savMatchedContentList);
    printf("  STL cleared\n");
    
    /* Emit and free buffer */
    fBufEmit(fbuf, &err);
    printf("  Buffer emitted\n");
    
    fBufFree(fbuf);
    printf("  Buffer freed\n");
    
    /* Session manages templates, free it last */
    fbSessionFree(session);
    printf("  Session freed\n");
    
    fbInfoModelFree(model);
    printf("  Model freed\n");
    
    printf("\n✅ Success! File created: %s\n", output_file);
    printf("Try: ./build/bin/sav_dump %s\n\n", output_file);
    
    return 0;
}
