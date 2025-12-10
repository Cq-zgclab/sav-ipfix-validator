#!/bin/bash

# SAV IPFIX Demo System Quick Test Script

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     SAV IPFIX Demo System - Quick Test                   ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if server is running
echo -n "Checking if server is running... "
if pgrep -f "sav-demo-lite" > /dev/null; then
    echo -e "${GREEN}✓ Running${NC}"
else
    echo -e "${RED}✗ Not running${NC}"
    echo "Starting server..."
    ./sav-demo-lite --port 8888 &
    sleep 2
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Testing API Endpoints"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test status endpoint
echo -n "1. GET /api/status ... "
RESPONSE=$(curl -s http://localhost:8888/api/status)
if [[ $RESPONSE == *"total_records"* ]]; then
    echo -e "${GREEN}✓ OK${NC}"
    echo "   Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC}"
fi
echo ""

# Test start control
echo -n "2. POST /api/control/start ... "
RESPONSE=$(curl -s -X POST http://localhost:8888/api/control/start)
if [[ $RESPONSE == *"started"* ]]; then
    echo -e "${GREEN}✓ OK${NC}"
    echo "   Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC}"
fi
echo ""

# Wait a bit
sleep 1

# Test pause control
echo -n "3. POST /api/control/pause ... "
RESPONSE=$(curl -s -X POST http://localhost:8888/api/control/pause)
if [[ $RESPONSE == *"paused"* ]]; then
    echo -e "${GREEN}✓ OK${NC}"
    echo "   Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC}"
fi
echo ""

# Test speed control
echo -n "4. POST /api/control/speed?value=2.0 ... "
RESPONSE=$(curl -s -X POST "http://localhost:8888/api/control/speed?value=2.0")
if [[ $RESPONSE == *"speed"* ]]; then
    echo -e "${GREEN}✓ OK${NC}"
    echo "   Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC}"
fi
echo ""

# Test reset control
echo -n "5. POST /api/control/reset ... "
RESPONSE=$(curl -s -X POST http://localhost:8888/api/control/reset)
if [[ $RESPONSE == *"reset"* ]]; then
    echo -e "${GREEN}✓ OK${NC}"
    echo "   Response: $RESPONSE"
else
    echo -e "${RED}✗ FAILED${NC}"
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Testing SSE Stream"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "6. Testing SSE connection (will display first 5 events)..."
echo "   Endpoint: /api/stream/events"
echo ""

# Start playback
curl -s -X POST http://localhost:8888/api/control/start > /dev/null

# Connect to SSE and show first 5 events
timeout 5 curl -s http://localhost:8888/api/stream/events | head -30

echo ""
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✓ All tests completed!${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📊 Access the dashboard:"
echo "   http://localhost:8888"
echo ""
echo "🔧 API Documentation:"
echo "   See README.md for complete API reference"
echo ""
