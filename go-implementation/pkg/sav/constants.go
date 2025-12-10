package sav

// SAV IPFIX constants

// Enterprise ID for SAV IEs (CERT PEN)
const EnterpriseID uint32 = 6871

// Template IDs
const (
	TemplateIPv4InterfacePrefix = 901 // IPv4 Interface -> Prefix mapping
	TemplateIPv6InterfacePrefix = 902 // IPv6 Interface -> Prefix mapping
	TemplateIPv4PrefixInterface = 903 // IPv4 Prefix -> Interface mapping
	TemplateIPv6PrefixInterface = 904 // IPv6 Prefix -> Interface mapping
	TemplateMainDataRecord      = 400 // Main data record template
)

// SAV Rule Types (savRuleType IE, enterprise 6871, element 1)
// Based on draft-cao-opsawg-ipfix-sav-01 Section 4.1
const (
	RuleTypeAllowlist uint8 = 0 // Allowlist-based SAV (permit specified sources)
	RuleTypeBlocklist uint8 = 1 // Blocklist-based SAV (deny specified sources)
)

// SAV Target Types (savTargetType IE, enterprise 6871, element 2)
// Based on draft-cao-opsawg-ipfix-sav-01 Section 4.2
const (
	TargetTypeInterfaceBased uint8 = 0 // Interface-based SAV (bind to interfaces)
	TargetTypePrefixBased    uint8 = 1 // Prefix-based SAV (bind to prefix ranges)
)

// SAV Policy Actions (savPolicyAction IE, enterprise 6871, element 4)
// Based on draft-cao-opsawg-ipfix-sav-01 Section 4.4
const (
	PolicyActionPermit    uint8 = 0 // Permit the traffic
	PolicyActionDiscard   uint8 = 1 // Discard the traffic (drop)
	PolicyActionRateLimit uint8 = 2 // Rate-limit the traffic
	PolicyActionRedirect  uint8 = 3 // Redirect to monitoring/scrubbing
)

// Helper function names for constants
func RuleTypeName(ruleType uint8) string {
	switch ruleType {
	case RuleTypeAllowlist:
		return "Allowlist"
	case RuleTypeBlocklist:
		return "Blocklist"
	default:
		return "Unknown"
	}
}

func TargetTypeName(targetType uint8) string {
	switch targetType {
	case TargetTypeInterfaceBased:
		return "InterfaceBased"
	case TargetTypePrefixBased:
		return "PrefixBased"
	default:
		return "Unknown"
	}
}

func PolicyActionName(action uint8) string {
	switch action {
	case PolicyActionPermit:
		return "Permit"
	case PolicyActionDiscard:
		return "Discard"
	case PolicyActionRateLimit:
		return "RateLimit"
	case PolicyActionRedirect:
		return "Redirect"
	default:
		return "Unknown"
	}
}
