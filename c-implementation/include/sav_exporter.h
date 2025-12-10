/**
 * @file sav_exporter.h
 * @brief SAV IPFIX Exporter API
 * 
 * This module provides functions to create and export SAV IPFIX records.
 * It handles the complexity of SubTemplateList creation and management.
 */

#ifndef SAV_EXPORTER_H
#define SAV_EXPORTER_H

#include <stdint.h>
#include <stdbool.h>
#include <fixbuf/public.h>
#include "sav_ie_definitions.h"

/* Maximum number of entries in a single SubTemplateList */
#define SAV_MAX_LIST_ENTRIES 100

/**
 * SAV Record Context
 * 
 * This structure manages the state needed to build a complete SAV IPFIX record.
 * It handles the SubTemplateList buffer and ensures proper memory management.
 */
typedef struct sav_record_ctx {
    fbInfoModel_t   *model;           /* Info model with SAV IEs */
    fbSession_t     *session;         /* Session with templates registered */
    fbTemplate_t    *main_tmpl;       /* Main template (400) */
    fbTemplate_t    *sub_tmpl;        /* Current sub-template (901-904) */
    uint16_t        sub_tmpl_id;      /* Sub-template ID for convenience */
    uint8_t         *stl_buffer;      /* Buffer for SubTemplateList entries */
    size_t          stl_capacity;     /* Buffer capacity in bytes */
    size_t          entry_size;       /* Size of one entry in current sub-template */
    uint32_t        entry_count;      /* Number of entries in list */
} sav_record_ctx_t;

/**
 * Initialize a SAV record context
 * 
 * @param ctx          Pointer to context structure to initialize
 * @param model        Info model with SAV IEs registered
 * @param session      Session with SAV templates registered
 * @param rule_type    SAV rule type (allowlist=1, blocklist=2)
 * @param target_type  SAV target type (interface-prefix=1, prefix-interface=2)
 * @param err          Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_record_ctx_init(
    sav_record_ctx_t *ctx,
    fbInfoModel_t    *model,
    fbSession_t      *session,
    uint8_t          rule_type,
    uint8_t          target_type,
    GError           **err);

/**
 * Clean up SAV record context
 * 
 * Frees allocated buffers. Does NOT free model or session.
 * 
 * @param ctx  Context to clean up
 */
void sav_record_ctx_cleanup(sav_record_ctx_t *ctx);

/**
 * Add an IPv4 Interface-to-Prefix entry to the SubTemplateList
 * 
 * @param ctx              Record context
 * @param interface_id     Ingress interface ID
 * @param prefix           IPv4 prefix (network byte order)
 * @param prefix_len       Prefix length (0-32)
 * @param err              Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_add_ipv4_interface_prefix(
    sav_record_ctx_t *ctx,
    uint32_t         interface_id,
    uint32_t         prefix,
    uint8_t          prefix_len,
    GError           **err);

/**
 * Add an IPv6 Interface-to-Prefix entry to the SubTemplateList
 * 
 * @param ctx              Record context
 * @param interface_id     Ingress interface ID
 * @param prefix           IPv6 prefix (16 bytes, network byte order)
 * @param prefix_len       Prefix length (0-128)
 * @param err              Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_add_ipv6_interface_prefix(
    sav_record_ctx_t *ctx,
    uint32_t         interface_id,
    const uint8_t    *prefix,
    uint8_t          prefix_len,
    GError           **err);

/**
 * Add an IPv4 Prefix-to-Interface entry to the SubTemplateList
 * 
 * @param ctx              Record context
 * @param prefix           IPv4 prefix (network byte order)
 * @param prefix_len       Prefix length (0-32)
 * @param interface_id     Ingress interface ID
 * @param err              Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_add_ipv4_prefix_interface(
    sav_record_ctx_t *ctx,
    uint32_t         prefix,
    uint8_t          prefix_len,
    uint32_t         interface_id,
    GError           **err);

/**
 * Add an IPv6 Prefix-to-Interface entry to the SubTemplateList
 * 
 * @param ctx              Record context
 * @param prefix           IPv6 prefix (16 bytes, network byte order)
 * @param prefix_len       Prefix length (0-128)
 * @param interface_id     Ingress interface ID
 * @param err              Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_add_ipv6_prefix_interface(
    sav_record_ctx_t *ctx,
    const uint8_t    *prefix,
    uint8_t          prefix_len,
    uint32_t         interface_id,
    GError           **err);

/**
 * Export a complete SAV record to an IPFIX file
 * 
 * This function writes a complete SAV record (template 400) with its SubTemplateList
 * to an IPFIX exporter/file buffer.
 * 
 * @param ctx                Context with populated SubTemplateList
 * @param exporter           IPFIX exporter/fbuf
 * @param timestamp_ms       Observation timestamp in milliseconds
 * @param rule_type          SAV rule type (allowlist=1, blocklist=2)
 * @param target_type        SAV target type (interface-prefix=1, prefix-interface=2)
 * @param policy_action      Policy action (drop=1, rate-limit=2, redirect=3)
 * @param err                Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_export_record(
    sav_record_ctx_t *ctx,
    fBuf_t           *exporter,
    uint64_t         timestamp_ms,
    uint8_t          rule_type,
    uint8_t          target_type,
    uint8_t          policy_action,
    GError           **err);

/**
 * Create an IPFIX exporter writing to a file
 * 
 * Convenience function to create a file-based exporter with SAV templates.
 * 
 * @param model        Info model with SAV IEs
 * @param session      Session with SAV templates
 * @param filename     Output filename
 * @param err          Error structure
 * 
 * @return Exporter buffer on success, NULL on error
 */
fBuf_t* sav_create_file_exporter(
    fbInfoModel_t *model,
    fbSession_t   *session,
    const char    *filename,
    GError        **err);

/**
 * Export all SAV templates to the exporter
 * 
 * MUST be called after sav_create_file_exporter() and before the first
 * sav_export_record() call. This writes all template definitions to the file.
 * 
 * @param exporter  Exporter buffer (from sav_create_file_exporter)
 * @param err       Error structure
 * 
 * @return TRUE on success, FALSE on error
 */
gboolean sav_export_templates(
    fBuf_t  *exporter,
    GError **err);

/**
 * Close and free an IPFIX exporter
 * 
 * @param exporter  Exporter to close
 */
void sav_close_exporter(fBuf_t *exporter);

#endif /* SAV_EXPORTER_H */
