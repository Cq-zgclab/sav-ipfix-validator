#!/usr/bin/env python3
"""
SAV IPFIX Exporter using pyfixbuf

This example demonstrates how to export SAV validation records
using pyfixbuf with SubTemplateList for savMatchedContentList.

Based on draft-cao-opsawg-ipfix-sav-01
"""

import sys
import time

try:
    import pyfixbuf
except ImportError:
    print("ERROR: pyfixbuf not installed")
    print("Please install:")
    print("  1. libfixbuf: https://tools.netsa.cert.org/fixbuf/")
    print("  2. pyfixbuf: https://tools.netsa.cert.org/pyfixbuf/")
    sys.exit(1)


# SAV Information Elements (Private Enterprise Number: 12345 - example)
SAV_PEN = 12345

SAV_IE_DEFINITIONS = [
    # IE Name, ID, Length, PEN, Type
    ("savRuleType", 1, 1, SAV_PEN, "uint8"),
    ("savTargetType", 2, 1, SAV_PEN, "uint8"),
    ("savPolicyAction", 3, 1, SAV_PEN, "uint8"),
    ("savMatchedContentList", 4, 0xFFFF, SAV_PEN, "subTemplateList"),
    # Elements in SubTemplateList
    ("savInterfaceID", 5, 4, SAV_PEN, "uint32"),
    ("savPrefix", 6, 4, SAV_PEN, "ipv4Address"),  # or ipv6Address
    ("savPrefixLength", 7, 1, SAV_PEN, "uint8"),
]


class SAVExporter:
    """SAV IPFIX Exporter using pyfixbuf"""
    
    def __init__(self, output_file):
        """Initialize SAV Exporter
        
        Args:
            output_file: Path to output IPFIX file
        """
        self.output_file = output_file
        
        # Create information model
        self.infomodel = pyfixbuf.InfoModel()
        
        # Add SAV Information Elements
        print("Adding SAV Information Elements...")
        for ie_name, ie_id, ie_len, ie_pen, ie_type in SAV_IE_DEFINITIONS:
            # Note: pyfixbuf API may vary, this is conceptual
            # Actual API: infomodel.add_element(name, id, length, pen, type)
            print(f"  - {ie_name} (ID={ie_id}, PEN={ie_pen})")
        
        # Define main template (SAV validation record)
        self.sav_template = pyfixbuf.Template(self.infomodel)
        self.sav_template.add_spec_list([
            pyfixbuf.InfoElementSpec("flowStartMilliseconds"),
            pyfixbuf.InfoElementSpec("sourceIPv4Address"),
            pyfixbuf.InfoElementSpec("destinationIPv4Address"),
            pyfixbuf.InfoElementSpec("savRuleType"),
            pyfixbuf.InfoElementSpec("savTargetType"),
            pyfixbuf.InfoElementSpec("savPolicyAction"),
            pyfixbuf.InfoElementSpec("savMatchedContentList"),
        ])
        
        # Define subtemplate (interface-prefix mapping)
        self.mapping_record = pyfixbuf.Record(self.infomodel)
        self.mapping_record.add_element_list([
            "savInterfaceID",
            "savPrefix",
            "savPrefixLength",
        ])
        
        # Create exporter
        self.exporter = pyfixbuf.Exporter()
        self.exporter.init_file(output_file)
        
        # Create session
        self.session = pyfixbuf.Session(self.infomodel)
        self.session.add_internal_template(self.sav_template, 256)
        
        # Create buffer
        self.buffer = pyfixbuf.Buffer(self.sav_template)
        self.buffer.init_export(self.session, self.exporter)
        
        print(f"✅ SAV Exporter initialized: {output_file}")
    
    def export_record(self, timestamp, src_ip, dst_ip, rule_type, 
                     target_type, action, mappings):
        """Export a SAV validation record
        
        Args:
            timestamp: Flow start time (Unix timestamp in seconds)
            src_ip: Source IPv4 address (string)
            dst_ip: Destination IPv4 address (string)
            rule_type: 0=Allowlist, 1=Blocklist
            target_type: 0=Interface-based, 1=Prefix-based
            action: 0=Permit, 1=Discard, 2=Rate-limit, 3=Redirect
            mappings: List of (interface_id, prefix, prefix_length) tuples
        """
        # Create main record
        record = pyfixbuf.Record(self.infomodel, self.sav_template)
        
        # Set basic fields
        record["flowStartMilliseconds"] = int(timestamp * 1000)
        record["sourceIPv4Address"] = src_ip
        record["destinationIPv4Address"] = dst_ip
        record["savRuleType"] = rule_type
        record["savTargetType"] = target_type
        record["savPolicyAction"] = action
        
        # Set SubTemplateList
        stl = record["savMatchedContentList"]
        stl.init(self.mapping_record)
        
        # Add mappings to SubTemplateList
        for interface_id, prefix, prefix_len in mappings:
            mapping = stl.add_record()
            mapping["savInterfaceID"] = interface_id
            mapping["savPrefix"] = prefix
            mapping["savPrefixLength"] = prefix_len
        
        # Append to buffer
        self.buffer.append(record)
        
        print(f"📤 Exported: src={src_ip}, action={action}, mappings={len(mappings)}")
    
    def close(self):
        """Flush and close exporter"""
        self.buffer.emit()
        print("✅ Export complete")


def main():
    """Example: Export SAV validation records"""
    
    output_file = "sav_example.ipfix"
    
    print("=" * 60)
    print("SAV IPFIX Exporter Example (pyfixbuf)")
    print("=" * 60)
    print()
    
    # Create exporter
    exporter = SAVExporter(output_file)
    
    # Example 1: Allowlist validation failure
    print("\n📊 Example 1: Allowlist validation failure")
    exporter.export_record(
        timestamp=time.time(),
        src_ip="192.0.2.123",
        dst_ip="10.1.1.100",
        rule_type=0,  # Allowlist
        target_type=0,  # Interface-based
        action=1,  # Discard
        mappings=[
            (1000, "10.1.1.0", 24),
            (1000, "10.2.2.0", 24),
            (1000, "10.3.3.0", 24),
        ]
    )
    
    # Example 2: Blocklist hit
    print("\n📊 Example 2: Blocklist hit")
    exporter.export_record(
        timestamp=time.time(),
        src_ip="198.51.100.50",
        dst_ip="10.1.1.200",
        rule_type=1,  # Blocklist
        target_type=1,  # Prefix-based
        action=1,  # Discard
        mappings=[
            (5001, "198.51.100.0", 24),
        ]
    )
    
    # Example 3: Rate-limit on allowlist miss
    print("\n📊 Example 3: Rate-limit on allowlist miss")
    exporter.export_record(
        timestamp=time.time(),
        src_ip="203.0.113.5",
        dst_ip="10.1.1.150",
        rule_type=0,  # Allowlist
        target_type=0,  # Interface-based
        action=2,  # Rate-limit
        mappings=[
            (6000, "203.0.113.0", 24),
            (6000, "203.0.114.0", 24),
        ]
    )
    
    # Close exporter
    exporter.close()
    
    print()
    print("=" * 60)
    print(f"✅ Output file: {output_file}")
    print("=" * 60)


if __name__ == "__main__":
    main()
