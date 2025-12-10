/**
 * @file sav_collector.h
 * @brief SAV IPFIX Collector API
 * 
 * This module provides functions to collect and parse SAV IPFIX records
 * from files and network streams.
 */

#ifndef SAV_COLLECTOR_H
#define SAV_COLLECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <fixbuf/public.h>
#include "sav_ie_definitions.h"

/**
 * SAV Parsed Record
 * 
 * Represents a fully parsed SAV IPFIX record with extracted mappings.
 */
typedef struct sav_parsed_record {
    uint64_t  timestamp_ms;           /* Observation time in milliseconds */
    uint8_t   rule_type;              /* SAV rule type (allowlist/blocklist) */
    uint8_t   target_type;            /* SAV target type (interface/prefix based) */
    uint8_t   policy_action;          /* Policy action */
    uint16_t  sub_template_id;        /* SubTemplateList template ID used */
    uint32_t  mapping_count;          /* Number of mappings in the list */
    
    /* Parsed mappings - using union for different types */
    union {
        sav_ipv4_mapping_t *ipv4_mappings;
        sav_ipv6_mapping_t *ipv6_mappings;
    } mappings;
} sav_parsed_record_t;

/**
 * SAV Collector Context
 * 
 * Manages the state of a collector reading SAV IPFIX records.
 */
typedef struct sav_collector_ctx {
    fbInfoModel_t   *model;           /* Info model with SAV IEs */
    fbSession_t     *session;         /* Session with templates */
    fBuf_t          *fbuf;            /* Collection buffer */
    uint64_t        records_read;     /* Statistics: total records */
    uint64_t        parse_errors;     /* Statistics: parse errors */
} sav_collector_ctx_t;

/**
 * Create a file-based SAV collector
 * 
 * @param filename     Path to IPFIX file to read
 * @param err          Error structure
 * 
 * @return Collector context on success, NULL on error
 */
sav_collector_ctx_t* sav_create_file_collector(
    const char *filename,
    GError     **err);

/**
 * Read next SAV record from collector
 * 
 * @param ctx          Collector context
 * @param record       Output: parsed record structure
 * @param err          Error structure
 * 
 * @return TRUE if record was read, FALSE on EOF or error
 */
gboolean sav_read_record(
    sav_collector_ctx_t *ctx,
    sav_parsed_record_t *record,
    GError              **err);

/**
 * Free a parsed record's internal memory
 * 
 * Call this after processing each record to free mapping arrays.
 * 
 * @param record  Record to free
 */
void sav_free_parsed_record(sav_parsed_record_t *record);

/**
 * Close and free a collector context
 * 
 * @param ctx  Collector context to free
 */
void sav_collector_ctx_destroy(sav_collector_ctx_t *ctx);

/**
 * Get collector statistics
 * 
 * @param ctx           Collector context
 * @param records_read  Output: number of records successfully read
 * @param parse_errors  Output: number of parse errors encountered
 */
void sav_collector_get_stats(
    sav_collector_ctx_t *ctx,
    uint64_t            *records_read,
    uint64_t            *parse_errors);

/**
 * Print a parsed SAV record in human-readable format
 * 
 * @param record  Record to print
 * @param output  File stream (e.g., stdout, stderr, or a file)
 */
void sav_print_record(
    const sav_parsed_record_t *record,
    FILE                      *output);

/**
 * Export a parsed SAV record to JSON format
 * 
 * @param record  Record to export
 * @param output  File stream for JSON output
 */
void sav_export_record_json(
    const sav_parsed_record_t *record,
    FILE                      *output);

/**
 * Validate a parsed SAV record
 * 
 * Checks:
 * - Valid rule_type, target_type, policy_action values
 * - SubTemplateList template ID matches rule/target types
 * - Mapping count > 0
 * - Prefix lengths are valid
 * 
 * @param record  Record to validate
 * @param err     Error structure (populated if validation fails)
 * 
 * @return TRUE if valid, FALSE otherwise
 */
gboolean sav_validate_record(
    const sav_parsed_record_t *record,
    GError                    **err);

#endif /* SAV_COLLECTOR_H */
