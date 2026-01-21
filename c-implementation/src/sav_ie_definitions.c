/**
 * @file sav_ie_definitions.c
 * @brief Implementation of SAV Information Elements
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sav_ie_definitions.h"

/* SAV Information Elements - using FB_IE_INIT_FULL macro to specify data types */
static fbInfoElement_t sav_info_elements[] = {
    FB_IE_INIT_FULL("savRuleType", SAV_ENTERPRISE_ID, SAV_IE_RULE_TYPE, 1,
                    FB_IE_F_ENDIAN | FB_IE_QUANTITY,
                    0, 0, FB_UINT_8, "SAV rule type (allowlist=1, blocklist=2)"),
    FB_IE_INIT_FULL("savTargetType", SAV_ENTERPRISE_ID, SAV_IE_TARGET_TYPE, 1,
                    FB_IE_F_ENDIAN | FB_IE_QUANTITY,
                    0, 0, FB_UINT_8, "SAV target type (interface-to-prefix=1, prefix-to-interface=2)"),
    FB_IE_INIT_FULL("savMatchedContentList", SAV_ENTERPRISE_ID, SAV_IE_MATCHED_CONTENT_LIST,
                    FB_IE_VARLEN, 0,
                    0, 0, FB_SUB_TMPL_LIST, "SAV matched content sub-template list"),
    FB_IE_INIT_FULL("savPolicyAction", SAV_ENTERPRISE_ID, SAV_IE_POLICY_ACTION, 1,
                    FB_IE_F_ENDIAN | FB_IE_QUANTITY,
                    0, 0, FB_UINT_8, "SAV policy action (drop=1, rate-limit=2, redirect=3)"),
    FB_IE_NULL
};

/* Template spec for IPv4 Interface-to-Prefix mapping (Template 900) */
static fbInfoElementSpec_t sav_ipv4_interface_prefix_spec[] = {
    { "ingressInterface",        4, 0 },
    { "sourceIPv4Prefix",        4, 0 },
    { "sourceIPv4PrefixLength",  1, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv6 Interface-to-Prefix mapping (Template 901) */
static fbInfoElementSpec_t sav_ipv6_interface_prefix_spec[] = {
    { "ingressInterface",        4, 0 },
    { "sourceIPv6Prefix",       16, 0 },
    { "sourceIPv6PrefixLength",  1, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv4 Prefix-to-Interface mapping (Template 902) */
static fbInfoElementSpec_t sav_ipv4_prefix_interface_spec[] = {
    { "sourceIPv4Prefix",        4, 0 },
    { "sourceIPv4PrefixLength",  1, 0 },
    { "ingressInterface",        4, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv6 Prefix-to-Interface mapping (Template 903) */
static fbInfoElementSpec_t sav_ipv6_prefix_interface_spec[] = {
    { "sourceIPv6Prefix",       16, 0 },
    { "sourceIPv6PrefixLength",  1, 0 },
    { "ingressInterface",        4, 0 },
    FB_IESPEC_NULL
};

/* T1 template spec (Template 400) */
static fbInfoElementSpec_t sav_t1_template_spec[] = {
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "ingressInterface",            4, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               9, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

/* ---- New observation model templates (new architecture) ---- */

/* T2 IPv4 template spec (Template 410) */
static fbInfoElementSpec_t sav_t2_v4_template_spec[] = {
    { "observationTimeMilliseconds", 8, 0 },
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "ingressInterface",            4, 0 },
    { "sourceIPv4Address",           4, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               5, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

/* T2 IPv6 template spec (Template 420) */
static fbInfoElementSpec_t sav_t2_v6_template_spec[] = {
    { "observationTimeMilliseconds", 8, 0 },
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "ingressInterface",            4, 0 },
    { "sourceIPv6Address",          16, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               1, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

static fbInfoElementSpec_t sav_t3_v4_template_spec[] = {
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "sourceIPv4Prefix",            4, 0 },
    { "sourceIPv4PrefixLength",      1, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

static fbInfoElementSpec_t sav_t3_v6_template_spec[] = {
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "sourceIPv6Prefix",           16, 0 },
    { "sourceIPv6PrefixLength",      1, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               4, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

/* ---- Story templates ----
 * Template A (Ops monitoring, default enabled)
 * - "哪个接口被打得最多、哪类 SAV 规则在生效"
 * - IPv4/IPv6 split (500/501)
 */
static fbInfoElementSpec_t sav_template_a_v4_spec[] = {
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "ingressInterface",            4, 0 },
    { "sourceIPv4Prefix",            4, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               5, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

static fbInfoElementSpec_t sav_template_a_v6_spec[] = {
    { "flowStartMilliseconds",       8, 0 },
    { "flowEndMilliseconds",         8, 0 },
    { "packetDeltaCount",            8, 0 },
    { "octetDeltaCount",             8, 0 },
    { "ingressInterface",            4, 0 },
    { "sourceIPv6Prefix",           16, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               1, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

/* Template B (Incident investigation, disabled by default)
 * - "调查具体攻击流，并理解其被 SAV 判定的原因"
 * - NOTE: Not suitable for long-term enablement.
 * - IPv4/IPv6 split (502/503)
 */
static fbInfoElementSpec_t sav_template_b_v4_spec[] = {
    { "sourceIPv4Address",            4, 0 },
    { "destinationIPv4Address",       4, 0 },
    { "sourceTransportPort",          2, 0 },
    { "destinationTransportPort",     2, 0 },
    { "ingressInterface",             4, 0 },
    { "packetDeltaCount",             8, 0 },
    { "flowStartMilliseconds",        8, 0 },
    { "protocolIdentifier",           1, 0 },
    { "savRuleType",                  1, 0 },
    { "savTargetType",                1, 0 },
    { "savPolicyAction",              1, 0 },
    { "paddingOctets",                4, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

static fbInfoElementSpec_t sav_template_b_v6_spec[] = {
    { "sourceIPv6Address",           16, 0 },
    { "destinationIPv6Address",      16, 0 },
    { "sourceTransportPort",          2, 0 },
    { "destinationTransportPort",     2, 0 },
    { "ingressInterface",             4, 0 },
    { "packetDeltaCount",             8, 0 },
    { "flowStartMilliseconds",        8, 0 },
    { "protocolIdentifier",           1, 0 },
    { "savRuleType",                  1, 0 },
    { "savTargetType",                1, 0 },
    { "savPolicyAction",              1, 0 },
    { "paddingOctets",                4, 0 },
    { "subTemplateList", FB_IE_VARLEN, 0 },
    FB_IESPEC_NULL
};

static gboolean env_enable_template_b(void)
{
    const char *v = getenv("SAV_ENABLE_TEMPLATE_B");
    if (v && v[0] != '\0' && strcmp(v, "0") != 0) {
        return TRUE;
    }

    /* Demo mode forces Template B on so we can emit 502/503 records. */
    const char *demo = getenv("SAV_DEMO_TEMPLATE_A_CROSSPRODUCT");
    return (demo && demo[0] != '\0' && strcmp(demo, "0") != 0);
}

static gboolean env_enable_legacy_t123(void)
{
    const char *v = getenv("SAV_EXPORT_T123");
    return (v && v[0] != '\0' && strcmp(v, "0") != 0);
}

gboolean sav_init_info_model(fbInfoModel_t *model)
{
    if (!model) {
        return FALSE;
    }
    
    /* Add SAV-specific information elements to the model */
    fbInfoModelAddElementArray(model, sav_info_elements);
    
    return TRUE;
}

gboolean sav_add_templates(fbSession_t *session, GError **err)
{
    fbTemplate_t *tmpl = NULL;
    fbInfoModel_t *model = fbSessionGetInfoModel(session);

    if (!model) {
        g_set_error(err, FB_ERROR_DOMAIN, FB_ERROR_SETUP, "Could not get info model from session");
        return FALSE;
    }

    /* Template 900: IPv4 Interface-to-Prefix */
    /* Use fbSessionAddTemplatesForExport which handles dual registration automatically */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv4_interface_prefix_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV4_INTERFACE_PREFIX, tmpl, NULL, err)) return FALSE;

    /* Template 901: IPv6 Interface-to-Prefix */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv6_interface_prefix_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV6_INTERFACE_PREFIX, tmpl, NULL, err)) return FALSE;

    /* Template 902: IPv4 Prefix-to-Interface */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv4_prefix_interface_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV4_PREFIX_INTERFACE, tmpl, NULL, err)) return FALSE;

    /* Template 903: IPv6 Prefix-to-Interface */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv6_prefix_interface_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV6_PREFIX_INTERFACE, tmpl, NULL, err)) return FALSE;

    /* Legacy observation models (T1/T2/T3) are disabled by default.
     * Enable only for backward-compat research runs:
     *   SAV_EXPORT_T123=1
     */
    if (env_enable_legacy_t123()) {
        /* T1 template (400) */
        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_t1_template_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_T1_TEMPLATE, tmpl, NULL, err)) return FALSE;

        /* T2 templates (410/420) */
        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_t2_v4_template_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_T2_TEMPLATE_IPV4, tmpl, NULL, err)) return FALSE;

        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_t2_v6_template_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_T2_TEMPLATE_IPV6, tmpl, NULL, err)) return FALSE;

        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_t3_v4_template_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_T3_TEMPLATE_IPV4, tmpl, NULL, err)) return FALSE;

        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_t3_v6_template_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_T3_TEMPLATE_IPV6, tmpl, NULL, err)) return FALSE;
    }

    /* ---- Story templates ---- */
    /* Template A: Ops monitoring (default enabled) */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_template_a_v4_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TEMPLATE_A_IPV4, tmpl, NULL, err)) return FALSE;

    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_template_a_v6_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TEMPLATE_A_IPV6, tmpl, NULL, err)) return FALSE;

    /* Template B: Incident investigation (disabled by default)
     * Enable by setting: SAV_ENABLE_TEMPLATE_B=1
     */
    if (env_enable_template_b()) {
        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_template_b_v4_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_TEMPLATE_B_IPV4, tmpl, NULL, err)) return FALSE;

        tmpl = fbTemplateAlloc(model);
        if (!fbTemplateAppendSpecArray(tmpl, sav_template_b_v6_spec, 0, err)) return FALSE;
        if (!fbSessionAddTemplatesForExport(session, SAV_TEMPLATE_B_IPV6, tmpl, NULL, err)) return FALSE;
    }

    /* Template pairs are now automatically established by dual registration above */
    /* But we keep these calls for compatibility with older libfixbuf versions */
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV4_INTERFACE_PREFIX, SAV_TMPL_IPV4_INTERFACE_PREFIX);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV6_INTERFACE_PREFIX, SAV_TMPL_IPV6_INTERFACE_PREFIX);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV4_PREFIX_INTERFACE, SAV_TMPL_IPV4_PREFIX_INTERFACE);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV6_PREFIX_INTERFACE, SAV_TMPL_IPV6_PREFIX_INTERFACE);

    if (env_enable_legacy_t123()) {
        fbSessionAddTemplatePair(session, SAV_T1_TEMPLATE,      SAV_T1_TEMPLATE);
        fbSessionAddTemplatePair(session, SAV_T2_TEMPLATE_IPV4, SAV_T2_TEMPLATE_IPV4);
        fbSessionAddTemplatePair(session, SAV_T2_TEMPLATE_IPV6, SAV_T2_TEMPLATE_IPV6);
        fbSessionAddTemplatePair(session, SAV_T3_TEMPLATE_IPV4, SAV_T3_TEMPLATE_IPV4);
        fbSessionAddTemplatePair(session, SAV_T3_TEMPLATE_IPV6, SAV_T3_TEMPLATE_IPV6);
    }

    fbSessionAddTemplatePair(session, SAV_TEMPLATE_A_IPV4, SAV_TEMPLATE_A_IPV4);
    fbSessionAddTemplatePair(session, SAV_TEMPLATE_A_IPV6, SAV_TEMPLATE_A_IPV6);
    if (env_enable_template_b()) {
        fbSessionAddTemplatePair(session, SAV_TEMPLATE_B_IPV4, SAV_TEMPLATE_B_IPV4);
        fbSessionAddTemplatePair(session, SAV_TEMPLATE_B_IPV6, SAV_TEMPLATE_B_IPV6);
    }

    return TRUE;
}

const char* sav_rule_type_name(uint8_t type)
{
    switch (type) {
        case SAV_RULE_TYPE_ALLOWLIST:
            return "allowlist";
        case SAV_RULE_TYPE_BLOCKLIST:
            return "blocklist";
        default:
            return "unknown";
    }
}

const char* sav_target_type_name(uint8_t type)
{
    switch (type) {
        case SAV_TARGET_TYPE_INTERFACE_BASED:
            return "interface-based";
        case SAV_TARGET_TYPE_PREFIX_BASED:
            return "prefix-based";
        default:
            return "unknown";
    }
}

const char* sav_policy_action_name(uint8_t action)
{
    switch (action) {
        case SAV_POLICY_ACTION_PERMIT:
            return "permit";
        case SAV_POLICY_ACTION_DISCARD:
            return "discard";
        case SAV_POLICY_ACTION_RATE_LIMIT:
            return "rate-limit";
        case SAV_POLICY_ACTION_REDIRECT:
            return "redirect";
        default:
            return "unknown";
    }
}

gboolean sav_validate_rule_type(uint8_t type)
{
    return (type <= SAV_RULE_TYPE_MAX);
}

gboolean sav_validate_target_type(uint8_t type)
{
    return (type <= SAV_TARGET_TYPE_MAX);
}

gboolean sav_validate_policy_action(uint8_t action)
{
    return (action <= SAV_POLICY_ACTION_MAX);
}

uint16_t sav_get_template_id(uint8_t rule_type, uint8_t target_type)
{
    (void)rule_type;

    /* Determine template ID based on target type */
    /* For now, we ignore rule_type and focus on target_type + IP version */
    /* IPv4/IPv6 will be determined at runtime when adding entries */
    
    if (target_type == SAV_TARGET_TYPE_INTERFACE_BASED) {
        /* Default to IPv4 Interface->Prefix, caller can override */
        return SAV_TMPL_IPV4_INTERFACE_PREFIX;
    } else if (target_type == SAV_TARGET_TYPE_PREFIX_BASED) {
        /* Default to IPv4 Prefix->Interface, caller can override */
        return SAV_TMPL_IPV4_PREFIX_INTERFACE;
    }
    
    /* Fallback */
    return SAV_TMPL_IPV4_INTERFACE_PREFIX;
}
