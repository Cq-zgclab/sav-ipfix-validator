/**
 * @file sav_exporter.c
 * @brief SAV IPFIX Exporter Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "sav_exporter.h"

/* Initialize a SAV record context */
gboolean sav_record_ctx_init(
    sav_record_ctx_t *ctx,
    fbInfoModel_t    *model,
    fbSession_t      *session,
    uint8_t          rule_type,
    uint8_t          target_type,
    GError           **err)
{
    if (!ctx || !model || !session) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "NULL parameter passed to sav_record_ctx_init");
        return FALSE;
    }
    
    /* Validate parameters */
    if (!sav_validate_rule_type(rule_type) || !sav_validate_target_type(target_type)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid rule_type (%u) or target_type (%u)", rule_type, target_type);
        return FALSE;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->model = model;
    ctx->session = session;
    
    /* Get main template */
    ctx->main_tmpl = fbSessionGetTemplate(session, TRUE, SAV_MAIN_TEMPLATE_ID, err);
    if (!ctx->main_tmpl) {
        return FALSE;
    }
    
    /* Determine which sub-template to use based on rule_type and target_type */
    uint16_t tmpl_id = sav_get_template_id(rule_type, target_type);
    ctx->sub_tmpl_id = tmpl_id; /* Store for later use */
    
    /* CRITICAL FIX (from YAF analysis): Use INTERNAL template pointer for SubTemplateList!
     * YAF always uses internal template pointers in fbSubTemplateListInit().
     * The internal template must be registered via fbSessionAddTemplate(session, TRUE, ...).
     * Both internal and external templates must be registered for SubTemplateList to work.
     */
    ctx->sub_tmpl = fbSessionGetTemplate(session, TRUE, tmpl_id, err);
    if (!ctx->sub_tmpl) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Cannot get internal template %u", tmpl_id);
        return FALSE;
    }
    g_debug("Got internal template %u for SubTemplateList", tmpl_id);
    
    /* Calculate entry size based on sub-template */
    /* Template 901/902: ingressInterface(4) + prefix(4/16) + prefixLen(1) = 9/21 bytes */
    /* Template 903/904: prefix(4/16) + prefixLen(1) + ingressInterface(4) = 9/21 bytes */
    if (tmpl_id == SAV_TMPL_IPV4_INTERFACE_PREFIX || tmpl_id == SAV_TMPL_IPV4_PREFIX_INTERFACE) {
        ctx->entry_size = 4 + 4 + 1; /* interface(4) + ipv4(4) + prefixlen(1) = 9 */
    } else {
        ctx->entry_size = 4 + 16 + 1; /* interface(4) + ipv6(16) + prefixlen(1) = 21 */
    }
    
    /* Allocate buffer for SubTemplateList entries */
    ctx->stl_capacity = ctx->entry_size * SAV_MAX_LIST_ENTRIES;
    ctx->stl_buffer = g_malloc0(ctx->stl_capacity);
    if (!ctx->stl_buffer) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to allocate SubTemplateList buffer");
        return FALSE;
    }
    
    ctx->entry_count = 0;
    
    return TRUE;
}

/* Clean up SAV record context */
void sav_record_ctx_cleanup(sav_record_ctx_t *ctx)
{
    if (ctx) {
        if (ctx->stl_buffer) {
            g_free(ctx->stl_buffer);
            ctx->stl_buffer = NULL;
        }
        memset(ctx, 0, sizeof(*ctx));
    }
}

/* Helper: Check if there's space for one more entry */
static gboolean check_capacity(sav_record_ctx_t *ctx, GError **err)
{
    if (ctx->entry_count >= SAV_MAX_LIST_ENTRIES) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "SubTemplateList capacity exceeded (%u entries)", SAV_MAX_LIST_ENTRIES);
        return FALSE;
    }
    return TRUE;
}

/* Add IPv4 Interface-to-Prefix entry */
gboolean sav_add_ipv4_interface_prefix(
    sav_record_ctx_t *ctx,
    uint32_t         interface_id,
    uint32_t         prefix,
    uint8_t          prefix_len,
    GError           **err)
{
    if (!ctx || !ctx->stl_buffer) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Context not initialized");
        return FALSE;
    }
    
    if (!check_capacity(ctx, err)) {
        return FALSE;
    }
    
    if (prefix_len > 32) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid IPv4 prefix length: %u", prefix_len);
        return FALSE;
    }
    
    /* Calculate offset for new entry */
    uint8_t *entry = ctx->stl_buffer + (ctx->entry_count * ctx->entry_size);
    
    /* Write entry: ingressInterface(4) + sourceIPv4Prefix(4) + sourceIPv4PrefixLength(1) */
    uint32_t interface_be = htonl(interface_id);
    memcpy(entry, &interface_be, 4);
    memcpy(entry + 4, &prefix, 4); /* prefix already in network byte order */
    entry[8] = prefix_len;
    
    ctx->entry_count++;
    return TRUE;
}

/* Add IPv6 Interface-to-Prefix entry */
gboolean sav_add_ipv6_interface_prefix(
    sav_record_ctx_t *ctx,
    uint32_t         interface_id,
    const uint8_t    *prefix,
    uint8_t          prefix_len,
    GError           **err)
{
    if (!ctx || !ctx->stl_buffer || !prefix) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid parameters to sav_add_ipv6_interface_prefix");
        return FALSE;
    }
    
    if (!check_capacity(ctx, err)) {
        return FALSE;
    }
    
    if (prefix_len > 128) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid IPv6 prefix length: %u", prefix_len);
        return FALSE;
    }
    
    /* Calculate offset for new entry */
    uint8_t *entry = ctx->stl_buffer + (ctx->entry_count * ctx->entry_size);
    
    /* Write entry: ingressInterface(4) + sourceIPv6Prefix(16) + sourceIPv6PrefixLength(1) */
    uint32_t interface_be = htonl(interface_id);
    memcpy(entry, &interface_be, 4);
    memcpy(entry + 4, prefix, 16);
    entry[20] = prefix_len;
    
    ctx->entry_count++;
    return TRUE;
}

/* Add IPv4 Prefix-to-Interface entry */
gboolean sav_add_ipv4_prefix_interface(
    sav_record_ctx_t *ctx,
    uint32_t         prefix,
    uint8_t          prefix_len,
    uint32_t         interface_id,
    GError           **err)
{
    if (!ctx || !ctx->stl_buffer) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Context not initialized");
        return FALSE;
    }
    
    if (!check_capacity(ctx, err)) {
        return FALSE;
    }
    
    if (prefix_len > 32) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid IPv4 prefix length: %u", prefix_len);
        return FALSE;
    }
    
    /* Calculate offset for new entry */
    uint8_t *entry = ctx->stl_buffer + (ctx->entry_count * ctx->entry_size);
    
    /* Write entry: sourceIPv4Prefix(4) + sourceIPv4PrefixLength(1) + ingressInterface(4) */
    memcpy(entry, &prefix, 4); /* prefix already in network byte order */
    entry[4] = prefix_len;
    uint32_t interface_be = htonl(interface_id);
    memcpy(entry + 5, &interface_be, 4);
    
    ctx->entry_count++;
    return TRUE;
}

/* Add IPv6 Prefix-to-Interface entry */
gboolean sav_add_ipv6_prefix_interface(
    sav_record_ctx_t *ctx,
    const uint8_t    *prefix,
    uint8_t          prefix_len,
    uint32_t         interface_id,
    GError           **err)
{
    if (!ctx || !ctx->stl_buffer || !prefix) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid parameters to sav_add_ipv6_prefix_interface");
        return FALSE;
    }
    
    if (!check_capacity(ctx, err)) {
        return FALSE;
    }
    
    if (prefix_len > 128) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid IPv6 prefix length: %u", prefix_len);
        return FALSE;
    }
    
    /* Calculate offset for new entry */
    uint8_t *entry = ctx->stl_buffer + (ctx->entry_count * ctx->entry_size);
    
    /* Write entry: sourceIPv6Prefix(16) + sourceIPv6PrefixLength(1) + ingressInterface(4) */
    memcpy(entry, prefix, 16);
    entry[16] = prefix_len;
    uint32_t interface_be = htonl(interface_id);
    memcpy(entry + 17, &interface_be, 4);
    
    ctx->entry_count++;
    return TRUE;
}

/* Export a complete SAV record */
gboolean sav_export_record(
    sav_record_ctx_t *ctx,
    fBuf_t           *exporter,
    uint64_t         timestamp_ms,
    uint8_t          rule_type,
    uint8_t          target_type,
    uint8_t          policy_action,
    GError           **err)
{
    if (!ctx || !exporter) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid parameters to sav_export_record");
        return FALSE;
    }
    
    if (!sav_validate_policy_action(policy_action)) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid policy_action: %u", policy_action);
        return FALSE;
    }
    
    /* Internal template should already be set by sav_create_file_exporter */
    /* Prepare SAV main record structure */
    sav_data_record_t record;
    memset(&record, 0, sizeof(record));
    
    record.observationTimeMilliseconds = timestamp_ms;
    record.savRuleType = rule_type;
    record.savTargetType = target_type;
    record.savPolicyAction = policy_action;
    
    /* Initialize SubTemplateList for export
     * CRITICAL: Template pointer MUST be valid even for empty lists!
     * Based on testing: libfixbuf will pre-allocate buffer if entry_count > 0
     */
    g_debug("sav_export_record: sub_tmpl_id=%u, entry_count=%u, entry_size=%zu",
            ctx->sub_tmpl_id, ctx->entry_count, ctx->entry_size);
    
    /* Determine semantic value based on rule_type per RFC6313 and draft-cao-opsawg-ipfix-sav
     * - Allowlist (rule_type=1): use allOf (0x03) - packet didn't match any rule in list
     * - Blocklist (rule_type=2): use exactlyOneOf (0x01) - packet matched this specific rule
     */
    uint8_t semantic;
    if (rule_type == 1) {
        semantic = 0x03;  /* allOf - for allowlist, export all rules that were checked */
    } else if (rule_type == 2) {
        semantic = 0x01;  /* exactlyOneOf - for blocklist, export the matched rule */
    } else {
        semantic = 0x00;  /* undefined - for unknown rule types */
    }
    
    fbSubTemplateListInit(&record.savMatchedContentList, 
                          semantic,           /* semantic value per RFC6313 */
                          ctx->sub_tmpl_id,   /* external template ID */
                          ctx->sub_tmpl,      /* internal template pointer, MUST NOT be NULL */
                          ctx->entry_count);


    
    /* If we have entries, copy them to the pre-allocated buffer */
    if (ctx->entry_count > 0 && ctx->stl_buffer) {
        void *stl_data = fbSubTemplateListGetDataPtr(&record.savMatchedContentList);
        if (!stl_data) {
            g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                        "SubTemplateList data pointer is NULL (expected pre-allocated)");
            fbSubTemplateListClear(&record.savMatchedContentList);
            return FALSE;
        }
        /* Copy all entries at once */
        memcpy(stl_data, ctx->stl_buffer, ctx->entry_count * ctx->entry_size);
        g_debug("  Copied %u entries (%zu bytes total)", ctx->entry_count, 
                ctx->entry_count * ctx->entry_size);
        
        /* Debug: Print first entry */
        if (ctx->entry_size == 9) {
            uint32_t *iface = (uint32_t *)stl_data;
            uint32_t *prefix = (uint32_t *)(stl_data + 4);
            uint8_t *len = (uint8_t *)(stl_data + 8);
            g_debug("  First entry: iface=%u, prefix=0x%08x, len=%u", 
                    ntohl(*iface), ntohl(*prefix), *len);
        }
    }
    
    /* Verify STL before export */
    uint16_t stl_tmpl_id = fbSubTemplateListGetTemplateID(&record.savMatchedContentList);
    uint32_t stl_count = fbSubTemplateListCountElements(&record.savMatchedContentList);
    g_debug("  Before fBufAppend: STL tmpl_id=%u, count=%u", stl_tmpl_id, stl_count);
    
    /* CRITICAL: Set both internal and export templates before fBufAppend
     * Using libfixbuf 3.x API */
    if (!fBufSetTemplatesForExport(exporter, SAV_MAIN_TEMPLATE_ID, err)) {
        fbSubTemplateListClear(&record.savMatchedContentList);
        return FALSE;
    }
    
    /* Append record to exporter */
    gboolean result = fBufAppend(exporter, (uint8_t *)&record, sizeof(record), err);
    
    /* Clean up SubTemplateList */
    fbSubTemplateListClear(&record.savMatchedContentList);
    
    return result;
}

/* Create file exporter */
fBuf_t* sav_create_file_exporter(
    fbInfoModel_t *model,
    fbSession_t   *session,
    const char    *filename,
    GError        **err)
{
    if (!model || !session || !filename) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Invalid parameters to sav_create_file_exporter");
        return NULL;
    }
    
    /* Create exporter */
    fbExporter_t *exporter = fbExporterAllocFile(filename);
    if (!exporter) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to create file exporter for %s", filename);
        return NULL;
    }
    
    /* Create buffer */
    fBuf_t *fbuf = fBufAllocForExport(session, exporter);
    if (!fbuf) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "Failed to create export buffer");
        return NULL;
    }
    
    /* Verify fBuf has the correct session */
    fbSession_t *fbuf_session = fBufGetSession(fbuf);
    if (fbuf_session != session) {
        g_warning("fBuf session (%p) != input session (%p)", fbuf_session, session);
    } else {
        g_debug("fBuf session matches input session");
    }
    
    /* Test: Can we get template 901 from the fBuf's session? */
    GError *tmp_err = NULL;
    fbTemplate_t *test_tmpl = fbSessionGetTemplate(fbuf_session, FALSE, 
                                                     SAV_TMPL_IPV4_INTERFACE_PREFIX, 
                                                     &tmp_err);
    if (!test_tmpl) {
        g_warning("Cannot get template 901 from fBuf session: %s",
                  tmp_err ? tmp_err->message : "unknown");
        if (tmp_err) g_error_free(tmp_err);
    } else {
        g_debug("Template 901 found in fBuf session (external)");
    }
    
    /* Now also check internal template */
    tmp_err = NULL;
    test_tmpl = fbSessionGetTemplate(fbuf_session, TRUE, 
                                      SAV_TMPL_IPV4_INTERFACE_PREFIX, 
                                      &tmp_err);
    if (!test_tmpl) {
        g_warning("Cannot get template 901 as INTERNAL from fBuf session: %s",
                  tmp_err ? tmp_err->message : "unknown");
        if (tmp_err) g_error_free(tmp_err);
    } else {
        g_debug("Template 901 found in fBuf session (internal)");
    }
    
    /* Mark all sub-templates as export templates */
    uint16_t sub_tmpl_ids[] = {
        SAV_TMPL_IPV4_INTERFACE_PREFIX,
        SAV_TMPL_IPV4_PREFIX_INTERFACE,
        SAV_TMPL_IPV6_INTERFACE_PREFIX,
        SAV_TMPL_IPV6_PREFIX_INTERFACE
    };
    for (size_t i = 0; i < sizeof(sub_tmpl_ids) / sizeof(sub_tmpl_ids[0]); i++) {
        tmp_err = NULL;
        if (!fBufSetExportTemplate(fbuf, sub_tmpl_ids[i], &tmp_err)) {
            g_debug("Warning: Could not set export template %u: %s", 
                    sub_tmpl_ids[i], tmp_err ? tmp_err->message : "unknown");
            if (tmp_err) g_error_free(tmp_err);
        } else {
            g_debug("Set export template %u", sub_tmpl_ids[i]);
        }
    }
    
    /* Export all templates to file */
    g_debug("Exporting all templates to file...");
    if (!fbSessionExportTemplates(fbuf_session, err)) {
        fBufFree(fbuf);
        return NULL;
    }
    g_debug("Templates exported successfully");
    
    /* Set main template using the new libfixbuf 3.x API */
    if (!fBufSetTemplatesForExport(fbuf, SAV_MAIN_TEMPLATE_ID, err)) {
        fBufFree(fbuf);
        return NULL;
    }
    g_debug("Main template %u set as current (both internal and external)", SAV_MAIN_TEMPLATE_ID);
    
    return fbuf;
}

/* Export templates */
gboolean sav_export_templates(
    fBuf_t  *exporter,
    GError **err)
{
    if (!exporter) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP,
                    "NULL exporter parameter");
        return FALSE;
    }
    
    /* Templates are now exported automatically in sav_create_file_exporter()
     * This function is kept for API compatibility but does nothing. */
    g_debug("sav_export_templates: Templates already exported during exporter creation");
    
    return TRUE;
}

/* Close exporter */
void sav_close_exporter(fBuf_t *exporter)
{
    if (exporter) {
        fBufEmit(exporter, NULL);
        fBufFree(exporter);
    }
}
