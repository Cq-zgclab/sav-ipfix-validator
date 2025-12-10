#!/usr/bin/env python3
"""
SAV IPFIX Collector using pyfixbuf

This example demonstrates how to collect and parse SAV validation records
using pyfixbuf with SubTemplateList support.

Based on draft-cao-opsawg-ipfix-sav-01
"""

import sys
import json
from datetime import datetime

try:
    import pyfixbuf
except ImportError:
    print("ERROR: pyfixbuf not installed")
    print("Please install:")
    print("  1. libfixbuf: https://tools.netsa.cert.org/fixbuf/")
    print("  2. pyfixbuf: https://tools.netsa.cert.org/pyfixbuf/")
    sys.exit(1)


# SAV Information Elements (same as exporter)
SAV_PEN = 12345

SAV_IE_DEFINITIONS = [
    ("savRuleType", 1, 1, SAV_PEN, "uint8"),
    ("savTargetType", 2, 1, SAV_PEN, "uint8"),
    ("savPolicyAction", 3, 1, SAV_PEN, "uint8"),
    ("savMatchedContentList", 4, 0xFFFF, SAV_PEN, "subTemplateList"),
    ("savInterfaceID", 5, 4, SAV_PEN, "uint32"),
    ("savPrefix", 6, 4, SAV_PEN, "ipv4Address"),
    ("savPrefixLength", 7, 1, SAV_PEN, "uint8"),
]


class SAVCollector:
    """SAV IPFIX Collector using pyfixbuf"""
    
    def __init__(self, input_file):
        """Initialize SAV Collector
        
        Args:
            input_file: Path to input IPFIX file
        """
        self.input_file = input_file
        self.records = []
        
        # Create information model
        self.infomodel = pyfixbuf.InfoModel()
        
        # Add SAV Information Elements
        print("Adding SAV Information Elements...")
        for ie_name, ie_id, ie_len, ie_pen, ie_type in SAV_IE_DEFINITIONS:
            print(f"  - {ie_name} (ID={ie_id}, PEN={ie_pen})")
        
        # Create collector
        self.collector = pyfixbuf.Collector()
        self.collector.init_file(input_file)
        
        # Create session
        self.session = pyfixbuf.Session(self.infomodel)
        
        # Create buffer
        self.buffer = pyfixbuf.Buffer()
        self.buffer.init_collection(self.session, self.collector)
        
        print(f"✅ SAV Collector initialized: {input_file}")
    
    def collect(self):
        """Collect all records from IPFIX file"""
        print("\n📥 Collecting SAV records...")
        
        record_count = 0
        
        # Iterate over all records
        for record in self.buffer:
            record_count += 1
            parsed = self._parse_record(record)
            self.records.append(parsed)
            
            # Print summary
            print(f"  Record #{record_count}:")
            print(f"    Timestamp: {parsed['timestamp']}")
            print(f"    Source: {parsed['sourceIP']}")
            print(f"    Action: {parsed['actionName']} ({parsed['action']})")
            print(f"    Mappings: {len(parsed['mappings'])}")
        
        print(f"\n✅ Collected {record_count} records")
        return self.records
    
    def _parse_record(self, record):
        """Parse a single SAV record
        
        Args:
            record: pyfixbuf record object
            
        Returns:
            dict: Parsed record data
        """
        # Extract basic fields
        timestamp_ms = record.get("flowStartMilliseconds", 0)
        timestamp = datetime.fromtimestamp(timestamp_ms / 1000.0).isoformat()
        
        src_ip = record.get("sourceIPv4Address", "0.0.0.0")
        dst_ip = record.get("destinationIPv4Address", "0.0.0.0")
        
        rule_type = record.get("savRuleType", 0)
        target_type = record.get("savTargetType", 0)
        action = record.get("savPolicyAction", 0)
        
        # Map enums to names
        rule_name = {0: "Allowlist", 1: "Blocklist"}.get(rule_type, "Unknown")
        target_name = {0: "Interface", 1: "Prefix"}.get(target_type, "Unknown")
        action_name = {
            0: "Permit", 
            1: "Discard", 
            2: "Rate-limit", 
            3: "Redirect"
        }.get(action, "Unknown")
        
        # Parse SubTemplateList (savMatchedContentList)
        mappings = []
        if "savMatchedContentList" in record:
            stl = record["savMatchedContentList"]
            
            # Iterate over SubTemplateList entries
            for entry in stl:
                mapping = {
                    "interfaceID": entry.get("savInterfaceID", 0),
                    "prefix": entry.get("savPrefix", "0.0.0.0"),
                    "prefixLength": entry.get("savPrefixLength", 0),
                }
                mappings.append(mapping)
        
        return {
            "timestamp": timestamp,
            "sourceIP": src_ip,
            "destinationIP": dst_ip,
            "ruleType": rule_type,
            "ruleName": rule_name,
            "targetType": target_type,
            "targetName": target_name,
            "action": action,
            "actionName": action_name,
            "mappings": mappings,
        }
    
    def to_json(self, output_file=None):
        """Export collected records to JSON
        
        Args:
            output_file: Optional JSON output file path
            
        Returns:
            str: JSON string
        """
        json_data = json.dumps(self.records, indent=2)
        
        if output_file:
            with open(output_file, 'w') as f:
                f.write(json_data)
            print(f"✅ JSON exported: {output_file}")
        
        return json_data
    
    def analyze(self):
        """Perform analysis on collected records"""
        print("\n" + "=" * 60)
        print("📊 SAV Analysis Report")
        print("=" * 60)
        
        if not self.records:
            print("No records to analyze")
            return
        
        # Action distribution
        action_counts = {}
        for record in self.records:
            action = record["actionName"]
            action_counts[action] = action_counts.get(action, 0) + 1
        
        print("\n🎯 Policy Actions:")
        for action, count in sorted(action_counts.items()):
            print(f"  {action}: {count} records")
        
        # Rule type distribution
        rule_counts = {}
        for record in self.records:
            rule = record["ruleName"]
            rule_counts[rule] = rule_counts.get(rule, 0) + 1
        
        print("\n📋 Rule Types:")
        for rule, count in sorted(rule_counts.items()):
            print(f"  {rule}: {count} records")
        
        # Interface statistics
        interface_map = {}
        for record in self.records:
            for mapping in record["mappings"]:
                iface = mapping["interfaceID"]
                if iface not in interface_map:
                    interface_map[iface] = {
                        "count": 0,
                        "prefixes": set()
                    }
                interface_map[iface]["count"] += 1
                prefix_str = f"{mapping['prefix']}/{mapping['prefixLength']}"
                interface_map[iface]["prefixes"].add(prefix_str)
        
        print("\n🔌 Interface Distribution:")
        for iface, data in sorted(interface_map.items()):
            print(f"  Interface {iface}:")
            print(f"    Mappings: {data['count']}")
            print(f"    Unique Prefixes: {len(data['prefixes'])}")
            for prefix in sorted(data["prefixes"]):
                print(f"      - {prefix}")
        
        print("\n" + "=" * 60)


def main():
    """Example: Collect and analyze SAV validation records"""
    
    if len(sys.argv) < 2:
        print("Usage: python sav_collector_example.py <input.ipfix> [output.json]")
        print("\nExample:")
        print("  python sav_collector_example.py sav_example.ipfix sav_output.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_json = sys.argv[2] if len(sys.argv) > 2 else None
    
    print("=" * 60)
    print("SAV IPFIX Collector Example (pyfixbuf)")
    print("=" * 60)
    print()
    
    try:
        # Create collector
        collector = SAVCollector(input_file)
        
        # Collect records
        records = collector.collect()
        
        # Analyze
        collector.analyze()
        
        # Export to JSON if requested
        if output_json:
            print()
            collector.to_json(output_json)
        
        print()
        print("=" * 60)
        print(f"✅ Processing complete: {len(records)} records")
        print("=" * 60)
        
    except FileNotFoundError:
        print(f"❌ Error: File not found: {input_file}")
        sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
