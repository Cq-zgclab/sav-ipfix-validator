package main

import (
	"flag"
	"fmt"
	"net"
	"os"

	"github.com/Cq-zgclab/sav-ipfix-validator/pkg/sav"
)

func main() {
	scenario := flag.String("scenario", "", "Scenario name: attack, interface, action, or 'all'")
	output := flag.String("output", "", "Output file path")
	flag.Parse()

	if *scenario == "" || *output == "" {
		fmt.Println("SAV IPFIX Exporter - Hackthon Demo")
		fmt.Println("===================================")
		fmt.Println("\nUsage: sav_exporter --scenario <name> --output <file>")
		fmt.Println("\nAvailable scenarios (Macro + Micro + Combined):")
		fmt.Println("  attack     - Spoofing Attack: time-series trend + forensic details")
		fmt.Println("  interface  - Multi-Interface: traffic distribution + rule analysis")
		fmt.Println("  action     - Policy Actions: effectiveness + optimization insights")
		fmt.Println("  all        - Generate all 3 scenarios")
		fmt.Println("\nExample:")
		fmt.Println("  ./exporter --scenario attack --output scenario1.ipfix")
		fmt.Println("  ./exporter --scenario all --output all_scenarios.ipfix")
		os.Exit(1)
	}

	// Select scenarios to export
	var scenarios []sav.Scenario
	if *scenario == "all" {
		scenarios = sav.GetAllScenarios()
	} else {
		s := sav.GetScenarioByName(*scenario)
		if s == nil {
			fmt.Fprintf(os.Stderr, "❌ Unknown scenario: %s\n", *scenario)
			fmt.Println("Valid options: attack, interface, action, all")
			os.Exit(1)
		}
		scenarios = []sav.Scenario{*s}
	}

	// Create output file
	file, err := os.Create(*output)
	if err != nil {
		fmt.Fprintf(os.Stderr, "❌ Error creating file: %v\n", err)
		os.Exit(1)
	}
	defer file.Close()

	// Create writer
	writer := sav.NewSAVRecordWriter(file)
	defer writer.Close()

	fmt.Println("\n🚀 SAV IPFIX Exporter Starting...")
	fmt.Printf("📁 Output: %s\n\n", *output)

	totalRecords := 0

	// Export each scenario
	for _, sc := range scenarios {
		fmt.Printf("📊 Scenario: %s\n", sc.Name)
		fmt.Printf("   %s\n", sc.Description)
		
		recordCount := 0
		
		for _, record := range sc.Records {
			var err error
			
			// For simplicity, use first rule or zero values for macro records
			interfaceID := uint32(0)
			prefix := net.ParseIP("0.0.0.0")
			prefixLen := uint8(0)
			
			if len(record.Rules) > 0 {
				// Micro record with actual rules
				interfaceID = record.Rules[0].Interface
				prefix = record.Rules[0].Prefix
				prefixLen = record.Rules[0].PrefixLen
			}
			
			// Determine IPv4 or IPv6
			isIPv6 := prefix.To4() == nil
			
			if isIPv6 {
				err = writer.ExportIPv6InterfacePrefix(
					record.Timestamp,
					record.RuleType,
					record.TargetType,
					record.Action,
					interfaceID,
					prefix,
					prefixLen,
				)
			} else {
				err = writer.ExportIPv4InterfacePrefix(
					record.Timestamp,
					record.RuleType,
					record.TargetType,
					record.Action,
					interfaceID,
					prefix,
					prefixLen,
				)
			}
			
			if err != nil {
				fmt.Fprintf(os.Stderr, "❌ Error exporting record: %v\n", err)
				os.Exit(1)
			}
			recordCount++
			totalRecords++
		}
		
		fmt.Printf("   ✅ Exported: %d records\n", recordCount)
		fmt.Printf("   💡 %s\n\n", sc.Analysis)
	}

	fmt.Printf("🎉 Successfully exported %d total records\n", totalRecords)
	fmt.Printf("📈 Ready for visualization!\n")
}
