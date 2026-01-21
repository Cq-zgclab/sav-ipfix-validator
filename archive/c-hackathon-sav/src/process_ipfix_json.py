#!/usr/bin/env python3
"""
Post-process ipfix2json output for SAV IPFIX records
- Fix IP address byte order
- Decode alien IEs to SAV fields
- Fix interface ID byte order
- Convert to web-friendly JSON format
"""

import json
import sys
import base64
import struct
from datetime import datetime

def fix_ip_address(ip_str):
    """Fix reversed IP address (0.2.0.192 -> 192.0.2.0)"""
    parts = ip_str.split('.')
    return '.'.join(reversed(parts))

def fix_interface_id(interface_val):
    """Fix interface ID from network byte order display"""
    # The value is displayed as if read as big-endian uint32
    # but it's actually a uint16 in network byte order
    # Extract the first byte (MSB of the displayed value)
    return (interface_val >> 24) & 0xFF

def decode_alien_ie(base64_str):
    """Decode base64 alien IE to integer"""
    data = base64.b64decode(base64_str)
    if len(data) == 1:
        return struct.unpack('B', data)[0]
    return None

def parse_timestamp(ts_str):
    """Parse timestamp string to milliseconds since epoch"""
    # Format: "2025-12-10 12:28:38.000Z"
    dt = datetime.strptime(ts_str, '%Y-%m-%d %H:%M:%S.%fZ')
    return int(dt.timestamp() * 1000)

def process_jsonl(input_file, output_file):
    """Process JSON Lines output from ipfix2json"""
    
    records = []
    templates = {}
    
    with open(input_file, 'r') as f:
        for line in f:
            if not line.strip():
                continue
                
            obj = json.loads(line)
            
            # Skip template records
            if any(k.startswith('template_record:') for k in obj.keys()):
                # Store template info
                for k, v in obj.items():
                    if k.startswith('template_record:'):
                        tid = k.split(':')[1].split('(')[0]
                        templates[tid] = v
                continue
            
            # Process data records
            for key, value in obj.items():
                if key.startswith('template:0x02bc'):  # Main template
                    record = {
                        'recordId': len(records) + 1,
                        'timestamp': None,
                        'ruleType': None,
                        'ruleTypeName': None,
                        'targetType': None,
                        'policyAction': None,
                        'rules': []
                    }
                    
                    # Extract timestamp
                    if 'observationTimeMilliseconds' in value:
                        record['timestamp'] = parse_timestamp(value['observationTimeMilliseconds'])
                    
                    # Decode alien IEs (SAV fields)
                    alien_ies = []
                    for k, v in value.items():
                        if k == '_alienInformationElement':
                            alien_ies.append(decode_alien_ie(v))
                    
                    # Alien IEs order: [ruleType, targetType, policyAction]
                    if len(alien_ies) >= 3:
                        record['ruleType'] = alien_ies[0]
                        record['targetType'] = alien_ies[1]
                        record['policyAction'] = alien_ies[2]
                        
                        # Rule type name
                        rule_type_names = {
                            0: 'allowlist',
                            1: 'blocklist', 
                            2: 'prefix',
                            3: 'aspath'
                        }
                        record['ruleTypeName'] = rule_type_names.get(record['ruleType'], 'unknown')
                    
                    # Extract SubTemplateList
                    for k, v in value.items():
                        if k.startswith('template:0x0258'):  # Sub-template
                            if isinstance(v, list):
                                for sub_rec in v:
                                    rule = {
                                        'interfaceId': fix_interface_id(sub_rec.get('ingressInterface', 0)),
                                        'sourcePrefix': fix_ip_address(sub_rec.get('sourceIPv4Prefix', '0.0.0.0')),
                                        'prefixLength': sub_rec.get('sourceIPv4PrefixLength', 0)
                                    }
                                    record['rules'].append(rule)
                    
                    records.append(record)
    
    # Create final output
    output = {
        'totalRecords': len(records),
        'records': records,
        'generatedAt': int(datetime.now().timestamp()),
        'ipVersion': 4
    }
    
    with open(output_file, 'w') as f:
        json.dump(output, f, indent=2)
    
    return len(records)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.json> <output.json>")
        print(f"Example: {sys.argv[0]} web/data_raw.json web/data.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    print(f"Processing {input_file}...")
    count = process_jsonl(input_file, output_file)
    
    print(f"✅ Processed {count} records")
    print(f"✅ Output: {output_file}")

if __name__ == '__main__':
    main()
