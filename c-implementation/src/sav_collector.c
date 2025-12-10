/**
 * @file sav_collector.c
 * @brief SAV IPFIX Collector Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "sav_collector.h"

/* Create a file-based collector */
sav_collector_ctx_t* sav_create_file_collector(
    const char *filename,
    GError     **err)
{
    if (!filename) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "NULL filename provided");
        return NULL;
    }
    
    /* Allocate context */
    sav_collector_ctx_t *ctx = g_new0(sav_collector_ctx_t, 1);
    
    /* Initialize info model */
    ctx->model = fbInfoModelAlloc();
    if (!ctx->model) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to allocate info model");
        g_free(ctx);
        return NULL;
    }
    
    if (!sav_init_info_model(ctx->model)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to initialize SAV info model");
        fbInfoModelFree(ctx->model);
        g_free(ctx);
        return NULL;
    }
    
    /* Create session */
    ctx->session = fbSessionAlloc(ctx->model);
    if (!ctx->session) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to allocate session");
        fbInfoModelFree(ctx->model);
        g_free(ctx);
        return NULL;
    }
    
    /* Add templates to session */
    if (!sav_add_templates(ctx->session, err)) {
        sav_collector_ctx_destroy(ctx);
        return NULL;
    }
    
    /* Create collector */
    fbCollector_t *collector = fbCollectorAllocFile(NULL, filename, err);
    if (!collector) {
        fbSessionFree(ctx->session);
        fbInfoModelFree(ctx->model);
        g_free(ctx);
        return NULL;
    }
    
    /* Create buffer for collection */
    ctx->fbuf = fBufAllocForCollection(ctx->session, collector);
    if (!ctx->fbuf) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to create collection buffer");
        fbSessionFree(ctx->session);
        fbInfoModelFree(ctx->model);
        g_free(ctx);
        return NULL;
    }
    
    /* Set internal template for reading */
    if (!fBufSetInternalTemplate(ctx->fbuf, SAV_MAIN_TEMPLATE_ID, err)) {
        fBufFree(ctx->fbuf);
        fbSessionFree(ctx->session);
        fbInfoModelFree(ctx->model);
        g_free(ctx);
        return NULL;
    }
    
    ctx->records_read = 0;
    ctx->parse_errors = 0;
    
    return ctx;
}

/* Parse SubTemplateList entries */
static gboolean parse_subtmpl_list(
    fbSubTemplateList_t       *stl,
    sav_parsed_record_t       *record,
    GError                    **err)
{
    if (!stl || !record) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "NULL parameter in parse_subtmpl_list");
        return FALSE;
    }
    
    /* Get SubTemplateList info */
    record->sub_template_id = fbSubTemplateListGetTemplateID(stl);
    record->mapping_count = fbSubTemplateListCountElements(stl);
    
    if (record->mapping_count == 0) {
        /* Empty list is valid (no mappings) */
        record->mappings.ipv4_mappings = NULL;
        return TRUE;
    }
    
    /* Determine if IPv4 or IPv6 based on template ID */
    gboolean is_ipv4 = (record->sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX ||
                        record->sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE);
    gboolean is_ipv6 = (record->sub_template_id == SAV_TMPL_IPV6_INTERFACE_PREFIX ||
                        record->sub_template_id == SAV_TMPL_IPV6_PREFIX_INTERFACE);
    
    if (!is_ipv4 && !is_ipv6) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Unknown sub-template ID: %u", record->sub_template_id);
        return FALSE;
    }
    
    /* Allocate mapping array */
    if (is_ipv4) {
        record->mappings.ipv4_mappings = g_new0(sav_ipv4_mapping_t, record->mapping_count);
    } else {
        record->mappings.ipv6_mappings = g_new0(sav_ipv6_mapping_t, record->mapping_count);
    }
    
    /* Iterate through SubTemplateList entries */
    uint32_t idx = 0;
    void *entry_ptr = NULL;
    
    while ((entry_ptr = fbSubTemplateListGetNextPtr(stl, entry_ptr)) != NULL && 
           idx < record->mapping_count) {
        
        if (is_ipv4) {
            /* Copy IPv4 mapping */
            sav_ipv4_mapping_t *src = (sav_ipv4_mapping_t *)entry_ptr;
            memcpy(&record->mappings.ipv4_mappings[idx], src, sizeof(sav_ipv4_mapping_t));
        } else {
            /* Copy IPv6 mapping */
            sav_ipv6_mapping_t *src = (sav_ipv6_mapping_t *)entry_ptr;
            memcpy(&record->mappings.ipv6_mappings[idx], src, sizeof(sav_ipv6_mapping_t));
        }
        
        idx++;
    }
    
    if (idx != record->mapping_count) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "SubTemplateList iteration mismatch: expected %u, got %u",
                    record->mapping_count, idx);
        return FALSE;
    }
    
    return TRUE;
}

/* Read next SAV record */
gboolean sav_read_record(
    sav_collector_ctx_t *ctx,
    sav_parsed_record_t *record,
    GError              **err)
{
    if (!ctx || !record) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "NULL parameter in sav_read_record");
        return FALSE;
    }
    
    /* Clear record */
    memset(record, 0, sizeof(*record));
    
    /* Read raw IPFIX record */
    sav_data_record_t raw_record;
    memset(&raw_record, 0, sizeof(raw_record));
    
    size_t len = sizeof(raw_record);
    gboolean result = fBufNext(ctx->fbuf, (uint8_t *)&raw_record, &len, err);
    
    if (!result) {
        if (err && *err && (*err)->code == FB_ERROR_EOF) {
            /* End of file - not an error */
            g_clear_error(err);
            return FALSE;
        }
        /* Real error */
        ctx->parse_errors++;
        return FALSE;
    }
    
    /* Extract basic fields */
    record->timestamp_ms = raw_record.observationTimeMilliseconds;
    record->rule_type = raw_record.savRuleType;
    record->target_type = raw_record.savTargetType;
    record->policy_action = raw_record.savPolicyAction;
    
    /* Parse SubTemplateList */
    if (!parse_subtmpl_list(&raw_record.savMatchedContentList, record, err)) {
        ctx->parse_errors++;
        fbSubTemplateListClear(&raw_record.savMatchedContentList);
        return FALSE;
    }
    
    /* Clean up SubTemplateList */
    fbSubTemplateListClear(&raw_record.savMatchedContentList);
    
    ctx->records_read++;
    return TRUE;
}

/* Free parsed record */
void sav_free_parsed_record(sav_parsed_record_t *record)
{
    if (record) {
        if (record->mappings.ipv4_mappings) {
            g_free(record->mappings.ipv4_mappings);
            record->mappings.ipv4_mappings = NULL;
        }
        /* Note: ipv4_mappings and ipv6_mappings share the same union,
         * so only need to free once */
    }
}

/* Close collector */
void sav_collector_ctx_destroy(sav_collector_ctx_t *ctx)
{
    if (!ctx) return;

    if (ctx->fbuf) {
        fBufFree(ctx->fbuf);
    }
    if (ctx->session) {
        fbSessionFree(ctx->session);
    }
    if (ctx->model) {
        fbInfoModelFree(ctx->model);
    }
    g_free(ctx);
}

/**
 * Close and free a collector context
 * 
 * @param ctx  Collector context to free
 */
void sav_close_collector(sav_collector_ctx_t *ctx)
{
    sav_collector_ctx_destroy(ctx);
}

/* Get statistics */
void sav_collector_get_stats(
    sav_collector_ctx_t *ctx,
    uint64_t            *records_read,
    uint64_t            *parse_errors)
{
    if (ctx) {
        if (records_read) *records_read = ctx->records_read;
        if (parse_errors) *parse_errors = ctx->parse_errors;
    }
}

/* Print record in human-readable format */
void sav_print_record(
    const sav_parsed_record_t *record,
    FILE                      *output)
{
    if (!record || !output) return;
    
    fprintf(output, "=== SAV Record ===\n");
    fprintf(output, "Timestamp: %lu ms\n", (unsigned long)record->timestamp_ms);
    fprintf(output, "Rule Type: %s (%u)\n", 
            sav_rule_type_name(record->rule_type), record->rule_type);
    fprintf(output, "Target Type: %s (%u)\n",
            sav_target_type_name(record->target_type), record->target_type);
    fprintf(output, "Policy Action: %s (%u)\n",
            sav_policy_action_name(record->policy_action), record->policy_action);
    fprintf(output, "Sub-Template ID: %u\n", record->sub_template_id);
    fprintf(output, "Mapping Count: %u\n", record->mapping_count);
    
    if (record->mapping_count > 0) {
        fprintf(output, "\nMappings:\n");
        
        gboolean is_ipv4 = (record->sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX ||
                            record->sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE);
        
        for (uint32_t i = 0; i < record->mapping_count; i++) {
            if (is_ipv4) {
                sav_ipv4_mapping_t *m = &record->mappings.ipv4_mappings[i];
                struct in_addr addr;
                addr.s_addr = m->sourceIPv4Prefix;
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
                
                fprintf(output, "  [%u] Interface %u <-> %s/%u\n",
                        i, ntohl(m->ingressInterface), ip_str, m->sourceIPv4PrefixLength);
            } else {
                sav_ipv6_mapping_t *m = &record->mappings.ipv6_mappings[i];
                char ip_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, m->sourceIPv6Prefix, ip_str, sizeof(ip_str));
                
                fprintf(output, "  [%u] Interface %u <-> %s/%u\n",
                        i, ntohl(m->ingressInterface), ip_str, m->sourceIPv6PrefixLength);
            }
        }
    }
    fprintf(output, "\n");
}

/* Export to JSON */
void sav_export_record_json(
    const sav_parsed_record_t *record,
    FILE                      *output)
{
    if (!record || !output) return;
    
    fprintf(output, "{\n");
    fprintf(output, "  \"timestamp_ms\": %lu,\n", (unsigned long)record->timestamp_ms);
    fprintf(output, "  \"rule_type\": %u,\n", record->rule_type);
    fprintf(output, "  \"rule_type_name\": \"%s\",\n", sav_rule_type_name(record->rule_type));
    fprintf(output, "  \"target_type\": %u,\n", record->target_type);
    fprintf(output, "  \"target_type_name\": \"%s\",\n", sav_target_type_name(record->target_type));
    fprintf(output, "  \"policy_action\": %u,\n", record->policy_action);
    fprintf(output, "  \"policy_action_name\": \"%s\",\n", sav_policy_action_name(record->policy_action));
    fprintf(output, "  \"sub_template_id\": %u,\n", record->sub_template_id);
    fprintf(output, "  \"mapping_count\": %u,\n", record->mapping_count);
    fprintf(output, "  \"mappings\": [\n");
    
    gboolean is_ipv4 = (record->sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX ||
                        record->sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE);
    
    for (uint32_t i = 0; i < record->mapping_count; i++) {
        fprintf(output, "    {\n");
        
        if (is_ipv4) {
            sav_ipv4_mapping_t *m = &record->mappings.ipv4_mappings[i];
            struct in_addr addr;
            addr.s_addr = m->sourceIPv4Prefix;
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
            
            fprintf(output, "      \"interface\": %u,\n", ntohl(m->ingressInterface));
            fprintf(output, "      \"prefix\": \"%s\",\n", ip_str);
            fprintf(output, "      \"prefix_length\": %u\n", m->sourceIPv4PrefixLength);
        } else {
            sav_ipv6_mapping_t *m = &record->mappings.ipv6_mappings[i];
            char ip_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, m->sourceIPv6Prefix, ip_str, sizeof(ip_str));
            
            fprintf(output, "      \"interface\": %u,\n", ntohl(m->ingressInterface));
            fprintf(output, "      \"prefix\": \"%s\",\n", ip_str);
            fprintf(output, "      \"prefix_length\": %u\n", m->sourceIPv6PrefixLength);
        }
        
        fprintf(output, "    }%s\n", (i < record->mapping_count - 1) ? "," : "");
    }
    
    fprintf(output, "  ]\n");
    fprintf(output, "}\n");
}

/* Validate record */
gboolean sav_validate_record(
    const sav_parsed_record_t *record,
    GError                    **err)
{
    if (!record) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "NULL record");
        return FALSE;
    }
    
    /* Validate enum values */
    if (!sav_validate_rule_type(record->rule_type)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid rule_type: %u", record->rule_type);
        return FALSE;
    }
    
    if (!sav_validate_target_type(record->target_type)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid target_type: %u", record->target_type);
        return FALSE;
    }
    
    if (!sav_validate_policy_action(record->policy_action)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid policy_action: %u", record->policy_action);
        return FALSE;
    }
    
    /* Validate template ID matches target type */
    uint16_t expected_tmpl = sav_get_template_id(record->rule_type, record->target_type);
    if (record->sub_template_id != expected_tmpl && 
        record->sub_template_id != expected_tmpl + 1) { /* Allow IPv4/IPv6 variance */
        /* More flexible check */
        gboolean valid_tmpl = (record->sub_template_id >= SAV_TMPL_IPV4_INTERFACE_PREFIX &&
                               record->sub_template_id <= SAV_TMPL_IPV6_PREFIX_INTERFACE);
        if (!valid_tmpl) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                        "Invalid sub-template ID: %u", record->sub_template_id);
            return FALSE;
        }
    }
    
    /* Validate prefix lengths */
    gboolean is_ipv4 = (record->sub_template_id == SAV_TMPL_IPV4_INTERFACE_PREFIX ||
                        record->sub_template_id == SAV_TMPL_IPV4_PREFIX_INTERFACE);
    
    for (uint32_t i = 0; i < record->mapping_count; i++) {
        if (is_ipv4) {
            uint8_t plen = record->mappings.ipv4_mappings[i].sourceIPv4PrefixLength;
            if (plen > 32) {
                g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                            "Invalid IPv4 prefix length at index %u: %u", i, plen);
                return FALSE;
            }
        } else {
            uint8_t plen = record->mappings.ipv6_mappings[i].sourceIPv6PrefixLength;
            if (plen > 128) {
                g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                            "Invalid IPv6 prefix length at index %u: %u", i, plen);
                return FALSE;
            }
        }
    }
    
    return TRUE;
}
