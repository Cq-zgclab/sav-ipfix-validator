/**
 * @file test_basic.c
 * @brief Basic test for SAV IE and template registration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sav_ie_definitions.h"

#define TEST_PASS "\033[32m✓ PASS\033[0m"
#define TEST_FAIL "\033[31m✗ FAIL\033[0m"

int tests_run = 0;
int tests_passed = 0;

void test_info_model_init()
{
    printf("Test 1: Info Model Initialization... ");
    tests_run++;
    
    fbInfoModel_t *model = fbInfoModelAlloc();
    assert(model != NULL);
    
    gboolean result = sav_init_info_model(model);
    assert(result == TRUE);
    
    /* Verify that SAV IEs were added by trying to get them by name */
    const fbInfoElement_t *ie = fbInfoModelGetElementByName(model, "savRuleType");
    if (ie != NULL) {
        printf("%s\n", TEST_PASS);
        tests_passed++;
    } else {
        printf("%s (IE not found)\n", TEST_FAIL);
    }
    
    fbInfoModelFree(model);
}

void test_template_registration()
{
    printf("Test 2: Template Registration... ");
    tests_run++;
    
    GError *err = NULL;
    fbInfoModel_t *model = fbInfoModelAlloc();
    sav_init_info_model(model);
    
    fbSession_t *session = fbSessionAlloc(model);
    assert(session != NULL);
    
    /* Register templates */
    gboolean result = sav_add_templates(session, &err);
    if (!result) {
        printf("%s - %s\n", TEST_FAIL, err ? err->message : "Unknown error");
        if (err) g_error_free(err);
        return;
    }
    
    /* Verify templates were added */
    fbTemplate_t *tmpl_901 = fbSessionGetTemplate(session, TRUE, SAV_TMPL_IPV4_INTERFACE_PREFIX, &err);
    fbTemplate_t *tmpl_902 = fbSessionGetTemplate(session, TRUE, SAV_TMPL_IPV6_INTERFACE_PREFIX, &err);
    fbTemplate_t *tmpl_903 = fbSessionGetTemplate(session, TRUE, SAV_TMPL_IPV4_PREFIX_INTERFACE, &err);
    fbTemplate_t *tmpl_904 = fbSessionGetTemplate(session, TRUE, SAV_TMPL_IPV6_PREFIX_INTERFACE, &err);
    fbTemplate_t *tmpl_400 = fbSessionGetTemplate(session, TRUE, SAV_MAIN_TEMPLATE_ID, &err);
    
    if (tmpl_901 && tmpl_902 && tmpl_903 && tmpl_904 && tmpl_400) {
        printf("%s\n", TEST_PASS);
        tests_passed++;
    } else {
        printf("%s\n", TEST_FAIL);
    }
    
    fbSessionFree(session);
}

void test_validation_functions()
{
    printf("Test 3: Validation Functions... ");
    tests_run++;
    
    int all_pass = 1;
    
    /* Test savRuleType validation */
    if (!sav_validate_rule_type(SAV_RULE_TYPE_ALLOWLIST)) all_pass = 0;
    if (!sav_validate_rule_type(SAV_RULE_TYPE_BLOCKLIST)) all_pass = 0;
    if (sav_validate_rule_type(255)) all_pass = 0;
    
    /* Test savTargetType validation */
    if (!sav_validate_target_type(SAV_TARGET_TYPE_INTERFACE_BASED)) all_pass = 0;
    if (!sav_validate_target_type(SAV_TARGET_TYPE_PREFIX_BASED)) all_pass = 0;
    if (sav_validate_target_type(255)) all_pass = 0;
    
    /* Test savPolicyAction validation */
    if (!sav_validate_policy_action(SAV_POLICY_ACTION_PERMIT)) all_pass = 0;
    if (!sav_validate_policy_action(SAV_POLICY_ACTION_DISCARD)) all_pass = 0;
    if (!sav_validate_policy_action(SAV_POLICY_ACTION_RATE_LIMIT)) all_pass = 0;
    if (!sav_validate_policy_action(SAV_POLICY_ACTION_REDIRECT)) all_pass = 0;
    if (sav_validate_policy_action(255)) all_pass = 0;
    
    if (all_pass) {
        printf("%s\n", TEST_PASS);
        tests_passed++;
    } else {
        printf("%s\n", TEST_FAIL);
    }
}

void test_name_functions()
{
    printf("Test 4: Name Functions... ");
    tests_run++;
    
    int all_pass = 1;
    
    /* Test rule type names */
    if (strcmp(sav_rule_type_name(SAV_RULE_TYPE_ALLOWLIST), "allowlist") != 0) all_pass = 0;
    if (strcmp(sav_rule_type_name(SAV_RULE_TYPE_BLOCKLIST), "blocklist") != 0) all_pass = 0;
    if (strcmp(sav_rule_type_name(255), "unknown") != 0) all_pass = 0;
    
    /* Test target type names */
    if (strcmp(sav_target_type_name(SAV_TARGET_TYPE_INTERFACE_BASED), "interface-based") != 0) all_pass = 0;
    if (strcmp(sav_target_type_name(SAV_TARGET_TYPE_PREFIX_BASED), "prefix-based") != 0) all_pass = 0;
    if (strcmp(sav_target_type_name(255), "unknown") != 0) all_pass = 0;
    
    /* Test policy action names */
    if (strcmp(sav_policy_action_name(SAV_POLICY_ACTION_PERMIT), "permit") != 0) all_pass = 0;
    if (strcmp(sav_policy_action_name(SAV_POLICY_ACTION_DISCARD), "discard") != 0) all_pass = 0;
    if (strcmp(sav_policy_action_name(SAV_POLICY_ACTION_RATE_LIMIT), "rate-limit") != 0) all_pass = 0;
    if (strcmp(sav_policy_action_name(SAV_POLICY_ACTION_REDIRECT), "redirect") != 0) all_pass = 0;
    if (strcmp(sav_policy_action_name(255), "unknown") != 0) all_pass = 0;
    
    if (all_pass) {
        printf("%s\n", TEST_PASS);
        tests_passed++;
    } else {
        printf("%s\n", TEST_FAIL);
    }
}

int main()
{
    printf("\n=== SAV IPFIX Validator - Basic Tests ===\n\n");
    
    test_info_model_init();
    test_template_registration();
    test_validation_functions();
    test_name_functions();
    
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    
    if (tests_passed == tests_run) {
        printf("\n\033[32m✓ All tests passed!\033[0m\n\n");
        return 0;
    } else {
        printf("\n\033[31m✗ Some tests failed!\033[0m\n\n");
        return 1;
    }
}
