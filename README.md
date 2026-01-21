# SAV IPFIX Validator

Source Address Validation (SAV) telemetry using IPFIX protocol.

**C 参考实现（hackathon 使用）**
- 🔧 **C Implementation** - libfixbuf-based exporter/collector prototype


## 🔧 C Implementation (Research)

Using libfixbuf for IPFIX encoding/decoding.

```bash
cd c-implementation

# Build + run E2E exporter
make clean
make tests
./build/bin/test_test_sav_e2e
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

实现口径与可复现路径以 C 实现的新架构为准：

- `c-implementation/T1_T2_T3_ARCHITECTURE.md`
- `docs/ENGINEERING_GUIDE.md`

**Key Technical Decisions**:
- Uses libfixbuf for IPFIX encoding/decoding
- Uses RFC 6313 SubTemplateList (STL)
- Requires explicit struct/template alignment (incl. `paddingOctets`)

## 📖 Documentation

- `c-implementation/T1_T2_T3_ARCHITECTURE.md` - 新架构说明（权威）
- `docs/ENGINEERING_GUIDE.md` - 工程实施规范/踩坑 checklist/验收标准（原文汇编）


## 🔗 References

- [draft-cao-opsawg-ipfix-sav-01](./draft-01-20251102.md)
- [RFC 7011 - IPFIX Protocol](https://www.rfc-editor.org/rfc/rfc7011)
- [RFC 6313 - Export of Structured Data](https://www.rfc-editor.org/rfc/rfc6313)

## 📝 License

Copyright © 2025 Cq-zgclab
