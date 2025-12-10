# SAV IPFIX Validator

Source Address Validation (SAV) telemetry using IPFIX protocol.

**Three Independent Implementations:**
- 🎬 **Demo System** - Real-time streaming demo (NEW! Recommended for demos)
- 🐹 **Go Implementation** - Production-ready, data generation and parsing
- 🔧 **C Implementation** - Research prototype using libfixbuf

---

## 🎬 Real-Time Streaming Demo (NEW!)

**最佳演示体验** - 实时流式 SAV IPFIX 遥测展示系统

### Quick Start
```bash
cd sav-demo-lite
./sav-demo-lite
# Open: http://localhost:8888
```

### Features
- 🔄 **Real-time SSE streaming** - 27 SAV IPFIX records
- 🎮 **Playback control** - Start/Pause/Reset/Speed (0.5x~4x)
- 📊 **Live analysis panels** - Root cause, mode stats, policy tracking
- ⚡ **Dynamic visualization** - Timeline + Event stream + Charts

**Perfect for**: Hackathon demos, technical presentations, SAV concept validation

See `sav-demo-lite/README.md` and `sav-demo-lite/DEMO_SCRIPT.md` for details.

---

## 🐹 Go Implementation (Recommended)

### Quick Start

```bash
cd go-implementation

# 1. Generate IPFIX data
go build -o bin/exporter ./cmd/exporter
./bin/exporter --scenario all --output data/all_scenarios.ipfix

# 2. Convert to JSON
go build -o bin/collector_json ./cmd/collector_json
./bin/collector_json --input data/all_scenarios.ipfix --output web/data.json

# 3. View Dashboard
python3 -m http.server 8000
# Open: http://localhost:8000/web/index.html
```

See `go-implementation/README.md` for complete documentation.

---

## 🔧 C Implementation (Research)

Using libfixbuf for IPFIX encoding/decoding.

```bash
cd c-implementation

# Build
mkdir -p build && cd build
cmake ..
make

# Run exporter
./sav_exporter
```

See `c-implementation/README.md` for details.

## 📊 Three Demo Scenarios

### 1. Spoofing Attack Detection
- **Macro**: Time-series showing attack timeline (12 data points)
- **Micro**: Peak moment forensic detail with matched rules
- **Value**: Trend analysis + rule-level incident response

### 2. Multi-Interface Distribution  
- **Macro**: Per-interface traffic distribution (5 interfaces)
- **Micro**: Detailed rule configuration for hotspots
- **Value**: Spatial analysis + configuration audit

### 3. Policy Action Effectiveness
- **Macro**: Action distribution (Discard 84%, Rate-limit 13%, etc.)
- **Micro**: Trigger details for each policy type
- **Value**: Quantify enforcement + optimization insights

## 📜 RFC Compliance

- ✅ **RFC 7011**: IPFIX Protocol Specification
- ✅ **RFC 6313**: SubTemplateList (manual binary encoding)
- ✅ **draft-cao-opsawg-ipfix-sav-01**: SAV Information Elements
  - `savRuleType`: Allowlist(0), Blocklist(1)
  - `savTargetType`: InterfaceBased(0), PrefixBased(1)  
  - `savPolicyAction`: Permit(0), Discard(1), RateLimit(2), Redirect(3)
  - `savMatchedContentList`: SubTemplateList with rules

## 📁 Project Structure

```
sav-ipfix-validator/
├── go-implementation/          # Go implementation (production-ready)
│   ├── cmd/                    # Command-line tools
│   │   ├── exporter/          # IPFIX data generator
│   │   ├── collector/         # Console IPFIX reader
│   │   └── collector_json/    # IPFIX to JSON converter
│   ├── pkg/sav/               # Core library
│   │   ├── constants.go       # SAV IE definitions
│   │   ├── scenarios.go       # Demo scenarios
│   │   ├── writer.go          # IPFIX encoder
│   │   └── reader.go          # IPFIX decoder
│   ├── web/                   # Visualization dashboard
│   │   ├── index.html         # Interactive dashboard
│   │   └── data.json          # IPFIX data (generated)
│   ├── data/                  # Generated IPFIX files
│   └── README.md              # Go implementation guide
│
├── c-implementation/           # C implementation (research)
│   ├── src/                   # Source files
│   ├── include/               # Headers
│   ├── tests/                 # Test programs
│   └── README.md              # C implementation guide
│
├── docs/                      # Technical documentation
├── research/                  # Research artifacts
└── README.md                  # This file
```

## 🛠️ Development

See `HACKATHON_PLAN.md` for complete implementation details.

**Key Technical Decisions**:
- Pure binary IPFIX encoding (no high-level library APIs)
- Direct RFC 6313 SubTemplateList implementation
- Avoids libfixbuf's complexity and template lookup issues

See `docs/STL_IMPLEMENTATION_COMPARISON.md` for technical rationale.

## 📖 Documentation

- `HACKATHON_PLAN.md` - Complete hackathon implementation plan
- `docs/STL_IMPLEMENTATION_COMPARISON.md` - Why manual encoding works better
- `docs/SCTP_SUPPORT.md` - Future network transport implementation
- `GO_IMPLEMENTATION_TODO.md` - 9-phase roadmap for full implementation
- `REVIEW_REPORT.md` - Semantic correctness validation

## 🔗 References

- [draft-cao-opsawg-ipfix-sav-01](./draft-01-20251102.md)
- [RFC 7011 - IPFIX Protocol](https://www.rfc-editor.org/rfc/rfc7011)
- [RFC 6313 - Export of Structured Data](https://www.rfc-editor.org/rfc/rfc6313)

## 📝 License

Copyright © 2025 Cq-zgclab
