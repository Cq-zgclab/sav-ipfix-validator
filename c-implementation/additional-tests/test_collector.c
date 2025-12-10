/**
 * @file test_collector.c
 * @brief Test SAV IPFIX collector functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include "sav_collector.h"

int main(int argc, char **argv)
{
    GError *err = NULL;
    const char *input_file = argc > 1 ? argv[1] : "test.ipfix";
    
    printf("=== SAV IPFIX Collector Test ===\n\n");
    
    /* Create collector */
    printf("Opening IPFIX file: %s\n", input_file);
    sav_collector_ctx_t *collector = sav_create_file_collector(input_file, &err);
    if (!collector) {
        fprintf(stderr, "ERROR: Failed to create collector: %s\n", 
                err ? err->message : "Unknown error");
        if (err) g_error_free(err);
        return 1;
    }
    printf("✓ Collector created successfully\n\n");
    
    /* Read and process records */
    printf("Reading records...\n\n");
    sav_parsed_record_t record;
    uint32_t count = 0;
    
    while (sav_read_record(collector, &record, &err)) {
        count++;
        printf("--- Record #%u ---\n", count);
        sav_print_record(&record, stdout);
        
        /* Validate record */
        if (!sav_validate_record(&record, &err)) {
            fprintf(stderr, "WARNING: Record validation failed: %s\n",
                    err ? err->message : "Unknown error");
            if (err) {
                g_error_free(err);
                err = NULL;
            }
        } else {
            printf("✓ Record validation passed\n");
        }
        
        /* Export to JSON */
        printf("\nJSON format:\n");
        sav_export_record_json(&record, stdout);
        printf("\n");
        
        /* Free record memory */
        sav_free_parsed_record(&record);
    }
    
    /* Check for errors */
    if (err && err->code != FB_ERROR_EOF) {
        fprintf(stderr, "\nERROR: Failed to read record: %s\n", err->message);
        g_error_free(err);
    } else if (err) {
        g_error_free(err);
    }
    
    /* Get statistics */
    uint64_t records_read, parse_errors;
    sav_collector_get_stats(collector, &records_read, &parse_errors);
    
    printf("=== Statistics ===\n");
    printf("Records successfully read: %lu\n", (unsigned long)records_read);
    printf("Parse errors: %lu\n", (unsigned long)parse_errors);
    
    if (records_read == 0) {
        printf("\n⚠ No records read. This is expected if no IPFIX file exists yet.\n");
        printf("To test the collector, you need to:\n");
        printf("1. Fix Phase 2 (exporter) to generate test IPFIX files, OR\n");
        printf("2. Create test files manually using libfixbuf examples, OR\n");
        printf("3. Use actual SAV IPFIX data from a network device\n");
    }
    
    /* Clean up */
    sav_close_collector(collector);
    
    printf("\n✓ Collector test complete\n");
    
    return (records_read > 0) ? 0 : 1;
}
