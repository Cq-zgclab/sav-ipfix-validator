package sav

import (
	"net"
	"time"
)

// ScenarioType indicates the granularity of the record
type ScenarioType string

const (
	MacroRecord  ScenarioType = "macro"  // Aggregated statistics
	MicroRecord  ScenarioType = "micro"  // Detailed event with full rules
	CombinedView ScenarioType = "combined" // Both macro and micro
)

// Scenario represents a complete SAV business use case with context
type Scenario struct {
	Name        string
	Description string
	Analysis    string // Analysis insight for this scenario
	Records     []SAVRecord
}

// SAVRecord represents a single IPFIX record with full context
type SAVRecord struct {
	Type        ScenarioType // macro, micro, or combined
	Timestamp   time.Time
	RuleType    uint8
	TargetType  uint8
	Action      uint8
	Rules       []RuleContent
	PacketCount uint64
	ByteCount   uint64
	Label       string // Human-readable label for this record
}

// RuleContent represents a single SAV rule (interface-prefix mapping)
type RuleContent struct {
	Interface uint32
	Prefix    net.IP
	PrefixLen uint8
	IsIPv6    bool
}

// GetScenario1 - Spoofing Attack: From Detection to Forensics
// Demonstrates: Macro trend → Micro detail → Combined analysis
func GetScenario1() Scenario {
	now := time.Now()
	records := []SAVRecord{}
	
	// MACRO: Time-series aggregation (past 1 hour, every 5 minutes)
	// Shows attack timeline: start, peak, duration
	basePackets := uint64(1000)
	for i := 0; i < 12; i++ {
		timestamp := now.Add(-time.Duration(60-i*5) * time.Minute)
		
		// Simulate attack pattern: gradual increase, peak at i=7, then decrease
		var multiplier float64
		if i < 5 {
			multiplier = float64(i) * 0.5 // Ramp up
		} else if i < 9 {
			multiplier = 10.0 // Peak attack
		} else {
			multiplier = float64(12-i) * 0.8 // Cool down
		}
		
		packets := uint64(float64(basePackets) * multiplier)
		bytes := packets * 600 // Average 600 bytes per packet
		
		records = append(records, SAVRecord{
			Type:        MacroRecord,
			Timestamp:   timestamp,
			RuleType:    RuleTypeAllowlist,
			TargetType:  TargetTypeInterfaceBased,
			Action:      PolicyActionDiscard,
			Rules:       nil, // Macro records don't include full rules
			PacketCount: packets,
			ByteCount:   bytes,
			Label:       "Aggregated-5min",
		})
	}
	
	// MICRO: Detailed event at peak moment (i=7)
	// Shows exact rule that triggered, with full savMatchedContentList
	peakTime := now.Add(-time.Duration(60-7*5) * time.Minute)
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  peakTime,
		RuleType:   RuleTypeAllowlist,
		TargetType: TargetTypeInterfaceBased,
		Action:     PolicyActionDiscard,
		Rules: []RuleContent{
			// Interface eth0 allowlist - packet from 192.0.2.123 didn't match any
			{Interface: 1000, Prefix: net.ParseIP("203.0.113.0"), PrefixLen: 24, IsIPv6: false},
			{Interface: 1000, Prefix: net.ParseIP("198.51.100.0"), PrefixLen: 24, IsIPv6: false},
			{Interface: 1000, Prefix: net.ParseIP("192.0.2.0"), PrefixLen: 24, IsIPv6: false},
		},
		PacketCount: 1, // Single event
		ByteCount:   64,
		Label:       "Spoofed-192.0.2.123",
	})
	
	return Scenario{
		Name:        "Spoofing-Attack-Detection",
		Description: "Spoofing attack analysis: time-series trend + detailed forensics",
		Analysis: `MACRO: Attack started 60min ago, peaked at 35min ago (10K pps), lasted 20min.
MICRO: At peak, source 192.0.2.123 arrived on eth0 but not in allowlist.
COMBINED: Macro identifies anomaly window; micro provides rule-level forensics for incident response.`,
		Records: records,
	}
}

// GetScenario2 - Multi-Interface Traffic Distribution Analysis
// Demonstrates: Spatial distribution (per-interface) → Interface detail → Rule efficiency
func GetScenario2() Scenario {
	now := time.Now()
	records := []SAVRecord{}
	
	// MACRO: Per-interface spoofing traffic distribution
	// Shows which interfaces have the most problems
	interfaces := []struct {
		id      uint32
		name    string
		packets uint64
	}{
		{1000, "eth0-external", 45000},
		{2001, "eth1-customer-A", 8500},
		{2002, "eth2-customer-B", 12000},
		{3001, "eth3-peering", 3200},
		{4000, "eth4-internal", 1800},
	}
	
	for _, iface := range interfaces {
		records = append(records, SAVRecord{
			Type:        MacroRecord,
			Timestamp:   now,
			RuleType:    RuleTypeAllowlist,
			TargetType:  TargetTypeInterfaceBased,
			Action:      PolicyActionDiscard,
			Rules:       nil,
			PacketCount: iface.packets,
			ByteCount:   iface.packets * 600,
			Label:       iface.name,
		})
	}
	
	// MICRO: Detailed rules for worst interface (eth0)
	// Shows complete allowlist that was checked
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  now,
		RuleType:   RuleTypeAllowlist,
		TargetType: TargetTypeInterfaceBased,
		Action:     PolicyActionDiscard,
		Rules: []RuleContent{
			// eth0 allowlist - spoofed packet didn't match any of these
			{Interface: 1000, Prefix: net.ParseIP("198.51.100.0"), PrefixLen: 24, IsIPv6: false},
			{Interface: 1000, Prefix: net.ParseIP("203.0.113.0"), PrefixLen: 24, IsIPv6: false},
			{Interface: 1000, Prefix: net.ParseIP("192.0.2.0"), PrefixLen: 24, IsIPv6: false},
		},
		PacketCount: 1,
		ByteCount:   64,
		Label:       "eth0-detail",
	})
	
	// MICRO: Detailed rules for customer interface (eth1) with different action
	// Shows prefix-based validation with rate-limit
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  now,
		RuleType:   RuleTypeAllowlist,
		TargetType: TargetTypePrefixBased,
		Action:     PolicyActionRateLimit,
		Rules: []RuleContent{
			// Prefix 10.1.0.0/16 should come from eth1 or eth2
			{Interface: 2001, Prefix: net.ParseIP("10.1.0.0"), PrefixLen: 16, IsIPv6: false},
			{Interface: 2002, Prefix: net.ParseIP("10.1.0.0"), PrefixLen: 16, IsIPv6: false},
		},
		PacketCount: 1,
		ByteCount:   64,
		Label:       "prefix-10.1.0.0-detail",
	})
	
	return Scenario{
		Name:        "Multi-Interface-Distribution",
		Description: "Interface-level SAV traffic distribution and rule analysis",
		Analysis: `MACRO: eth0 (external) has most spoofing (45K pps), eth4 (internal) has least (1.8K pps).
MICRO: eth0 uses interface-based allowlist with 3 prefixes; eth1 uses prefix-based validation.
COMBINED: Identify problem interfaces (macro), then audit their rule configurations (micro) for optimization.`,
		Records: records,
	}
}

// GetScenario3 - Policy Action Effectiveness Evaluation
// Demonstrates: Action distribution → Per-action details → Optimization insights
func GetScenario3() Scenario {
	now := time.Now()
	records := []SAVRecord{}
	
	// MACRO: Traffic distribution by policy action
	// Quantifies the effect of different enforcement policies
	actions := []struct {
		action  uint8
		name    string
		packets uint64
		bytes   uint64
	}{
		{PolicyActionPermit, "permit-monitoring", 1000, 600000},
		{PolicyActionDiscard, "discard-block", 50000, 30000000},
		{PolicyActionRateLimit, "rate-limit", 8000, 4800000},
		{PolicyActionRedirect, "redirect-honeypot", 500, 300000},
	}
	
	for _, act := range actions {
		records = append(records, SAVRecord{
			Type:        MacroRecord,
			Timestamp:   now,
			RuleType:    RuleTypeBlocklist, // Varies by action
			TargetType:  TargetTypePrefixBased,
			Action:      act.action,
			Rules:       nil,
			PacketCount: act.packets,
			ByteCount:   act.bytes,
			Label:       act.name,
		})
	}
	
	// MICRO: Discard action detail - blocklist hit
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  now,
		RuleType:   RuleTypeBlocklist,
		TargetType: TargetTypePrefixBased,
		Action:     PolicyActionDiscard,
		Rules: []RuleContent{
			// Prefix 2001:db8:bad::/48 is explicitly blocked on interface 5001
			{Interface: 5001, Prefix: net.ParseIP("2001:db8:bad::"), PrefixLen: 48, IsIPv6: true},
		},
		PacketCount: 1,
		ByteCount:   64,
		Label:       "discard-event",
	})
	
	// MICRO: Rate-limit action detail - allowlist miss
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  now,
		RuleType:   RuleTypeAllowlist,
		TargetType: TargetTypeInterfaceBased,
		Action:     PolicyActionRateLimit,
		Rules: []RuleContent{
			// Interface 6000 allowlist - packet didn't match, so rate-limited
			{Interface: 6000, Prefix: net.ParseIP("192.0.2.0"), PrefixLen: 24, IsIPv6: false},
			{Interface: 6000, Prefix: net.ParseIP("198.51.100.0"), PrefixLen: 24, IsIPv6: false},
		},
		PacketCount: 1,
		ByteCount:   64,
		Label:       "rate-limit-event",
	})
	
	// MICRO: Redirect action detail - security analysis
	records = append(records, SAVRecord{
		Type:       MicroRecord,
		Timestamp:  now,
		RuleType:   RuleTypeBlocklist,
		TargetType: TargetTypeInterfaceBased,
		Action:     PolicyActionRedirect,
		Rules: []RuleContent{
			// Interface 7000 blocks this prefix, redirects to honeypot for analysis
			{Interface: 7000, Prefix: net.ParseIP("10.99.99.0"), PrefixLen: 24, IsIPv6: false},
		},
		PacketCount: 1,
		ByteCount:   64,
		Label:       "redirect-event",
	})
	
	return Scenario{
		Name:        "Policy-Action-Effectiveness",
		Description: "Quantitative analysis of different SAV policy actions",
		Analysis: `MACRO: Discard dropped 50K packets (30MB bandwidth saved); Rate-limit handled 8K packets.
MICRO: Discard triggered by blocklist (2001:db8:bad::/48); Rate-limit by allowlist miss on if-6000.
COMBINED: 84% traffic discarded, 13% rate-limited, 2% redirected. Consider converting some rate-limits to discards.`,
		Records: records,
	}
}



// GetAllScenarios returns all predefined scenarios
func GetAllScenarios() []Scenario {
	return []Scenario{
		GetScenario1(),
		GetScenario2(),
		GetScenario3(),
	}
}

// GetScenarioByName returns a scenario by name
func GetScenarioByName(name string) *Scenario {
	scenarios := map[string]func() Scenario{
		"attack":      GetScenario1, // Time-series + forensics
		"interface":   GetScenario2, // Spatial distribution + rules
		"action":      GetScenario3, // Action effectiveness + optimization
	}
	
	if fn, ok := scenarios[name]; ok {
		s := fn()
		return &s
	}
	return nil
}
