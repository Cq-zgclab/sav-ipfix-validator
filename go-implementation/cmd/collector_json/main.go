package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"time"

	"github.com/Cq-zgclab/sav-ipfix-validator/pkg/sav"
)

// JSONMapping represents interface-prefix mapping
type JSONMapping struct {
	InterfaceID  uint32 `json:"interface_id"`
	Prefix       string `json:"prefix"`
	PrefixLength uint8  `json:"prefix_length"`
	IsIPv6       bool   `json:"is_ipv6"`
}

// JSONRecord represents a single SAV IPFIX record in JSON format
type JSONRecord struct {
	RecordNumber     int            `json:"record_number"`
	Timestamp        time.Time      `json:"timestamp"`
	TimestampStr     string         `json:"timestamp_str"`
	RuleType         uint8          `json:"rule_type"`
	RuleTypeName     string         `json:"rule_type_name"`
	TargetType       uint8          `json:"target_type"`
	TargetTypeName   string         `json:"target_type_name"`
	PolicyAction     uint8          `json:"policy_action"`
	PolicyActionName string         `json:"policy_action_name"`
	Mappings         []JSONMapping  `json:"mappings"`
	MappingCount     int            `json:"mapping_count"`
}

// JSONOutput represents the complete output structure
type JSONOutput struct {
	Metadata struct {
		InputFile    string    `json:"input_file"`
		ProcessedAt  time.Time `json:"processed_at"`
		TotalRecords int       `json:"total_records"`
		RFCCompliance []string `json:"rfc_compliance"`
	} `json:"metadata"`
	Records []JSONRecord `json:"records"`
}

func main() {
	input := flag.String("input", "", "Input IPFIX file path")
	output := flag.String("output", "", "Output JSON file path (use '-' for stdout)")
	flag.Parse()

	if *input == "" || *output == "" {
		fmt.Println("SAV IPFIX to JSON Converter")
		fmt.Println("===========================")
		fmt.Println("\nUsage: collector_json --input <ipfix_file> --output <json_file>")
		fmt.Println("\nExample:")
		fmt.Println("  ./collector_json --input data/all_scenarios.ipfix --output web/data.json")
		fmt.Println("  ./collector_json --input data/scenario1.ipfix --output - | jq .")
		os.Exit(1)
	}

	// Open input file
	file, err := os.Open(*input)
	if err != nil {
		fmt.Fprintf(os.Stderr, "❌ Error opening file: %v\n", err)
		os.Exit(1)
	}
	defer file.Close()

	// Create reader
	reader := sav.NewSAVRecordReader(file)

	fmt.Printf("📖 Reading IPFIX file: %s\n", *input)

	// Prepare output
	jsonOutput := JSONOutput{}
	jsonOutput.Metadata.InputFile = *input
	jsonOutput.Metadata.ProcessedAt = time.Now()
	jsonOutput.Metadata.RFCCompliance = []string{
		"RFC 7011 - IPFIX Protocol Specification",
		"RFC 6313 - Export of Structured Data in IPFIX",
		"draft-cao-opsawg-ipfix-sav-01 - SAV Information Elements",
	}

	// Read all records
	recordNum := 0
	for {
		record, err := reader.ReadRecord()
		if err == io.EOF {
			break
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "⚠️  Error reading record %d: %v\n", recordNum+1, err)
			break
		}

		recordNum++
		
		// Convert mappings
		var jsonMappings []JSONMapping
		for _, mapping := range record.Mappings {
			jsonMappings = append(jsonMappings, JSONMapping{
				InterfaceID:  mapping.InterfaceID,
				Prefix:       mapping.Prefix.String(),
				PrefixLength: mapping.PrefixLen,
				IsIPv6:       mapping.IsIPv6,
			})
		}
		
		jsonRecord := JSONRecord{
			RecordNumber:     recordNum,
			Timestamp:        record.Timestamp,
			TimestampStr:     record.Timestamp.Format("2006-01-02 15:04:05.000"),
			RuleType:         record.RuleType,
			RuleTypeName:     sav.RuleTypeName(record.RuleType),
			TargetType:       record.TargetType,
			TargetTypeName:   sav.TargetTypeName(record.TargetType),
			PolicyAction:     record.PolicyAction,
			PolicyActionName: sav.PolicyActionName(record.PolicyAction),
			Mappings:         jsonMappings,
			MappingCount:     len(jsonMappings),
		}

		jsonOutput.Records = append(jsonOutput.Records, jsonRecord)
	}

	jsonOutput.Metadata.TotalRecords = recordNum

	fmt.Printf("✅ Parsed %d IPFIX records\n", recordNum)
	fmt.Printf("📊 Converting to JSON...\n")

	// Marshal to JSON
	jsonData, err := json.MarshalIndent(jsonOutput, "", "  ")
	if err != nil {
		fmt.Fprintf(os.Stderr, "❌ Error marshaling JSON: %v\n", err)
		os.Exit(1)
	}

	// Write output
	if *output == "-" {
		fmt.Println(string(jsonData))
	} else {
		err = os.WriteFile(*output, jsonData, 0644)
		if err != nil {
			fmt.Fprintf(os.Stderr, "❌ Error writing file: %v\n", err)
			os.Exit(1)
		}
		fmt.Printf("💾 Output written to: %s\n", *output)
	}

	// Print summary
	fmt.Printf("\n" + "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
	fmt.Printf("📈 Summary:\n")
	fmt.Printf("   Input:  %s (IPFIX binary format)\n", *input)
	fmt.Printf("   Output: %s (JSON format)\n", *output)
	fmt.Printf("   Records: %d\n", recordNum)
	fmt.Printf("\n✨ IPFIX data successfully converted to JSON for visualization!\n")
	fmt.Printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
}
