/**
 * @file sav_dump.c
 * @brief Dump SAV IPFIX file contents in various formats
 * 
 * Usage: sav_dump [options] <ipfix_file>
 * Options:
 *   -j, --json     Output in JSON format
 *   -v, --verbose  Verbose output with validation
 *   -h, --help     Show this help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "sav_collector.h"

static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options] <ipfix_file>\n\n", prog_name);
    printf("Options:\n");
    printf("  -j, --json      Output in JSON format\n");
    printf("  -v, --verbose   Verbose output with validation\n");
    printf("  -s, --stats     Show only statistics\n");
    printf("  -h, --help      Show this help\n\n");
    printf("Examples:\n");
    printf("  %s data.ipfix               # Dump in text format\n", prog_name);
    printf("  %s -j data.ipfix            # Dump in JSON format\n", prog_name);
    printf("  %s -v data.ipfix            # Dump with validation\n", prog_name);
    printf("  %s -s data.ipfix            # Show only statistics\n\n", prog_name);
}

int main(int argc, char **argv)
{
    int json_format = 0;
    int verbose = 0;
    int stats_only = 0;
    
    /* Parse options */
    static struct option long_options[] = {
        {"json",    no_argument, 0, 'j'},
        {"verbose", no_argument, 0, 'v'},
        {"stats",   no_argument, 0, 's'},
        {"help",    no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "jvsh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'j':
                json_format = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 's':
                stats_only = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Check for input file */
    if (optind >= argc) {
        fprintf(stderr, "ERROR: No input file specified\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    const char *input_file = argv[optind];
    GError *err = NULL;
    
    /* Create collector */
    sav_collector_ctx_t *collector = sav_create_file_collector(input_file, &err);
    if (!collector) {
        fprintf(stderr, "ERROR: Failed to open %s: %s\n", 
                input_file, err ? err->message : "Unknown error");
        if (err) g_error_free(err);
        return 1;
    }
    
    /* JSON array start */
    if (json_format && !stats_only) {
        printf("[\n");
    }
    
    /* Read and process records */
    sav_parsed_record_t record;
    uint32_t count = 0;
    int first_record = 1;
    
    while (sav_read_record(collector, &record, &err)) {
        count++;
        
        if (!stats_only) {
            if (json_format) {
                if (!first_record) printf(",\n");
                sav_export_record_json(&record, stdout);
                first_record = 0;
            } else {
                printf("=== Record #%u ===\n", count);
                sav_print_record(&record, stdout);
            }
            
            /* Validate if verbose */
            if (verbose) {
                if (!sav_validate_record(&record, &err)) {
                    fprintf(stderr, "⚠ Validation failed: %s\n",
                            err ? err->message : "Unknown error");
                    if (err) {
                        g_error_free(err);
                        err = NULL;
                    }
                } else {
                    if (!json_format) {
                        printf("✓ Validation passed\n\n");
                    }
                }
            }
        }
        
        sav_free_parsed_record(&record);
    }
    
    /* JSON array end */
    if (json_format && !stats_only) {
        printf("\n]\n");
    }
    
    /* Check for read errors */
    if (err && err->code != FB_ERROR_EOF) {
        fprintf(stderr, "\nERROR: %s\n", err->message);
        g_error_free(err);
    } else if (err) {
        g_error_free(err);
    }
    
    /* Statistics */
    uint64_t records_read, parse_errors;
    sav_collector_get_stats(collector, &records_read, &parse_errors);
    
    if (stats_only || verbose) {
        if (!json_format || stats_only) {
            printf("\n=== Statistics ===\n");
            printf("File: %s\n", input_file);
            printf("Records read: %lu\n", (unsigned long)records_read);
            printf("Parse errors: %lu\n", (unsigned long)parse_errors);
            
            if (records_read > 0) {
                printf("Success rate: %.1f%%\n",
                       100.0 * records_read / (records_read + parse_errors));
            }
        }
    }
    
    /* Clean up */
    sav_close_collector(collector);
    
    return (records_read > 0) ? 0 : 1;
}
