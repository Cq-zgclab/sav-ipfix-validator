/**
 * @file test_empty_stl.c
 * @brief Test exporting with EMPTY SubTemplateList
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sav_exporter.h"

int main(void)
{
    GError *err = NULL;
    
    printf("=== Test: Empty SubTemplateList Export ===\n\n");
    
    /* Initialize */
    printf("Step 1: Creating info model and session...\n");
    fbInfoModel_t *model = fbInfoModelAlloc();
    if (!sav_init_info_model(model)) {
        fprintf(stderr, "ERROR: Failed to init info model\n");
        return 1;
    }
    
    fbSession_t *session = fbSessionAlloc(model);
    if (!sav_add_templates(session, &err)) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("  ✓ OK\n");
    
    /* Create exporter */
    printf("\nStep 2: Creating exporter...\n");
    fBuf_t *exporter = sav_create_file_exporter(model, session, "test_empty_stl.ipfix", &err);
    if (!exporter) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("  ✓ OK\n");
    
    /* Initialize context with NO entries */
    printf("\nStep 3: Creating context (NO entries will be added)...\n");
    sav_record_ctx_t ctx;
    if (!sav_record_ctx_init(&ctx, model, session,
                             SAV_RULE_TYPE_ALLOWLIST,
                             SAV_TARGET_TYPE_INTERFACE_BASED,
                             &err)) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("  ✓ Context has %u entries (should be 0)\n", ctx.entry_count);
    
    /* Export templates */
    printf("\nStep 4: Exporting templates...\n");
    if (!sav_export_templates(exporter, &err)) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("  ✓ OK\n");
    
    /* Export record with EMPTY SubTemplateList */
    printf("\nStep 5: Exporting record with EMPTY SubTemplateList...\n");
    uint64_t timestamp = (uint64_t)time(NULL) * 1000;
    if (!sav_export_record(&ctx, exporter, timestamp,
                          SAV_RULE_TYPE_ALLOWLIST,
                          SAV_TARGET_TYPE_INTERFACE_BASED,
                          SAV_POLICY_ACTION_PERMIT,
                          &err)) {
        fprintf(stderr, "ERROR: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    printf("  ✓ Export successful!\n");
    
    /* Cleanup */
    sav_record_ctx_cleanup(&ctx);
    sav_close_exporter(exporter);
    fbSessionFree(session);
    fbInfoModelFree(model);
    
    printf("\n✅ Success! Empty SubTemplateList exported to test_empty_stl.ipfix\n\n");
    
    return 0;
}
