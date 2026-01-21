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

/* SubTemplateList IDs (ONLY for savMatchedContentList)
 * Task 1 rule:
 * - 900: interface-based IPv4 (Interface -> Prefix)
 * - 901: interface-based IPv6 (Interface -> Prefix)
 * - 902: prefix-based IPv4    (Prefix -> Interface)
 * - 903: prefix-based IPv6    (Prefix -> Interface)
 */
#define SAV_TMPL_IPV4_INTERFACE_PREFIX  900
#define SAV_TMPL_IPV6_INTERFACE_PREFIX  901
#define SAV_TMPL_IPV4_PREFIX_INTERFACE  902
#define SAV_TMPL_IPV6_PREFIX_INTERFACE  903

/* Flow Record Template IDs (ONLY for records)
 * Task 1 rule:
 * - 400: T1
 * - 410: T2 / IPv4
 * - 420: T2 / IPv6
 * - 430: T3 / IPv4
 * - 440: T3 / IPv6
 */
#define SAV_T1_TEMPLATE      400
#define SAV_T2_TEMPLATE_IPV4 410
#define SAV_T2_TEMPLATE_IPV6 420
#define SAV_T3_TEMPLATE_IPV4 430
#define SAV_T3_TEMPLATE_IPV6 440

/* ---- Story-driven exporter templates ----
 * Template A (Ops /长期运维监控): default enabled
 * - 500: IPv4
 * - 501: IPv6
 *
 * Template B (Security /事件调查): enable on demand via env var
 * - 502: IPv4
 * - 503: IPv6
 */
#define SAV_TEMPLATE_A_IPV4 500
#define SAV_TEMPLATE_A_IPV6 501
#define SAV_TEMPLATE_B_IPV4 502
#define SAV_TEMPLATE_B_IPV6 503

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
 * Structure for IPv4 Interface-to-Prefix mapping (Template 900)
 */
typedef struct sav_ipv4_mapping_st {
    uint32_t ingressInterface;
    uint32_t sourceIPv4Prefix;
    uint8_t  sourceIPv4PrefixLength;
} sav_ipv4_mapping_t;

/**
 * Structure for IPv6 Interface-to-Prefix mapping (Template 901)
 */
typedef struct sav_ipv6_mapping_st {
    uint32_t ingressInterface;
    uint8_t  sourceIPv6Prefix[16];
    uint8_t  sourceIPv6PrefixLength;
} sav_ipv6_mapping_t;

/**
 * T1 record structure (Template 400)
 * 
 * CRITICAL: SubTemplateList must be LAST in the struct!
 */
typedef struct sav_t1_record_st {
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            ingressInterface;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[9];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_t1_record_t;

/* ---- New observation model record layouts ---- */

/* T2 (fine-grained) - IPv4 (Template 410) */
typedef struct sav_t2_record_v4_st {
    uint64_t            observationTimeMilliseconds;
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            ingressInterface;
    uint32_t            sourceIPv4Address;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[5];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_t2_record_v4_t;

/* T2 (fine-grained) - IPv6 (Template 420) */
typedef struct sav_t2_record_v6_st {
    uint64_t            observationTimeMilliseconds;
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            ingressInterface;
    uint8_t             sourceIPv6Address[16];
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[1];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_t2_record_v6_t;

/* T3 (Prefix / Mode view) - IPv4 */
typedef struct sav_t3_record_v4_st {
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            sourceIPv4Prefix;
    uint8_t             sourceIPv4PrefixLength;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_t3_record_v4_t;

/* T3 (Prefix / Mode view) - IPv6 */
typedef struct sav_t3_record_v6_st {
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint8_t             sourceIPv6Prefix[16];
    uint8_t             sourceIPv6PrefixLength;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[4];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_t3_record_v6_t;

/* ---- Template A: Ops monitoring (default enabled) ----
 * Purpose: "哪个接口被打得最多、哪类 SAV 规则在生效"
 * Flow key is formed by exporter aggregation logic (not represented in this struct).
 * CRITICAL: SubTemplateList must be LAST in the struct!
 */
typedef struct sav_template_a_v4_record_st {
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            ingressInterface;
    uint32_t            sourceIPv4Prefix;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[5];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_template_a_v4_record_t;

typedef struct sav_template_a_v6_record_st {
    uint64_t            flowStartMilliseconds;
    uint64_t            flowEndMilliseconds;
    uint64_t            packetDeltaCount;
    uint64_t            octetDeltaCount;
    uint32_t            ingressInterface;
    uint8_t             sourceIPv6Prefix[16];
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[1];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_template_a_v6_record_t;

/* ---- Template B: Incident investigation (disabled by default) ----
 * Purpose: "调查具体攻击流，并理解其被 SAV 判定的原因"
 * NOTE: Template B is NOT suitable for long-term enablement.
 * CRITICAL: SubTemplateList must be LAST in the struct!
 */
typedef struct sav_template_b_v4_record_st {
    uint32_t            sourceIPv4Address;
    uint32_t            destinationIPv4Address;
    uint16_t            sourceTransportPort;
    uint16_t            destinationTransportPort;
    uint32_t            ingressInterface;
    uint64_t            packetDeltaCount;
    uint64_t            flowStartMilliseconds;
    uint8_t             protocolIdentifier;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[4];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_template_b_v4_record_t;

typedef struct sav_template_b_v6_record_st {
    uint8_t             sourceIPv6Address[16];
    uint8_t             destinationIPv6Address[16];
    uint16_t            sourceTransportPort;
    uint16_t            destinationTransportPort;
    uint32_t            ingressInterface;
    uint64_t            packetDeltaCount;
    uint64_t            flowStartMilliseconds;
    uint8_t             protocolIdentifier;
    uint8_t             savRuleType;
    uint8_t             savTargetType;
    uint8_t             savPolicyAction;
    uint8_t             _padding[4];
    fbSubTemplateList_t savMatchedContentList; /* must be last */
} sav_template_b_v6_record_t;

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
 * This function registers all record templates (400/410/420/430/440) and all
 * sub-templates (900-903) for the savMatchedContentList. It uses the
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
 * @return Template ID (900-903)
 */
uint16_t sav_get_template_id(uint8_t rule_type, uint8_t target_type);

#endif /* SAV_IE_DEFINITIONS_H */
