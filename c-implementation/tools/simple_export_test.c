/**
 * @file simple_export_test.c
 * @brief Simple test to verify basic SAV IPFIX export without SubTemplateList
 * 
 * This test creates a minimal SAV record to validate the export infrastructure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include "sav_ie_definitions.h"
#include "sav_exporter.h"

int main(int argc, char **argv)
{
    GError *err = NULL;
    const char *output_file = argc > 1 ? argv[1] : "test_output.ipfix";
    
    printf("=== SAV IPFIX Simple Export Test ===\n\n");
    
    /* Step 1: Initialize info model */
    printf("Step 1: Initializing info model...\n");
    fbInfoModel_t *model = fbInfoModelAlloc();
    if (!model) {
        fprintf(stderr, "ERROR: Failed to allocate info model\n");
        return 1;
    }
    
    if (!sav_init_info_model(model)) {
        fprintf(stderr, "ERROR: Failed to initialize SAV info model\n");
        fbInfoModelFree(model);
        return 1;
    }
    printf("  ✓ Info model initialized\n");
    
    /* Step 2: Create session and add templates */
    printf("\nStep 2: Creating session and registering templates...\n");
    fbSession_t *session = fbSessionAlloc(model);
    if (!session) {
        fprintf(stderr, "ERROR: Failed to allocate session\n");
        fbInfoModelFree(model);
        return 1;
    }
    
    /* For exporters, add templates as BOTH internal and external */
    if (!sav_add_templates(session, TRUE, &err)) {  /* TRUE = internal templates */
        fprintf(stderr, "ERROR: Failed to add internal templates: %s\n", err->message);
        g_error_free(err);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    if (!sav_add_templates(session, FALSE, &err)) {  /* FALSE = external templates */
        fprintf(stderr, "ERROR: Failed to add external templates: %s\n", err->message);
        g_error_free(err);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    printf("  ✓ Templates registered (internal + external)\n");
    
    /* Step 3: Create file exporter */
    printf("\nStep 3: Creating file exporter: %s\n", output_file);
    fBuf_t *exporter = sav_create_file_exporter(model, session, output_file, &err);
    if (!exporter) {
        fprintf(stderr, "ERROR: Failed to create exporter: %s\n", err->message);
        g_error_free(err);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    printf("  ✓ Exporter created\n");
    
    /* WORKAROUND: Manually set sub-template as export template before exporting templates
     * This tells libfixbuf that sub-template 901 is available for SubTemplateList encoding */
    if (!fBufSetExportTemplate(exporter, SAV_TMPL_IPV4_INTERFACE_PREFIX, &err)) {
        fprintf(stderr, "ERROR: Failed to set sub-template: %s\n", err->message);
        g_error_free(err);
        sav_close_exporter(exporter);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    /* Reset back to main template */
    if (!fBufSetExportTemplate(exporter, SAV_MAIN_TEMPLATE_ID, &err)) {
        fprintf(stderr, "ERROR: Failed to reset main template: %s\n", err->message);
        g_error_free(err);
        sav_close_exporter(exporter);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    
    /* Step 4: Initialize record context */
    printf("\nStep 4: Initializing record context...\n");
    sav_record_ctx_t ctx;
    if (!sav_record_ctx_init(&ctx, model, session, 
                             SAV_RULE_TYPE_ALLOWLIST, 
                             SAV_TARGET_TYPE_INTERFACE_BASED, 
                             &err)) {
        fprintf(stderr, "ERROR: Failed to initialize context: %s\n", err->message);
        g_error_free(err);
        sav_close_exporter(exporter);
        fbSessionFree(session);
        fbInfoModelFree(model);
        return 1;
    }
    printf("  ✓ Context initialized\n");
    
    /* Step 5: Add some test entries */
    printf("\nStep 5: Adding test entries...\n");
    
    /* Add IPv4 Interface->Prefix mappings */
    uint32_t prefix1 = inet_addr("192.0.2.0");  /* TEST-NET-1 */
    if (!sav_add_ipv4_interface_prefix(&ctx, 1, prefix1, 24, &err)) {
        fprintf(stderr, "ERROR: Failed to add entry: %s\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    printf("  ✓ Added: Interface 1 -> 192.0.2.0/24\n");
    
    uint32_t prefix2 = inet_addr("198.51.100.0");  /* TEST-NET-2 */
    if (!sav_add_ipv4_interface_prefix(&ctx, 2, prefix2, 24, &err)) {
        fprintf(stderr, "ERROR: Failed to add entry: %s\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    printf("  ✓ Added: Interface 2 -> 198.51.100.0/24\n");
    
    uint32_t prefix3 = inet_addr("203.0.113.0");  /* TEST-NET-3 */
    if (!sav_add_ipv4_interface_prefix(&ctx, 3, prefix3, 24, &err)) {
        fprintf(stderr, "ERROR: Failed to add entry: %s\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    printf("  ✓ Added: Interface 3 -> 203.0.113.0/24\n");
    
    /* Step 6: Export templates */
    printf("\nStep 6: Exporting templates...\n");
    if (!sav_export_templates(exporter, &err)) {
        fprintf(stderr, "ERROR: Failed to export templates: %s\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    printf("  ✓ Templates exported\n");
    
    /* Step 7: Export record */
    printf("\nStep 7: Exporting record...\n");
    uint64_t timestamp = (uint64_t)time(NULL) * 1000; /* milliseconds */
    
    if (!sav_export_record(&ctx, exporter, timestamp,
                          SAV_RULE_TYPE_ALLOWLIST,
                          SAV_TARGET_TYPE_INTERFACE_BASED,
                          SAV_POLICY_ACTION_PERMIT,
                          &err)) {
        fprintf(stderr, "ERROR: Failed to export record: %s\n", err->message);
        g_error_free(err);
        goto cleanup;
    }
    printf("  ✓ Record exported\n");
    
    /* Success */
    printf("\n=== Export Test Complete ===\n");
    printf("Output file: %s\n", output_file);
    printf("Entries exported: %u\n", ctx.entry_count);
    printf("\nYou can inspect the file with:\n");
    printf("  ipfixDump %s\n", output_file);
    printf("  hexdump -C %s\n\n", output_file);
    
cleanup:
    sav_record_ctx_cleanup(&ctx);
    sav_close_exporter(exporter);
    fbSessionFree(session);
    fbInfoModelFree(model);
    
    return 0;
}
