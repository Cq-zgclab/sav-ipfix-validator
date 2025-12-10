#!/usr/bin/env python3
"""
Simple IPFIX to JSON converter for SAV records
Parses ipfixDump output and converts to JSON for web visualization
"""

import json
import re
import subprocess
import sys
from datetime import datetime

def parse_ipfix_dump(ipfix_file):
    """Parse ipfixDump output and extract SAV records"""
    
    # Run ipfixDump
    result = subprocess.run(
        ['ipfixDump', '--in', ipfix_file],
        capture_output=True,
        text=True
    )
    
    if result.returncode != 0:
        print(f"Error running ipfixDump: {result.stderr}", file=sys.stderr)
        return None
    
    output = result.stdout
    records = []
    current_record = None
    in_stl = False
    current_stl_record = None
    
    for line in output.split('\n'):
        # Match data record start
        if '--- Data Record' in line:
            if current_record:
                records.append(current_record)
            current_record = {
                'recordId': len(records) + 1,
                'timestamp': None,
                'ruleType': None,
                'ruleTypeName': None,
                'targetType': None,
                'policyAction': None,
                'rules': []
            }
            in_stl = False
            
        # Match timestamp
        elif 'observationTimeMilliseconds' in line and current_record:
            match = re.search(r'(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})', line)
            if match:
                ts_str = match.group(1)
                dt = datetime.strptime(ts_str, '%Y-%m-%d %H:%M:%S.%f')
                current_record['timestamp'] = int(dt.timestamp() * 1000)
                
        # Match ruleType (6871/1)
        elif '_alienInformationElement         (6871/1)' in line and current_record:
            match = re.search(r'0x([0-9a-fA-F]{2})', line)
            if match:
                rule_type = int(match.group(1), 16)
                current_record['ruleType'] = rule_type
                rule_type_names = {0: 'allowlist', 1: 'blocklist', 2: 'prefix', 3: 'aspath'}
                current_record['ruleTypeName'] = rule_type_names.get(rule_type, 'unknown')
                
        # Match targetType (6871/2)
        elif '_alienInformationElement         (6871/2)' in line and current_record:
            match = re.search(r'0x([0-9a-fA-F]{2})', line)
            if match:
                current_record['targetType'] = int(match.group(1), 16)
                
        # Match policyAction (6871/4)
        elif '_alienInformationElement         (6871/4)' in line and current_record:
            match = re.search(r'0x([0-9a-fA-F]{2})', line)
            if match:
                current_record['policyAction'] = int(match.group(1), 16)
                
        # Match STL start
        elif 'subTemplateList' in line and 'count:' in line:
            in_stl = True
            
        # Match STL record start
        elif '+--- STL Record' in line and in_stl:
            if current_stl_record and 'interfaceId' in current_stl_record:
                current_record['rules'].append(current_stl_record)
            current_stl_record = {}
            
        # Match interface
        elif '+ ingressInterface' in line and current_stl_record is not None:
            match = re.search(r': (\d+)', line)
            if match:
                # Convert from network byte order display
                interface_val = int(match.group(1))
                # Reinterpret as little-endian
                interface_id = (interface_val >> 24) & 0xFF
                current_stl_record['interfaceId'] = interface_id
                
        # Match source prefix
        elif '+ sourceIPv4Prefix' in line and current_stl_record is not None:
            match = re.search(r': ([\d.]+)', line)
            if match:
                # IP is shown in reverse byte order, fix it
                ip_parts = match.group(1).split('.')
                ip_fixed = '.'.join(reversed(ip_parts))
                current_stl_record['sourcePrefix'] = ip_fixed
                
        # Match prefix length
        elif '+ sourceIPv4PrefixLength' in line and current_stl_record is not None:
            match = re.search(r': (\d+)', line)
            if match:
                current_stl_record['prefixLength'] = int(match.group(1))
    
    # Add last STL record if any
    if current_stl_record and 'interfaceId' in current_stl_record and current_record:
        current_record['rules'].append(current_stl_record)
    
    # Add last record
    if current_record:
        records.append(current_record)
    
    return records

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.ipfix> <output.json>")
        print(f"Example: {sys.argv[0]} test_data/sample_sav.ipfix web/data.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    print(f"Parsing {input_file}...")
    records = parse_ipfix_dump(input_file)
    
    if records is None:
        sys.exit(1)
    
    # Create final JSON structure
    output_data = {
        'totalRecords': len(records),
        'records': records,
        'generatedAt': int(datetime.now().timestamp()),
        'ipVersion': 4  # Currently only IPv4
    }
    
    # Write JSON
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"✅ Parsed {len(records)} records")
    print(f"✅ JSON output: {output_file}")
    
    # Print summary
    for rec in records:
        print(f"   Record #{rec['recordId']}: {rec['ruleTypeName']}, {len(rec['rules'])} rules")

if __name__ == '__main__':
    main()
