/**
 * @file sav_ie_definitions.h
 * @brief SAV (Source Address Validation) IPFIX Information Element Definitions
 *
 * This file defines the SAV-specific Information Elements as specified in
 * draft-cao-opsawg-ipfix-sav-01.
 *
 * Enterprise ID: 6871 (reserved for private/example use)
 */

#ifndef SAV_IE_DEFINITIONS_H
#define SAV_IE_DEFINITIONS_H

#include <fixbuf/public.h>

/* SAV Enterprise ID - using private enterprise number for testing */
#define SAV_ENTERPRISE_ID 6871

/* SAV Information Element IDs (within private enterprise space) */
#define SAV_IE_RULE_TYPE            1  /* TBD1 in draft */
#define SAV_IE_TARGET_TYPE          2  /* TBD2 in draft */
#define SAV_IE_MATCHED_CONTENT_LIST 3  /* TBD3 in draft */
#define SAV_IE_POLICY_ACTION        4  /* TBD4 in draft */

/* Sub-template IDs for savMatchedContentList */
#define SAV_TMPL_IPV4_INTERFACE_PREFIX  901  /* IPv4 Interface-to-Prefix */
#define SAV_TMPL_IPV6_INTERFACE_PREFIX  902  /* IPv6 Interface-to-Prefix */
#define SAV_TMPL_IPV4_PREFIX_INTERFACE  903  /* IPv4 Prefix-to-Interface */
#define SAV_TMPL_IPV6_PREFIX_INTERFACE  904  /* IPv6 Prefix-to-Interface */

/* Main template ID for SAV Data Records */
#define SAV_MAIN_TEMPLATE_ID 400

/* savRuleType values */
typedef enum {
    SAV_RULE_TYPE_ALLOWLIST = 0,
    SAV_RULE_TYPE_BLOCKLIST = 1,
    SAV_RULE_TYPE_MAX = 1
} sav_rule_type_t;

/* savTargetType values */
typedef enum {
    SAV_TARGET_TYPE_INTERFACE_BASED = 0,
    SAV_TARGET_TYPE_PREFIX_BASED = 1,
    SAV_TARGET_TYPE_MAX = 1
} sav_target_type_t;

/* savPolicyAction values */
typedef enum {
    SAV_POLICY_ACTION_PERMIT = 0,
    SAV_POLICY_ACTION_DISCARD = 1,
    SAV_POLICY_ACTION_RATE_LIMIT = 2,
    SAV_POLICY_ACTION_REDIRECT = 3,
    SAV_POLICY_ACTION_MAX = 3
} sav_policy_action_t;

/**
 * Structure for IPv4 Interface-to-Prefix mapping (Template 901/903)
 */
typedef struct sav_ipv4_mapping_st {
    uint32_t ingressInterface;
    uint32_t sourceIPv4Prefix;
    uint8_t  sourceIPv4PrefixLength;
} sav_ipv4_mapping_t;

/**
 * Structure for IPv6 Interface-to-Prefix mapping (Template 902/904)
 */
typedef struct sav_ipv6_mapping_st {
    uint32_t ingressInterface;
    uint8_t  sourceIPv6Prefix[16];
    uint8_t  sourceIPv6PrefixLength;
} sav_ipv6_mapping_t;

/**
 * Main SAV Data Record structure (Template 400)
 * Matches the order in sav_main_template_spec
 * 
 * CRITICAL: SubTemplateList must be LAST in the struct!
 * Padding is required to align fbSubTemplateList_t (32-byte aligned).
 */
typedef struct sav_data_record_st {
    uint64_t            observationTimeMilliseconds;  /* IE 323 */
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[5];                  /* Explicit padding for alignment */
    fbSubTemplateList_t savMatchedContentList;        /* Must be last! */
} sav_data_record_t;

/**
 * Initialize SAV Information Elements in the info model
 * @param model Pointer to fbInfoModel_t
 * @return TRUE on success, FALSE on failure
 */
gboolean sav_init_info_model(
    fbInfoModel_t *model);

/**
 * @brief Adds all SAV-related templates to a session.
 *
 * This function registers the main SAV data record template (400) and all
 * sub-templates (901-904) for the savMatchedContentList. It uses the
 * libfixbuf 3.x API to register both internal and external templates at once.
 *
 * @param session The session to add templates to.
 * @param err A pointer to a GError, or NULL.
 * @return TRUE on success, FALSE on failure.
 */
gboolean sav_add_templates(
    fbSession_t *session,
    GError **err);

/**
 * Get human-readable name for savRuleType value
 */
const char* sav_rule_type_name(uint8_t type);

/**
 * Get human-readable name for savTargetType value
 */
const char* sav_target_type_name(uint8_t type);

/**
 * Get human-readable name for savPolicyAction value
 */
const char* sav_policy_action_name(uint8_t action);

/**
 * Validate savRuleType value
 */
gboolean sav_validate_rule_type(uint8_t type);

/**
 * Validate savTargetType value
 */
gboolean sav_validate_target_type(uint8_t type);

/**
 * Validate savPolicyAction value
 */
gboolean sav_validate_policy_action(uint8_t action);

/**
 * Get appropriate template ID based on rule type and target type
 * @param rule_type SAV rule type (allowlist=1, blocklist=2)
 * @param target_type SAV target type (interface-prefix=1, prefix-interface=2)
 * @return Template ID (901-904)
 */
uint16_t sav_get_template_id(uint8_t rule_type, uint8_t target_type);

#endif /* SAV_IE_DEFINITIONS_H */
