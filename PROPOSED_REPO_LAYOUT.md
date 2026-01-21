# Proposed Repo Layout (Demo-first, Non-destructive)

This document proposes a **display-friendly** repository organization for the hackathon demo.

Constraints (locked facts)
- The **only** hackathon demo entrypoint is: `demo/run_demo.sh`
- That script calls the **only** binary: `c-implementation/build/bin/test_test_sav_e2e`
- Judges only need to understand and run the path above

No file deletions, no assumptions about “unused” history, and **no code/logic changes** are proposed here.

## 5-minute judge path (what must be obvious)
1) Run the demo:
   - `./demo/run_demo.sh` (Template A + Template B)
   - `./demo/run_demo.sh template-a` (Template A only)
2) Read output: the script prints labeled `ipfixDump` output.

## What is demo-critical (must stay prominent)
**Keep at repo root (or clearly highlighted in root README):**
- `demo/`  
  - `demo/run_demo.sh` (the entrypoint)
  - `demo/README.md` (how to run; mode semantics)
- `c-implementation/`  
  - build + runtime dependencies for `test_test_sav_e2e`
  - the generated `test_sav_e2e.ipfix` is produced under this directory
- `README.md` (should point to `demo/run_demo.sh` as the primary entry)

## What is supporting material (background / history / research)
These items are valuable context but are not required for judges to run the demo:
- `docs/` (spec discussion, compliance notes, reports)
- `research/` (research notes, exploration artifacts)
- `c-hackathon-sav/` (historical hackathon work / alternative prototypes)
- Top-level HTML reports/pages:
  - `sav-dynamic-viewer.html`
  - `sav-hackathon-report.html`
  - `sav-standalone.html`
- `data.json` (supporting data / artifacts)

## Proposed high-level structure
Add one folder at the repo root:
- `archive/` (or `experiments/`)

Then move **entire directories** into it (proposal only; do not execute in this step):
- Move into `archive/` (proposal):
  - `c-hackathon-sav/` → `archive/c-hackathon-sav/`
  - `research/` → `archive/research/`
  - (optional) `docs/` → `archive/docs/`  *(only if you want an ultra-minimal root; otherwise keep `docs/` at root)*

Then move **top-level standalone artifacts** into it (proposal only; do not execute here):
- `sav-dynamic-viewer.html` → `archive/sav-dynamic-viewer.html`
- `sav-hackathon-report.html` → `archive/sav-hackathon-report.html`
- `sav-standalone.html` → `archive/sav-standalone.html`
- `data.json` → `archive/data.json`

## Resulting “judge-friendly” top level (target state)
Top-level should look like this:
- `README.md` (points to the demo entry in the first screen)
- `demo/` (the only thing judges need to run)
- `c-implementation/` (build + the single binary used by the demo)
- `archive/` (everything else: history, research, reports, optional docs)

## Naming guidance (to reduce confusion)
- Prefer the folder name `archive/` if the goal is “not required for running the demo”.
- Prefer `experiments/` if the goal is “alternative approaches / prototypes”.

## Minimal root README snippet (recommendation)
(Do not apply automatically here.)
- “Run: `./demo/run_demo.sh`”
- “Optional: `./demo/run_demo.sh template-a`”
- “That’s it.”
