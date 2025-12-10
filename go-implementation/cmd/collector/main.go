package main

import (
	"fmt"
	"io"
	"os"

	"github.com/Cq-zgclab/sav-ipfix-validator/pkg/sav"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: sav_collector <input_file>")
		os.Exit(1)
	}

	inputFile := os.Args[1]

	// Open input file
	file, err := os.Open(inputFile)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error opening file: %v\n", err)
		os.Exit(1)
	}
	defer file.Close()

	// Create reader
	reader := sav.NewSAVRecordReader(file)

	fmt.Println("SAV IPFIX Collector")
	fmt.Println("===================")
	fmt.Printf("Input file: %s\n\n", inputFile)

	// Read and display records
	recordNum := 0
	for {
		record, err := reader.ReadRecord()
		if err == io.EOF {
			break
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error reading record: %v\n", err)
			break
		}

		recordNum++
		fmt.Printf("Record #%d:\n", recordNum)
		fmt.Printf("  Timestamp: %s\n", record.Timestamp.Format("2006-01-02 15:04:05.000"))
		fmt.Printf("  Rule Type: %d\n", record.RuleType)
		fmt.Printf("  Target Type: %d\n", record.TargetType)
		fmt.Printf("  Policy Action: %d\n", record.PolicyAction)
		fmt.Printf("  Mapping Count: %d\n", len(record.Mappings))

		// Display mappings
		for i, mapping := range record.Mappings {
			ipType := "IPv4"
			if mapping.IsIPv6 {
				ipType = "IPv6"
			}
			fmt.Printf("    [%d] Interface %d -> %s/%d (%s)\n",
				i+1,
				mapping.InterfaceID,
				mapping.Prefix,
				mapping.PrefixLen,
				ipType)
		}

		fmt.Println()
	}

	fmt.Printf("\n✅ Successfully collected %d records\n", reader.CollectedCount())
}
