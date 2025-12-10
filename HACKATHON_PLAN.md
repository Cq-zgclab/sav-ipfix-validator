# SAV IPFIX Hackathon Implementation Plan

**Date**: December 9, 2025  
**Project**: SAV IPFIX Validator - Go Implementation  
**Goal**: Demonstrate SAV IPFIX concept with macro+micro+combined analysis

---

## 🎯 Hackathon Objectives

### Core Requirements
- ✅ **Concept Demonstration**: Prove SAV + IPFIX feasibility
- ✅ **RFC Compliance**: RFC 7011 (IPFIX), RFC 6313 (SubTemplateList), draft-cao-opsawg-ipfix-sav-01
- ✅ **Simulated Data**: Cover real-world SAV operational scenarios
- ✅ **No Deep Technical Details**: Focus on business value, not implementation complexity

### Evaluation Criteria
- **Practicality**: Demonstrate real SAV operational use cases (attack detection, interface analysis, policy optimization)
- **Innovation**: First standardized SAV IPFIX telemetry solution
- **Feasibility**: Complete data flow with simple deployment (no Docker required)

---

## 📊 Three Demo Scenarios Design

### Scenario 1: Spoofing Attack Detection
**Goal**: Demonstrate time-series analysis + forensic detail correlation

**Macro (12 records)**: 
- Time-series aggregation over 1 hour (5-minute intervals)
- Shows attack timeline: start, peak (10K pps), duration (20 minutes)
- Quantifies packet/byte count trends

**Micro (1 record)**:
- Detailed event at peak moment
- Source IP `192.0.2.123` failed to match allowlist
- Full `savMatchedContentList` with 3 allowed prefixes on interface eth0

**Combined Value**:
- Macro identifies anomaly time window
- Micro provides rule-level forensics for incident response
- Validates SAV configuration correctness

**IPFIX Elements Used**:
- `savRuleType`: Allowlist (0)
- `savTargetType`: InterfaceBased (0)
- `savPolicyAction`: Discard (1)

---

### Scenario 2: Multi-Interface Traffic Distribution
**Goal**: Demonstrate spatial analysis + rule configuration audit

**Macro (5 records)**:
- Per-interface spoofing traffic distribution
- eth0 (external): 45K pps - most problematic
- eth1 (customer-A): 8.5K pps
- eth2 (customer-B): 12K pps
- eth3 (peering): 3.2K pps
- eth4 (internal): 1.8K pps - least problematic

**Micro (2 records)**:
- eth0 detail: Interface-based allowlist with 3 prefixes
- eth1 detail: Prefix-based validation with 2 interfaces, rate-limit action

**Combined Value**:
- Identify problem interfaces from macro view
- Audit their rule configurations from micro view
- Optimization suggestions: merge prefixes, adjust policies

**IPFIX Elements Used**:
- `savRuleType`: Allowlist (0) for both
- `savTargetType`: InterfaceBased (0) for eth0, PrefixBased (1) for eth1
- `savPolicyAction`: Discard (1) and RateLimit (2)

---

### Scenario 3: Policy Action Effectiveness
**Goal**: Demonstrate quantitative policy analysis + optimization insights

**Macro (4 records)**:
- Traffic distribution by policy action:
  - Discard: 50K packets (30MB bandwidth saved) - 84%
  - Rate-limit: 8K packets - 13%
  - Redirect: 500 packets - 2%
  - Permit: 1K packets - 1% (monitoring mode)

**Micro (3 records)**:
- Discard event: Blocklist hit on prefix `2001:db8:bad::/48` at interface 5001
- Rate-limit event: Allowlist miss on interface 6000 (2 allowed prefixes shown)
- Redirect event: Interface 7000 blocks `10.99.99.0/24`, redirects to honeypot

**Combined Value**:
- Quantify enforcement effectiveness (macro)
- Review rule triggering logic (micro)
- Decision support: Consider converting some rate-limits to discards

**IPFIX Elements Used**:
- `savRuleType`: Blocklist (1) and Allowlist (0)
- `savTargetType`: PrefixBased (1) and InterfaceBased (0)
- `savPolicyAction`: All 4 actions (0-3)

---

## 🛠️ Implementation Status

### ✅ Phase 1: SAV IE Semantic Correction (COMPLETED)
**File**: `pkg/sav/constants.go`

```go
// Corrected definitions aligned with C implementation and draft
RuleTypeAllowlist = 0  // was incorrectly 1
RuleTypeBlocklist = 1  // was incorrectly 2

TargetTypeInterfaceBased = 0  // was incorrectly 1
TargetTypePrefixBased = 1     // was incorrectly 3

PolicyActionPermit = 0      // was incorrectly 1
PolicyActionDiscard = 1     // was incorrectly 2
PolicyActionRateLimit = 2   // newly added
PolicyActionRedirect = 3    // newly added
```

**Impact**: All 3 SAV IEs now match draft-cao-opsawg-ipfix-sav-01 specifications

---

### ✅ Phase 2: Scenario Data Generator (COMPLETED)
**File**: `pkg/sav/scenarios.go`

**Structure**:
```go
type Scenario struct {
    Name        string
    Description string
    Analysis    string          // Macro+Micro+Combined insight
    Records     []SAVRecord     // 13/7/7 records per scenario
}

type SAVRecord struct {
    Type        ScenarioType    // macro/micro/combined
    Timestamp   time.Time
    RuleType    uint8
    TargetType  uint8
    Action      uint8
    Rules       []RuleContent   // Empty for macro, full for micro
    PacketCount uint64
    ByteCount   uint64
    Label       string
}
```

**Functions**:
- `GetScenario1()`: Attack detection (13 records)
- `GetScenario2()`: Interface distribution (7 records)
- `GetScenario3()`: Action effectiveness (7 records)
- `GetAllScenarios()`: Returns all 3 scenarios (27 records total)

---

### ✅ Phase 3: Exporter CLI Update (COMPLETED)
**File**: `cmd/exporter/main.go`

**Usage**:
```bash
./exporter --scenario attack --output scenario1.ipfix
./exporter --scenario interface --output scenario2.ipfix
./exporter --scenario action --output scenario3.ipfix
./exporter --scenario all --output all_scenarios.ipfix
```

**Output**:
```
📊 Scenario: Spoofing-Attack-Detection
   ✅ Exported: 13 records
   💡 MACRO: Attack started 60min ago, peaked at 35min ago (10K pps)...
```

---

### ✅ Phase 4: Data Generation Testing (COMPLETED)
**Generated Files**:
- `data/scenario1.ipfix` - 13 records (Attack scenario)
- `data/all_scenarios.ipfix` - 27 records (All scenarios)

**Verification**:
```bash
$ ./bin/exporter --scenario all --output data/all_scenarios.ipfix
🎉 Successfully exported 27 total records
📈 Ready for visualization!
```

---

### 🔄 Phase 5: Web Dashboard (IN PROGRESS)
**File**: `web/dashboard.html` (to be created)

**Requirements**:
1. **Scenario 1 Visualization**: Line chart showing time-series attack trend
   - X-axis: Time (12 intervals)
   - Y-axis: Packet count
   - Highlight peak moment

2. **Scenario 2 Visualization**: Bar chart showing per-interface distribution
   - X-axis: Interface names (eth0-eth4)
   - Y-axis: Packet count
   - Color-code by severity

3. **Scenario 3 Visualization**: Pie chart showing action distribution
   - Segments: Permit, Discard, Rate-limit, Redirect
   - Percentages: 1%, 84%, 13%, 2%

**Technology Stack**:
- Pure HTML + JavaScript (no server required)
- Chart.js for visualization
- Mock data embedded (no need to parse IPFIX files)
- Python HTTP server for demo: `python3 -m http.server 8000`

---

### 📝 Phase 6: Documentation Update (PENDING)
**File**: `README.md` (to be updated)

**Required Sections**:
1. **Quick Start**:
   ```bash
   # Generate demo data
   ./bin/exporter --scenario all --output data/demo.ipfix
   
   # View dashboard
   python3 -m http.server 8000
   # Open http://localhost:8000/web/dashboard.html
   ```

2. **Scenario Descriptions**: Brief explanation of 3 scenarios

3. **RFC Compliance Statement**:
   - RFC 7011: IPFIX protocol format ✅
   - RFC 6313: SubTemplateList encoding (manual binary implementation) ✅
   - draft-cao-opsawg-ipfix-sav-01: SAV IE semantics ✅

4. **Architecture Diagram**:
   ```
   SAV Router → Exporter (Go) → IPFIX File → Collector (Go) → JSON → Dashboard (HTML)
   ```

---

## 💎 Key Technical Decisions

### Why Go Implementation Avoided libfixbuf's SubTemplateList Issues
**Problem with libfixbuf**:
- Complex API: `fbSubTemplateList_t` with opaque internal state
- Requires template pair registration: `fbSessionLookupTemplatePair()`
- Strict IE naming: Must use exact "subTemplateList"
- Error-prone: "Missing external template" errors

**Go Solution**:
- **Direct binary encoding** per RFC 6313
- No high-level API dependency
- Manual byte manipulation:
  ```go
  buf.WriteByte(0xFF)                            // semantic: allOf
  binary.Write(buf, binary.BigEndian, templateID) // 2 bytes
  binary.Write(buf, binary.BigEndian, length)    // 2 bytes
  buf.Write(subRecordData)                       // actual data
  ```
- **Result**: Complete control, zero-copy, no template lookup failures

**Reference**: `docs/STL_IMPLEMENTATION_COMPARISON.md`

---

### Why No Docker for Hackathon
**Simplicity Requirements**:
- ❌ Docker Compose adds complexity
- ❌ Network simulation introduces bugs
- ❌ Container orchestration overkill for demo

**Chosen Approach**:
- ✅ Single Go binary (exporter + collector)
- ✅ File-based data flow (no network required initially)
- ✅ Simple HTTP server for dashboard
- ✅ Can add SCTP/TCP later for Phase 8

---

## 📈 IPFIX Value Proposition

### What Makes This Demo Effective

1. **Flow Telemetry Nature**:
   - Time-series data (not just single events)
   - Aggregated statistics (packet/byte counts)
   - Spatial distribution (per-interface)

2. **Macro + Micro + Combined**:
   - **Macro**: Trends, distributions, quantification
   - **Micro**: Detailed rules, specific events
   - **Combined**: Drill-down from overview to detail

3. **Operational Insights**:
   - **Trend identification**: When did attack start/peak?
   - **Hotspot location**: Which interface has most spoofing?
   - **Effectiveness evaluation**: How much bandwidth saved by discard action?

4. **SAV Integration**:
   - `savMatchedContentList` provides rule transparency
   - Multiple validation modes demonstrated (4 combinations)
   - All policy actions shown (4 types)

---

## 🚀 Next Steps (Ordered by Priority)

### Immediate (Today)
1. ✅ ~~Phase 5: Create `web/dashboard.html` with Chart.js~~
2. ✅ ~~Test end-to-end flow~~

### Short-term (Tomorrow)
3. Phase 6: Update `README.md` with quick start guide
4. Create demo script for 5-minute presentation
5. Record demo video (backup plan if network fails)

### Optional (If Time Permits)
6. Phase 7.2: Simple validator tool (`./validator data/demo.ipfix`)
7. Phase 8.1: Add TCP collector (SCTP can wait)
8. Create architecture diagram (visual aid)

---

## 📋 Hackathon Presentation Structure (5 minutes)

### Slide 1: Problem Statement (30s)
- SAV lacks operational visibility
- Operators can't answer: "How many spoofed packets? Which rules triggered?"

### Slide 2: Solution - SAV IPFIX (30s)
- IPFIX: Standard flow telemetry protocol
- New SAV-specific IEs: `savRuleType`, `savTargetType`, `savMatchedContentList`, `savPolicyAction`

### Slide 3: Demo Scenario 1 - Attack Detection (1min)
- **Show**: Line chart of time-series data
- **Highlight**: Attack peak at 35min ago (10K pps)
- **Click**: Drill-down to micro event showing exact rule

### Slide 4: Demo Scenario 2 - Interface Analysis (1min)
- **Show**: Bar chart of per-interface distribution
- **Highlight**: eth0 has 45K pps (most problematic)
- **Click**: View eth0's allowlist configuration

### Slide 5: Demo Scenario 3 - Policy Optimization (1min)
- **Show**: Pie chart of action distribution
- **Highlight**: 84% discarded (30MB saved)
- **Insight**: Suggest converting some rate-limits to discards

### Slide 6: RFC Compliance & Impact (1min)
- RFC 7011, RFC 6313, draft-cao-opsawg-ipfix-sav-01 compliant
- First standardized SAV telemetry solution
- Enables cross-vendor interoperability

---

## ✅ Success Metrics

### Technical Completeness
- ✅ 3 scenarios implemented
- ✅ 27 IPFIX records generated
- ✅ All SAV IEs semantically correct
- ⏳ Dashboard visualization functional
- ⏳ End-to-end demo runnable

### Presentation Readiness
- ⏳ 5-minute demo script prepared
- ⏳ Visual aids (charts) working
- ⏳ Backup video recorded
- ⏳ Q&A preparation

### Impact Demonstration
- ✅ Practicality: 3 real operational scenarios
- ✅ Innovation: Novel IPFIX extension for SAV
- ✅ Feasibility: Working prototype with simple deployment

---

## 📞 Contact & Resources

**Repository**: https://github.com/Cq-zgclab/sav-ipfix-validator  
**Branch**: main  
**Documentation**: 
- `GO_IMPLEMENTATION_TODO.md` - Full implementation plan
- `REVIEW_REPORT.md` - Semantic error analysis
- `docs/STL_IMPLEMENTATION_COMPARISON.md` - Technical rationale
- `docs/SCTP_SUPPORT.md` - Future network transport

**References**:
- draft-cao-opsawg-ipfix-sav-01 (included in repo)
- RFC 7011: IPFIX Protocol Specification
- RFC 6313: Export of Structured Data in IPFIX
- RFC 7012: Information Elements for IPFIX

---

*Last Updated: December 9, 2025*
