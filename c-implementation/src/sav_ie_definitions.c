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

/* Template spec for IPv4 Interface-to-Prefix mapping (Template 901) */
static fbInfoElementSpec_t sav_ipv4_interface_prefix_spec[] = {
    { "ingressInterface",        4, 0 },
    { "sourceIPv4Prefix",        4, 0 },
    { "sourceIPv4PrefixLength",  1, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv6 Interface-to-Prefix mapping (Template 902) */
static fbInfoElementSpec_t sav_ipv6_interface_prefix_spec[] = {
    { "ingressInterface",        4, 0 },
    { "sourceIPv6Prefix",       16, 0 },
    { "sourceIPv6PrefixLength",  1, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv4 Prefix-to-Interface mapping (Template 903) */
static fbInfoElementSpec_t sav_ipv4_prefix_interface_spec[] = {
    { "sourceIPv4Prefix",        4, 0 },
    { "sourceIPv4PrefixLength",  1, 0 },
    { "ingressInterface",        4, 0 },
    FB_IESPEC_NULL
};

/* Template spec for IPv6 Prefix-to-Interface mapping (Template 904) */
static fbInfoElementSpec_t sav_ipv6_prefix_interface_spec[] = {
    { "sourceIPv6Prefix",       16, 0 },
    { "sourceIPv6PrefixLength",  1, 0 },
    { "ingressInterface",        4, 0 },
    FB_IESPEC_NULL
};

/* Main template spec for SAV Data Records (Template 400) */
static fbInfoElementSpec_t sav_main_template_spec[] = {
    { "observationTimeMilliseconds", 8, 0 },
    { "savRuleType",                 1, 0 },
    { "savTargetType",               1, 0 },
    { "savPolicyAction",             1, 0 },
    { "paddingOctets",               5, 0 },  /* Explicit padding for struct alignment */
    { "subTemplateList", FB_IE_VARLEN, 0 },   /* Must be last! */
    FB_IESPEC_NULL
};

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

    /* Template 901: IPv4 Interface-to-Prefix */
    /* Use fbSessionAddTemplatesForExport which handles dual registration automatically */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv4_interface_prefix_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV4_INTERFACE_PREFIX, tmpl, NULL, err)) return FALSE;

    /* Template 902: IPv6 Interface-to-Prefix */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv6_interface_prefix_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV6_INTERFACE_PREFIX, tmpl, NULL, err)) return FALSE;

    /* Template 903: IPv4 Prefix-to-Interface */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv4_prefix_interface_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV4_PREFIX_INTERFACE, tmpl, NULL, err)) return FALSE;

    /* Template 904: IPv6 Prefix-to-Interface */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_ipv6_prefix_interface_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_TMPL_IPV6_PREFIX_INTERFACE, tmpl, NULL, err)) return FALSE;

    /* Main template for SAV Data Records */
    tmpl = fbTemplateAlloc(model);
    if (!fbTemplateAppendSpecArray(tmpl, sav_main_template_spec, 0, err)) return FALSE;
    if (!fbSessionAddTemplatesForExport(session, SAV_MAIN_TEMPLATE_ID, tmpl, NULL, err)) return FALSE;

    /* Template pairs are now automatically established by dual registration above */
    /* But we keep these calls for compatibility with older libfixbuf versions */
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV4_INTERFACE_PREFIX, SAV_TMPL_IPV4_INTERFACE_PREFIX);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV6_INTERFACE_PREFIX, SAV_TMPL_IPV6_INTERFACE_PREFIX);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV4_PREFIX_INTERFACE, SAV_TMPL_IPV4_PREFIX_INTERFACE);
    fbSessionAddTemplatePair(session, SAV_TMPL_IPV6_PREFIX_INTERFACE, SAV_TMPL_IPV6_PREFIX_INTERFACE);
    fbSessionAddTemplatePair(session, SAV_MAIN_TEMPLATE_ID, SAV_MAIN_TEMPLATE_ID);

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
