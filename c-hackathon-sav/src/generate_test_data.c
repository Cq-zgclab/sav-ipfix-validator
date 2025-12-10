/**
 * @file generate_test_data.c
 * @brief Generate SAV IPFIX test data with IPv4 and IPv6 scenarios
 * 
 * Features:
 * - IPv4 packet header -> IPv4 SubTemplateList (allowlist/blocklist)
 * - IPv6 packet header -> IPv6 SubTemplateList (allowlist/blocklist)
 * - Five-tuple in message header (srcIP, dstIP, srcPort, dstPort, protocol)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fixbuf/public.h>

/* Enterprise number for SAV IEs (draft-cao-opsawg-ipfix-sav-01) */
#define SAV_ENTERPRISE_NUMBER 9999

/* SAV Information Element IDs */
#define IE_SAV_TIMESTAMP           1001
#define IE_SAV_DEVICE_ID           1002
#define IE_SAV_VERSION             1003
#define IE_SAV_MESSAGE             1004
#define IE_SAV_RULE_TYPE           1005
#define IE_SAV_ALLOWLIST_RULES     1010
#define IE_SAV_BLOCKLIST_RULES     1011
#define IE_SAV_PREFIX_RULES        1012
#define IE_SAV_ASPATH_RULES        1013

/* IPv4 AllowlistRule IEs */
#define IE_ALLOWLIST_SOURCE_IPV4_PREFIX    1020
#define IE_ALLOWLIST_SOURCE_IPV4_PREFIX_LEN 1021
#define IE_ALLOWLIST_INTERFACE             1022
#define IE_ALLOWLIST_TIMESTAMP             1023

/* IPv6 AllowlistRule IEs */
#define IE_ALLOWLIST_SOURCE_IPV6_PREFIX    1024
#define IE_ALLOWLIST_SOURCE_IPV6_PREFIX_LEN 1025

/* IPv4 BlocklistRule IEs */
#define IE_BLOCKLIST_SOURCE_IPV4_PREFIX    1030
#define IE_BLOCKLIST_SOURCE_IPV4_PREFIX_LEN 1031
#define IE_BLOCKLIST_INTERFACE             1032
#define IE_BLOCKLIST_TIMESTAMP             1033

/* IPv6 BlocklistRule IEs */
#define IE_BLOCKLIST_SOURCE_IPV6_PREFIX    1034
#define IE_BLOCKLIST_SOURCE_IPV6_PREFIX_LEN 1035

/* Five-tuple IEs (standard IPFIX) */
#define IE_SOURCE_IPV4_ADDRESS       8   /* sourceIPv4Address */
#define IE_DESTINATION_IPV4_ADDRESS 12   /* destinationIPv4Address */
#define IE_SOURCE_IPV6_ADDRESS      27   /* sourceIPv6Address */
#define IE_DESTINATION_IPV6_ADDRESS 28   /* destinationIPv6Address */
#define IE_SOURCE_TRANSPORT_PORT     7   /* sourceTransportPort */
#define IE_DESTINATION_TRANSPORT_PORT 11 /* destinationTransportPort */
#define IE_PROTOCOL_IDENTIFIER       4   /* protocolIdentifier */

/* Template IDs */
#define TID_SAV_RECORD_IPV4           500  /* Main template with IPv4 five-tuple */
#define TID_SAV_RECORD_IPV6           501  /* Main template with IPv6 five-tuple */
#define TID_ALLOWLIST_IPV4_SUB        901
#define TID_ALLOWLIST_IPV6_SUB        902
#define TID_BLOCKLIST_IPV4_SUB        903
#define TID_BLOCKLIST_IPV6_SUB        904

/* Data structures */
typedef struct {
    uint64_t timestamp;
    uint32_t device_id;
    uint8_t  version;
    uint8_t  message;
    uint8_t  rule_type;
} SAVCommonFields;

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;
} FiveTupleIPv4;

typedef struct {
    uint8_t src_ip[16];
    uint8_t dst_ip[16];
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;
} FiveTupleIPv6;

typedef struct {
    uint32_t source_prefix;
    uint8_t  prefix_len;
    fbVarfield_t interface;
    uint64_t timestamp;
} AllowlistRuleIPv4;

typedef struct {
    uint8_t  source_prefix[16];
    uint8_t  prefix_len;
    fbVarfield_t interface;
    uint64_t timestamp;
} AllowlistRuleIPv6;

/* Register SAV Information Elements */
static void register_sav_ies(fbInfoModel_t *model) {
    /* SAV common fields */
    fbInfoModelAddElement(model, FB_IE_INIT("savTimestamp", SAV_ENTERPRISE_NUMBER, IE_SAV_TIMESTAMP, 8, FB_IE_F_ENDIAN));
    fbInfoModelAddElement(model, FB_IE_INIT("savDeviceId", SAV_ENTERPRISE_NUMBER, IE_SAV_DEVICE_ID, 4, FB_IE_F_ENDIAN));
    fbInfoModelAddElement(model, FB_IE_INIT("savVersion", SAV_ENTERPRISE_NUMBER, IE_SAV_VERSION, 1, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("savMessage", SAV_ENTERPRISE_NUMBER, IE_SAV_MESSAGE, 1, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("savRuleType", SAV_ENTERPRISE_NUMBER, IE_SAV_RULE_TYPE, 1, 0));
    
    /* SubTemplateList fields */
    fbInfoModelAddElement(model, FB_IE_INIT("savAllowlistRules", SAV_ENTERPRISE_NUMBER, IE_SAV_ALLOWLIST_RULES, FB_IE_VARLEN, FB_IE_LIST));
    fbInfoModelAddElement(model, FB_IE_INIT("savBlocklistRules", SAV_ENTERPRISE_NUMBER, IE_SAV_BLOCKLIST_RULES, FB_IE_VARLEN, FB_IE_LIST));
    
    /* IPv4 AllowlistRule */
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistSourceIPv4Prefix", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_SOURCE_IPV4_PREFIX, 4, FB_IE_F_ENDIAN));
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistSourceIPv4PrefixLen", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_SOURCE_IPV4_PREFIX_LEN, 1, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistInterface", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_INTERFACE, FB_IE_VARLEN, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistTimestamp", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_TIMESTAMP, 8, FB_IE_F_ENDIAN));
    
    /* IPv6 AllowlistRule */
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistSourceIPv6Prefix", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_SOURCE_IPV6_PREFIX, 16, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("allowlistSourceIPv6PrefixLen", SAV_ENTERPRISE_NUMBER, IE_ALLOWLIST_SOURCE_IPV6_PREFIX_LEN, 1, 0));
    
    /* IPv4 BlocklistRule */
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistSourceIPv4Prefix", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_SOURCE_IPV4_PREFIX, 4, FB_IE_F_ENDIAN));
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistSourceIPv4PrefixLen", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_SOURCE_IPV4_PREFIX_LEN, 1, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistInterface", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_INTERFACE, FB_IE_VARLEN, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistTimestamp", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_TIMESTAMP, 8, FB_IE_F_ENDIAN));
    
    /* IPv6 BlocklistRule */
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistSourceIPv6Prefix", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_SOURCE_IPV6_PREFIX, 16, 0));
    fbInfoModelAddElement(model, FB_IE_INIT("blocklistSourceIPv6PrefixLen", SAV_ENTERPRISE_NUMBER, IE_BLOCKLIST_SOURCE_IPV6_PREFIX_LEN, 1, 0));
}

/* Create IPv4 main template (with five-tuple) */
static fbTemplate_t *create_ipv4_main_template(fbInfoModel_t *model, GError **err) {
    fbInfoElement_t *ie_array[13];
    int idx = 0;
    
    /* Five-tuple (IPv4) */
    ie_array[idx++] = fbInfoModelGetElement(model, "sourceIPv4Address", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "destinationIPv4Address", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "sourceTransportPort", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "destinationTransportPort", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "protocolIdentifier", 0);
    
    /* SAV common fields */
    ie_array[idx++] = fbInfoModelGetElement(model, "savTimestamp", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savDeviceId", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savVersion", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savMessage", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savRuleType", SAV_ENTERPRISE_NUMBER);
    
    /* SubTemplateLists */
    ie_array[idx++] = fbInfoModelGetElement(model, "savAllowlistRules", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savBlocklistRules", SAV_ENTERPRISE_NUMBER);
    
    return fbTemplateAlloc(model, ie_array, idx, TID_SAV_RECORD_IPV4, err);
}

/* Create IPv6 main template (with five-tuple) */
static fbTemplate_t *create_ipv6_main_template(fbInfoModel_t *model, GError **err) {
    fbInfoElement_t *ie_array[13];
    int idx = 0;
    
    /* Five-tuple (IPv6) */
    ie_array[idx++] = fbInfoModelGetElement(model, "sourceIPv6Address", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "destinationIPv6Address", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "sourceTransportPort", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "destinationTransportPort", 0);
    ie_array[idx++] = fbInfoModelGetElement(model, "protocolIdentifier", 0);
    
    /* SAV common fields */
    ie_array[idx++] = fbInfoModelGetElement(model, "savTimestamp", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savDeviceId", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savVersion", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savMessage", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savRuleType", SAV_ENTERPRISE_NUMBER);
    
    /* SubTemplateLists */
    ie_array[idx++] = fbInfoModelGetElement(model, "savAllowlistRules", SAV_ENTERPRISE_NUMBER);
    ie_array[idx++] = fbInfoModelGetElement(model, "savBlocklistRules", SAV_ENTERPRISE_NUMBER);
    
    return fbTemplateAlloc(model, ie_array, idx, TID_SAV_RECORD_IPV6, err);
}

/* Create IPv4 allowlist sub-template */
static fbTemplate_t *create_ipv4_allowlist_template(fbInfoModel_t *model, GError **err) {
    fbInfoElement_t *ie_array[4];
    ie_array[0] = fbInfoModelGetElement(model, "allowlistSourceIPv4Prefix", SAV_ENTERPRISE_NUMBER);
    ie_array[1] = fbInfoModelGetElement(model, "allowlistSourceIPv4PrefixLen", SAV_ENTERPRISE_NUMBER);
    ie_array[2] = fbInfoModelGetElement(model, "allowlistInterface", SAV_ENTERPRISE_NUMBER);
    ie_array[3] = fbInfoModelGetElement(model, "allowlistTimestamp", SAV_ENTERPRISE_NUMBER);
    
    return fbTemplateAlloc(model, ie_array, 4, TID_ALLOWLIST_IPV4_SUB, err);
}

/* Create IPv6 allowlist sub-template */
static fbTemplate_t *create_ipv6_allowlist_template(fbInfoModel_t *model, GError **err) {
    fbInfoElement_t *ie_array[4];
    ie_array[0] = fbInfoModelGetElement(model, "allowlistSourceIPv6Prefix", SAV_ENTERPRISE_NUMBER);
    ie_array[1] = fbInfoModelGetElement(model, "allowlistSourceIPv6PrefixLen", SAV_ENTERPRISE_NUMBER);
    ie_array[2] = fbInfoModelGetElement(model, "allowlistInterface", SAV_ENTERPRISE_NUMBER);
    ie_array[3] = fbInfoModelGetElement(model, "allowlistTimestamp", SAV_ENTERPRISE_NUMBER);
    
    return fbTemplateAlloc(model, ie_array, 4, TID_ALLOWLIST_IPV6_SUB, err);
}

/* Export IPv4 SAV record */
static void export_ipv4_record(fBuf_t *fbuf, fbTemplate_t *main_tmpl, fbTemplate_t *sub_tmpl) {
    GError *err = NULL;
    uint8_t record_buf[2048];
    memset(record_buf, 0, sizeof(record_buf));
    
    uint8_t *ptr = record_buf;
    
    /* Five-tuple (IPv4) */
    uint32_t src_ip = htonl(0xC0A80101);  /* 192.168.1.1 */
    uint32_t dst_ip = htonl(0xC0A80102);  /* 192.168.1.2 */
    uint16_t src_port = htons(12345);
    uint16_t dst_port = htons(80);
    uint8_t protocol = 6;  /* TCP */
    
    memcpy(ptr, &src_ip, 4); ptr += 4;
    memcpy(ptr, &dst_ip, 4); ptr += 4;
    memcpy(ptr, &src_port, 2); ptr += 2;
    memcpy(ptr, &dst_port, 2); ptr += 2;
    memcpy(ptr, &protocol, 1); ptr += 1;
    
    /* SAV common fields */
    uint64_t timestamp = (uint64_t)time(NULL) * 1000;
    uint32_t device_id = htonl(0x0A000001);  /* 10.0.0.1 */
    uint8_t version = 1;
    uint8_t message = 1;  /* rule_update */
    uint8_t rule_type = 0x03;  /* allowlist */
    
    memcpy(ptr, &timestamp, 8); ptr += 8;
    memcpy(ptr, &device_id, 4); ptr += 4;
    memcpy(ptr, &version, 1); ptr += 1;
    memcpy(ptr, &message, 1); ptr += 1;
    memcpy(ptr, &rule_type, 1); ptr += 1;
    
    /* SubTemplateList (allowlist) */
    fbSubTemplateList_t stl;
    fbSubTemplateListInit(&stl, 0x03, TID_ALLOWLIST_IPV4_SUB, sub_tmpl, 2);
    
    /* Rule 1 */
    AllowlistRuleIPv4 *rule1 = (AllowlistRuleIPv4 *)fbSubTemplateListGetNextPtr(&stl, NULL);
    rule1->source_prefix = htonl(0xC0A80100);  /* 192.168.1.0/24 */
    rule1->prefix_len = 24;
    rule1->interface.buf = (uint8_t *)"eth0";
    rule1->interface.len = 4;
    rule1->timestamp = timestamp;
    
    /* Rule 2 */
    AllowlistRuleIPv4 *rule2 = (AllowlistRuleIPv4 *)fbSubTemplateListGetNextPtr(&stl, rule1);
    rule2->source_prefix = htonl(0x0A000000);  /* 10.0.0.0/8 */
    rule2->prefix_len = 8;
    rule2->interface.buf = (uint8_t *)"eth1";
    rule2->interface.len = 4;
    rule2->timestamp = timestamp;
    
    /* Encode STL to buffer */
    memcpy(ptr, &stl, sizeof(fbSubTemplateList_t));
    ptr += sizeof(fbSubTemplateList_t);
    
    /* Empty blocklist STL */
    fbSubTemplateList_t stl_empty;
    fbSubTemplateListInit(&stl_empty, 0, 0, NULL, 0);
    memcpy(ptr, &stl_empty, sizeof(fbSubTemplateList_t));
    
    /* Set template and export */
    if (!fBufSetInternalTemplate(fbuf, main_tmpl, &err)) {
        fprintf(stderr, "Error setting internal template: %s\n", err->message);
        g_clear_error(&err);
        return;
    }
    
    if (!fBufAppend(fbuf, record_buf, sizeof(record_buf), &err)) {
        fprintf(stderr, "Error appending record: %s\n", err->message);
        g_clear_error(&err);
        return;
    }
    
    fbSubTemplateListClear(&stl);
}

/* Export IPv6 SAV record */
static void export_ipv6_record(fBuf_t *fbuf, fbTemplate_t *main_tmpl, fbTemplate_t *sub_tmpl) {
    GError *err = NULL;
    uint8_t record_buf[2048];
    memset(record_buf, 0, sizeof(record_buf));
    
    uint8_t *ptr = record_buf;
    
    /* Five-tuple (IPv6) */
    uint8_t src_ip[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};  /* 2001:db8::1 */
    uint8_t dst_ip[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2};  /* 2001:db8::2 */
    uint16_t src_port = htons(54321);
    uint16_t dst_port = htons(443);
    uint8_t protocol = 6;  /* TCP */
    
    memcpy(ptr, src_ip, 16); ptr += 16;
    memcpy(ptr, dst_ip, 16); ptr += 16;
    memcpy(ptr, &src_port, 2); ptr += 2;
    memcpy(ptr, &dst_port, 2); ptr += 2;
    memcpy(ptr, &protocol, 1); ptr += 1;
    
    /* SAV common fields */
    uint64_t timestamp = (uint64_t)time(NULL) * 1000;
    uint32_t device_id = htonl(0x0A000002);  /* 10.0.0.2 */
    uint8_t version = 1;
    uint8_t message = 1;  /* rule_update */
    uint8_t rule_type = 0x03;  /* allowlist */
    
    memcpy(ptr, &timestamp, 8); ptr += 8;
    memcpy(ptr, &device_id, 4); ptr += 4;
    memcpy(ptr, &version, 1); ptr += 1;
    memcpy(ptr, &message, 1); ptr += 1;
    memcpy(ptr, &rule_type, 1); ptr += 1;
    
    /* SubTemplateList (allowlist IPv6) */
    fbSubTemplateList_t stl;
    fbSubTemplateListInit(&stl, 0x03, TID_ALLOWLIST_IPV6_SUB, sub_tmpl, 2);
    
    /* Rule 1 */
    AllowlistRuleIPv6 *rule1 = (AllowlistRuleIPv6 *)fbSubTemplateListGetNextPtr(&stl, NULL);
    memcpy(rule1->source_prefix, src_ip, 16);
    rule1->prefix_len = 64;
    rule1->interface.buf = (uint8_t *)"eth0";
    rule1->interface.len = 4;
    rule1->timestamp = timestamp;
    
    /* Rule 2 */
    AllowlistRuleIPv6 *rule2 = (AllowlistRuleIPv6 *)fbSubTemplateListGetNextPtr(&stl, rule1);
    uint8_t prefix2[16] = {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  /* fe80::/10 */
    memcpy(rule2->source_prefix, prefix2, 16);
    rule2->prefix_len = 10;
    rule2->interface.buf = (uint8_t *)"eth1";
    rule2->interface.len = 4;
    rule2->timestamp = timestamp;
    
    /* Encode STL to buffer */
    memcpy(ptr, &stl, sizeof(fbSubTemplateList_t));
    ptr += sizeof(fbSubTemplateList_t);
    
    /* Empty blocklist STL */
    fbSubTemplateList_t stl_empty;
    fbSubTemplateListInit(&stl_empty, 0, 0, NULL, 0);
    memcpy(ptr, &stl_empty, sizeof(fbSubTemplateList_t));
    
    /* Set template and export */
    if (!fBufSetInternalTemplate(fbuf, main_tmpl, &err)) {
        fprintf(stderr, "Error setting internal template: %s\n", err->message);
        g_clear_error(&err);
        return;
    }
    
    if (!fBufAppend(fbuf, record_buf, sizeof(record_buf), &err)) {
        fprintf(stderr, "Error appending record: %s\n", err->message);
        g_clear_error(&err);
        return;
    }
    
    fbSubTemplateListClear(&stl);
}

int main(int argc, char *argv[]) {
    GError *err = NULL;
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output.ipfix>\n", argv[0]);
        return 1;
    }
    
    /* Initialize info model */
    fbInfoModel_t *model = fbInfoModelAlloc();
    register_sav_ies(model);
    
    /* Create session */
    fbSession_t *session = fbSessionAlloc(model);
    
    /* Create templates */
    fbTemplate_t *ipv4_main = create_ipv4_main_template(model, &err);
    fbTemplate_t *ipv6_main = create_ipv6_main_template(model, &err);
    fbTemplate_t *ipv4_allow_sub = create_ipv4_allowlist_template(model, &err);
    fbTemplate_t *ipv6_allow_sub = create_ipv6_allowlist_template(model, &err);
    
    /* Add templates to session */
    fbSessionAddTemplate(session, TRUE, TID_SAV_RECORD_IPV4, ipv4_main, &err);
    fbSessionAddTemplate(session, TRUE, TID_SAV_RECORD_IPV6, ipv6_main, &err);
    fbSessionAddTemplate(session, TRUE, TID_ALLOWLIST_IPV4_SUB, ipv4_allow_sub, &err);
    fbSessionAddTemplate(session, TRUE, TID_ALLOWLIST_IPV6_SUB, ipv6_allow_sub, &err);
    
    /* Create exporter */
    fbExporter_t *exporter = fbExporterAllocFile(argv[1]);
    fBuf_t *fbuf = fBufAllocForExport(session, exporter);
    
    /* Export IPv4 record */
    printf("Exporting IPv4 SAV record...\n");
    export_ipv4_record(fbuf, ipv4_main, ipv4_allow_sub);
    
    /* Export IPv6 record */
    printf("Exporting IPv6 SAV record...\n");
    export_ipv6_record(fbuf, ipv6_main, ipv6_allow_sub);
    
    /* Emit and cleanup */
    fBufEmit(fbuf, &err);
    fBufFree(fbuf);
    fbSessionFree(session);
    
    printf("✅ Test data generated: %s\n", argv[1]);
    printf("   - IPv4 record with five-tuple (192.168.1.1 -> 192.168.1.2)\n");
    printf("   - IPv6 record with five-tuple (2001:db8::1 -> 2001:db8::2)\n");
    
    return 0;
}
