#!/bin/bash
#
# SAV IPFIX Hackathon - One-click Run Script
# 
# This script:
# 1. Generates test data from c-implementation
# 2. Converts IPFIX to JSON using ipfix2json
# 3. Post-processes JSON for web display
# 4. Starts HTTP server for web frontend
#

set -e  # Exit on error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
C_IMPL_ROOT="$(cd "$PROJECT_ROOT/../c-implementation" && pwd)"

echo "╔═══════════════════════════════════════════════════════╗"
echo "║  SAV IPFIX Hackathon - Auto Run Script               ║"
echo "╚═══════════════════════════════════════════════════════╝"
echo ""

# Step 1: Generate test data
echo "📦 Step 1: Generating test data from c-implementation..."
cd "$C_IMPL_ROOT"

if [ ! -f "build/bin/test_test_sav_e2e" ]; then
    echo "   Building c-implementation test..."
    make tests
fi

echo "   Running test_sav_e2e to generate IPFIX data..."
./build/bin/test_test_sav_e2e > /dev/null

if [ -f "test_sav_e2e.ipfix" ]; then
    cp test_sav_e2e.ipfix "$PROJECT_ROOT/test_data/sample_sav.ipfix"
    echo "   ✅ Test data generated: test_data/sample_sav.ipfix"
else
    echo "   ❌ Failed to generate test data"
    exit 1
fi

# Step 2: Convert IPFIX to JSON
echo ""
echo "🔄 Step 2: Converting IPFIX to JSON using ipfix2json..."
cd "$PROJECT_ROOT"

ipfix2json --element-file sav_ies.xml --in test_data/sample_sav.ipfix --out web/data_raw.json
echo "   ✅ Raw JSON created: web/data_raw.json"

# Step 3: Post-process JSON
echo ""
echo "🔧 Step 3: Post-processing JSON (fixing byte order, decoding fields)..."
python3 src/process_ipfix_json.py web/data_raw.json web/data.json
echo "   ✅ Final JSON created: web/data.json"

# Step 4: Build and start Go web server
echo ""
echo "🌐 Step 4: Starting Go HTTP server..."
cd "$PROJECT_ROOT"

# Build Go server if not exists or source changed
if [ ! -f "server" ] || [ "server.go" -nt "server" ]; then
    echo "   Building Go server..."
    go build -o server server.go
    echo "   ✅ Server compiled"
fi

# Check if port 8000 is already in use
if lsof -Pi :8000 -sTCP:LISTEN -t >/dev/null 2>&1 ; then
    echo "   ⚠️  Port 8000 is already in use"
    echo "   You can manually access http://localhost:8000"
else
    echo "   Starting Go HTTP server on port 8000..."
    ./server 8000
fi
