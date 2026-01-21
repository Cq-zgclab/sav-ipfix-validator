/**
 * @file sav_record_parser.c
 * @brief Parse SAV IPFIX records and output JSON for web visualization
 * 
 * Features:
 * - Parse SAV records from IPFIX file
 * - Extract common fields + SubTemplateList
 * - Output JSON format for web frontend
 * - Support for allowlist/blocklist/prefix/aspath rules
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fixbuf/public.h>
#include <jansson.h>
#include <arpa/inet.h>

/* SAV Enterprise Number */
#define SAV_ENTERPRISE_NUMBER 9999

/* SAV Information Element IDs */
#define IE_SAV_TIMESTAMP           1001
#define IE_SAV_DEVICE_ID           1002
#define IE_SAV_VERSION             1003
#define IE_SAV_MESSAGE             1004
#define IE_SAV_RULE_TYPE           1005
#define IE_SAV_TARGET_TYPE         1006
#define IE_SAV_POLICY_ACTION       1007

#define IE_SAV_ALLOWLIST_RULES     1010
#define IE_SAV_BLOCKLIST_RULES     1011
#define IE_SAV_PREFIX_RULES        1012
#define IE_SAV_ASPATH_RULES        1013

/* Sub-template IEs */
#define IE_INTERFACE_ID            1020
#define IE_SOURCE_PREFIX_V4        1021
#define IE_PREFIX_LENGTH           1022

/* Template IDs */
#define TID_SAV_MAIN              700
#define TID_SAV_SUB               600

/* Data structures */
typedef struct {
    uint64_t timestamp;
    uint32_t device_id;
    uint8_t  version;
    uint8_t  message;
    uint8_t  rule_type;
    uint8_t  target_type;
    uint8_t  policy_action;
    fbSubTemplateList_t allowlist;
    fbSubTemplateList_t blocklist;
    fbSubTemplateList_t prefix;
    fbSubTemplateList_t aspath;
} SAVRecord;

typedef struct {
    uint16_t interface_id;
    uint32_t source_prefix;
    uint8_t  prefix_length;
} SubRecord;

/* Register SAV IEs */
static fbInfoModelAddElement(fbInfoModel_t *model, const fbInfoElement_t *ie) {
    return TRUE;
}

static void register_sav_ies(fbInfoModel_t *model) {
    fbInfoElement_t ies[] = {
        FB_IE_INIT_FULL("savTimestamp", SAV_ENTERPRISE_NUMBER, IE_SAV_TIMESTAMP, 8, 0, 0, 0, FB_UNITS_MILLISECONDS, 0, 0, FB_INT_64, 0),
        FB_IE_INIT_FULL("savDeviceId", SAV_ENTERPRISE_NUMBER, IE_SAV_DEVICE_ID, 4, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_32, 0),
        FB_IE_INIT_FULL("savVersion", SAV_ENTERPRISE_NUMBER, IE_SAV_VERSION, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_INIT_FULL("savMessage", SAV_ENTERPRISE_NUMBER, IE_SAV_MESSAGE, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_INIT_FULL("savRuleType", SAV_ENTERPRISE_NUMBER, IE_SAV_RULE_TYPE, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_INIT_FULL("savTargetType", SAV_ENTERPRISE_NUMBER, IE_SAV_TARGET_TYPE, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_INIT_FULL("savPolicyAction", SAV_ENTERPRISE_NUMBER, IE_SAV_POLICY_ACTION, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_INIT_FULL("savAllowlistRules", SAV_ENTERPRISE_NUMBER, IE_SAV_ALLOWLIST_RULES, FB_IE_VARLEN, 0, 0, FB_IE_SEMANTIC_LIST, FB_UNITS_NONE, 0, 0, FB_SUB_TMPL_LIST, 0),
        FB_IE_INIT_FULL("savBlocklistRules", SAV_ENTERPRISE_NUMBER, IE_SAV_BLOCKLIST_RULES, FB_IE_VARLEN, 0, 0, FB_IE_SEMANTIC_LIST, FB_UNITS_NONE, 0, 0, FB_SUB_TMPL_LIST, 0),
        FB_IE_INIT_FULL("savPrefixRules", SAV_ENTERPRISE_NUMBER, IE_SAV_PREFIX_RULES, FB_IE_VARLEN, 0, 0, FB_IE_SEMANTIC_LIST, FB_UNITS_NONE, 0, 0, FB_SUB_TMPL_LIST, 0),
        FB_IE_INIT_FULL("savAspathRules", SAV_ENTERPRISE_NUMBER, IE_SAV_ASPATH_RULES, FB_IE_VARLEN, 0, 0, FB_IE_SEMANTIC_LIST, FB_UNITS_NONE, 0, 0, FB_SUB_TMPL_LIST, 0),
        FB_IE_INIT_FULL("interfaceId", SAV_ENTERPRISE_NUMBER, IE_INTERFACE_ID, 2, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_16, 0),
        FB_IE_INIT_FULL("sourcePrefixV4", SAV_ENTERPRISE_NUMBER, IE_SOURCE_PREFIX_V4, 4, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_IP4_ADDR, 0),
        FB_IE_INIT_FULL("prefixLength", SAV_ENTERPRISE_NUMBER, IE_PREFIX_LENGTH, 1, 0, 0, 0, FB_UNITS_NONE, 0, 0, FB_UINT_8, 0),
        FB_IE_NULL
    };
    
    fbInfoModelAddElementArray(model, ies);
}

/* Convert IP address to string */
static void ip_to_string(uint32_t ip, char *buf, size_t buflen) {
    struct in_addr addr;
    addr.s_addr = ip;
    snprintf(buf, buflen, "%s", inet_ntoa(addr));
}

/* Parse SAV records and output JSON */
static int parse_and_output_json(const char *input_file, const char *output_file) {
    GError *err = NULL;
    
    /* Initialize info model */
    fbInfoModel_t *model = fbInfoModelAlloc();
    register_sav_ies(model);
    
    /* Create session */
    fbSession_t *session = fbSessionAlloc(model);
    
    /* Create collector */
    fbCollector_t *collector = fbCollectorAllocFile(NULL, input_file, &err);
    if (!collector) {
        fprintf(stderr, "Failed to create collector: %s\n", err->message);
        g_clear_error(&err);
        return 1;
    }
    
    /* Create buffer */
    fBuf_t *fbuf = fBufAllocForCollection(session, collector);
    if (!fbuf) {
        fprintf(stderr, "Failed to create buffer\n");
        return 1;
    }
    
    /* Define internal template specs */
    fbInfoElementSpec_t main_spec[] = {
        {"savTimestamp", 8, 0},
        {"savRuleType", 1, 0},
        {"savTargetType", 1, 0},
        {"savPolicyAction", 1, 0},
        {"savAllowlistRules", FB_IE_VARLEN, 0},
        FB_IESPEC_NULL
    };
    
    fbInfoElementSpec_t sub_spec[] = {
        {"interfaceId", 2, 0},
        {"sourcePrefixV4", 4, 0},
        {"prefixLength", 1, 0},
        FB_IESPEC_NULL
    };
    
    /* Create internal templates */
    fbTemplate_t *main_tmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(main_tmpl, main_spec, 0, &err);
    fbSessionAddTemplate(session, FALSE, TID_SAV_MAIN, main_tmpl, NULL, &err);
    
    fbTemplate_t *sub_tmpl = fbTemplateAlloc(model);
    fbTemplateAppendSpecArray(sub_tmpl, sub_spec, 0, &err);
    fbSessionAddTemplate(session, FALSE, TID_SAV_SUB, sub_tmpl, NULL, &err);
    
    /* Set internal template */
    fBufSetInternalTemplate(fbuf, TID_SAV_MAIN, &err);
    
    /* Create JSON array for records */
    json_t *records_array = json_array();
    int record_count = 0;
    
    /* Read records */
    uint8_t rec_buf[65535];
    while (fBufNext(fbuf, rec_buf, &rec_len, &err)) {
        SAVRecord *rec = (SAVRecord *)rec_buf;
        
        /* Create JSON object for this record */
        json_t *record_obj = json_object();
        json_object_set_new(record_obj, "recordId", json_integer(++record_count));
        json_object_set_new(record_obj, "timestamp", json_integer(rec->timestamp));
        json_object_set_new(record_obj, "ruleType", json_integer(rec->rule_type));
        json_object_set_new(record_obj, "targetType", json_integer(rec->target_type));
        json_object_set_new(record_obj, "policyAction", json_integer(rec->policy_action));
        
        /* Rule type name */
        const char *rule_type_name = "unknown";
        switch (rec->rule_type) {
            case 0: rule_type_name = "allowlist"; break;
            case 1: rule_type_name = "blocklist"; break;
            case 2: rule_type_name = "prefix"; break;
            case 3: rule_type_name = "aspath"; break;
        }
        json_object_set_new(record_obj, "ruleTypeName", json_string(rule_type_name));
        
        /* Parse SubTemplateList */
        json_t *rules_array = json_array();
        fbSubTemplateList_t *stl = &rec->allowlist;
        
        if (stl->numElements > 0) {
            SubRecord *sub_rec = NULL;
            while ((sub_rec = (SubRecord *)fbSubTemplateListGetNextPtr(stl, sub_rec)) != NULL) {
                json_t *rule_obj = json_object();
                
                char ip_str[INET_ADDRSTRLEN];
                ip_to_string(sub_rec->source_prefix, ip_str, sizeof(ip_str));
                
                json_object_set_new(rule_obj, "interfaceId", json_integer(sub_rec->interface_id));
                json_object_set_new(rule_obj, "sourcePrefix", json_string(ip_str));
                json_object_set_new(rule_obj, "prefixLength", json_integer(sub_rec->prefix_length));
                
                json_array_append_new(rules_array, rule_obj);
            }
        }
        
        json_object_set_new(record_obj, "rules", rules_array);
        json_array_append_new(records_array, record_obj);
        
        fbSubTemplateListClear(&rec->allowlist);
    }
    
    /* Check if EOF or error */
    if (err && !g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
        fprintf(stderr, "Error reading records: %s\n", err->message);
        g_clear_error(&err);
    }
    
    /* Create final JSON structure */
    json_t *root = json_object();
    json_object_set_new(root, "totalRecords", json_integer(record_count));
    json_object_set_new(root, "records", records_array);
    json_object_set_new(root, "generatedAt", json_integer(time(NULL)));
    
    /* Write JSON to file */
    if (json_dump_file(root, output_file, JSON_INDENT(2)) != 0) {
        fprintf(stderr, "Failed to write JSON file\n");
        json_decref(root);
        return 1;
    }
    
    json_decref(root);
    
    /* Cleanup */
    fBufFree(fbuf);
    fbInfoModelFree(model);
    
    printf("✅ Parsed %d records\n", record_count);
    printf("✅ JSON output: %s\n", output_file);
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.ipfix> <output.json>\n", argv[0]);
        fprintf(stderr, "Example: %s test_data/sample_sav.ipfix web/data.json\n", argv[0]);
        return 1;
    }
    
    return parse_and_output_json(argv[1], argv[2]);
}
