# ✅ SAV IPFIX Validator

Source Address Validation (SAV) telemetry using the IPFIX protocol.

**C reference implementation for IETF Hackathon demonstration**

This project demonstrates how SAV Information Elements can be exported using structured IPFIX (RFC 6313 SubTemplateList) with clear operational and security narratives.

---

# 🚀 Quick Start (Hackathon Demo Entry)

```bash
./demo/run_demo.sh
````

Default mode exports:

* **Template A (Operations Monitoring View)**
* **Template B (Incident Drill-down View)**

To run Template A only:

```bash
./demo/run_demo.sh template-a
```

The script:

* Builds the C implementation
* Executes end-to-end exporter
* Generates `test_sav_e2e.ipfix`
* Decodes output using `ipfixDump --rfc5610`
* Labels Flow Keys vs Attributes for readability

This is the **authoritative demonstration path**.

---

# 📊 Demo Storyline (Template A / B)

The hackathon demo is structured around two complementary views.

---

## Template A — Operations Monitoring View (500 / 501)

Focus:

> “Which interface is under attack? Which SAV policy is active?”

### Flow Keys

* `ingressInterface`
* `sourceIPv4Prefix` / `sourceIPv6Prefix`
* `savRuleType`
* `savTargetType`
* `savMatchedContentList` (SubTemplateList)

### Non-Key Attributes

* `packetDeltaCount`
* `octetDeltaCount`
* `flowStartMilliseconds`
* `flowEndMilliseconds`
* `savPolicyAction`

Purpose:

* Aggregated operational visibility
* Interface-level pressure analysis
* Rule-type distribution insight

This is the **default monitoring mode**.

---

## Template B — Incident Drill-down View (502 / 503)

Focus:

> “Investigate a specific attack flow and understand why SAV triggered.”

### Flow Keys

* 5-tuple (`source/destination IP`, `ports`, `protocol`)
* `ingressInterface`
* `savRuleType`
* `savTargetType`
* `savMatchedContentList`

### Non-Key Attributes

* `packetDeltaCount`
* `flowStartMilliseconds`
* `savPolicyAction`

Purpose:

* Forensic inspection
* Rule-trigger explanation
* Security investigation

Template B is intended for **short-term investigation**, not steady-state export.

---

# 📜 SAV Structured Encoding 

The core semantic component is:

```
savMatchedContentList
```

Encoded using **SubTemplateList (STL)** (RFC 6313).

## SubTemplate IDs (900–903)

| ID  | Semantics               |
| --- | ----------------------- |
| 900 | IPv4 Interface → Prefix |
| 901 | IPv6 Interface → Prefix |
| 902 | IPv4 Prefix → Interface |
| 903 | IPv6 Prefix → Interface |

Strong constraints enforced in implementation:

* allowlist ⇒ list length ≥ 1 (allOf semantics)
* blocklist ⇒ list length = 1 (exactlyOneOf semantics)
* A single Data Record never mixes validation modes
* `subTemplateID` is consistent within each record

---

# 📐 RFC Alignment

* RFC 7011 — IPFIX Protocol
* RFC 6313 — Structured Data Export
* draft-cao-opsawg-ipfix-sav-01 — SAV Information Elements

Enterprise IEs implemented:

* `savRuleType`
* `savTargetType`
* `savPolicyAction`
* `savMatchedContentList`

---

# 🏗 Implementation Architecture

```
Spoofed Packet Generator
        ↓
Exporter-side Aggregation
        ↓
Template A / Template B
        ↓
IPFIX Export (libfixbuf)
        ↓
ipfixDump (RFC5610 decode)
```

Implementation located in:

```
c-implementation/
```

Build manually:

```bash
cd c-implementation
make tests
./build/bin/sav_e2e_demo
```

However, for hackathon demonstration, always use:

```bash
./demo/run_demo.sh
```

---

# 📁 Project Structure

```
sav-ipfix-validator/

├── demo/                     # Hackathon demo entry
│   └── run_demo.sh
│
├── c-implementation/         # C implementation (libfixbuf-based)
│   ├── src/
│   ├── include/
│   ├── tests/
│   └── README.md
│
├── docs/
│   └── internal/             # Historical validation architectures
│
└── README.md
```

---

# 🧪 Internal Validation Templates (Historical)

Earlier versions of this project included experimental exporter-side aggregation models (T1/T2/T3 templates, IDs 400–440).

These are preserved under:

```
docs/internal/
```

They are retained for research traceability and validation history but are **not part of the hackathon demonstration model**.

The authoritative template family for the current design is:

```
Template A / Template B (500–503)
```

---

# 💾 T1 / T2 / T3 Template Overview (Research Only)

For completeness, the internal research templates were defined as:

| Template | Purpose                   | Flow Key                                                                          | Non-Key Attributes                                                                                             | SubTemplate IDs |
| -------- | ------------------------- | --------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | --------------- |
| T1       | Rule-Outcome (finest)     | ingressInterface + sourceIP + savRuleType + savTargetType + savMatchedContentList | packetDeltaCount + octetDeltaCount + flowStart/EndMilliseconds + savPolicyAction + observationTimeMilliseconds | 900–903         |
| T2       | Interface View            | ingressInterface + savRuleType + savTargetType + savMatchedContentList            | packetDeltaCount + octetDeltaCount + flowStart/EndMilliseconds + savPolicyAction                               | 900–903         |
| T3       | Prefix/Mode (Situational) | sourcePrefix + savRuleType + savTargetType + savMatchedContentList                | packetDeltaCount + octetDeltaCount + flowStart/EndMilliseconds + savPolicyAction                               | 900–903         |

These templates are **mutually independent** and only used for internal testing, not for hackathon demo.

---

# 🛠 Requirements

* libfixbuf (with `ipfixDump` in PATH)
* make
* gcc/clang

---

# 🔗 References

* [draft-cao-opsawg-ipfix-sav-01](./draft-01-20251102.md)
* [draft-ietf-savnet-general-sav-capabilities-02](https://datatracker.ietf.org/doc/draft-ietf-savnet-general-sav-capabilities/)
* [RFC 7011 — IPFIX Protocol](https://datatracker.ietf.org/doc/rfc7011/
* [RFC 6313 — Export of Structured Data](https://datatracker.ietf.org/doc/rfc6131/)

---

# 📝 License

Copyright © 2025 Cq-zgclab

