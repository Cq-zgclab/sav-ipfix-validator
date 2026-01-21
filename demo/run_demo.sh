#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  ./demo/run_demo.sh            # Template A + B
  ./demo/run_demo.sh template-a # Template A only

For judges:
- Default mode (Template A + Template B): operations monitoring view + security drill-down view.
- template-a mode (Template A only): daily operations monitoring view.
USAGE
}

mode="${1:-}";
case "$mode" in
  "" )
    enable_b=1
    ;;
  template-a )
    enable_b=0
    ;;
  -h|--help|help )
    usage
    exit 0
    ;;
  * )
    echo "ERROR: unknown mode: $mode" >&2
    usage >&2
    exit 2
    ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c_impl="$repo_root/c-implementation"

if ! command -v make >/dev/null 2>&1; then
  echo "ERROR: make not found" >&2
  exit 1
fi
if ! command -v ipfixDump >/dev/null 2>&1; then
  echo "ERROR: ipfixDump not found in PATH" >&2
  echo "Hint: ipfixDump is provided by libfixbuf tools." >&2
  exit 1
fi

if [[ ! -d "$c_impl" ]]; then
  echo "ERROR: missing directory: $c_impl" >&2
  exit 1
fi

pushd "$c_impl" >/dev/null

make tests -j

if [[ ! -x ./build/bin/sav_e2e_demo ]]; then
  echo "ERROR: missing executable: $c_impl/build/bin/sav_e2e_demo" >&2
  popd >/dev/null
  exit 1
fi

export SAV_ENABLE_TEMPLATE_B="$enable_b"
export SAV_DEMO_TEMPLATE_A_CROSSPRODUCT=0
export SAV_EXPORT_T123=0

./build/bin/sav_e2e_demo | sed -E \
  -e 's/^\[OK\] Exported story templates \(A\/B\) to (.+)$/✅ End-to-End Export Succeeded: Template A\/B flows written to \1/' \
  -e 's/^✅ E2E EXPORT PASSED$/✅ End-to-End Test Passed: All templates exported successfully/'

out_file="test_sav_e2e.ipfix"
if [[ ! -f "$out_file" ]]; then
  echo "ERROR: expected output file not found: $c_impl/$out_file" >&2
  popd >/dev/null
  exit 1
fi


# Template Records + Data Records (RFC5610-friendly output)
# Label each data-record field with a fixed tag:
# - Template A Flow Keys: ingressInterface, savRuleType, savTargetType, savMatchedContent, sourceIPv4Prefix
# - Template B Flow Keys: sourceIPv4Address, destinationIPv4Address, sourceTransportPort, destinationTransportPort, protocolIdentifier, ingressInterface
# - Attributes: everything else
ipfixDump --in "$out_file" --rfc5610 | awk '
  function is_key_A(name, pseudo) {
    return (name=="ingressInterface" || name=="sourceIPv4Prefix" || name=="subTemplateList" || pseudo=="savRuleType" || pseudo=="savTargetType");
  }
  function is_key_B(name, pseudo) {
    return (name=="sourceIPv4Address" || name=="destinationIPv4Address" || name=="sourceTransportPort" || name=="destinationTransportPort" || name=="protocolIdentifier" || name=="ingressInterface");
  }
  function current_class(tmpl) {
    if (tmpl==500 || tmpl==501) return "A";
    if (tmpl==502 || tmpl==503) return "B";
    return "";
  }
  BEGIN { cur_tmpl=-1; }
  /^--- Data Record[[:space:]]+[0-9]+[[:space:]]+---/ {
    cur_tmpl=-1;
    if (match($0, /tmpl:[[:space:]]+([0-9]+)/, m)) {
      cur_tmpl=m[1]+0;
    }
    print;
    next;
  }
  {
    cls=current_class(cur_tmpl);
    # Match top-level field lines inside data records.
    # STL internal lines (prefixed with "+ ") are preserved as-is and are NOT labeled.
    if (cls != "" && match($0, /^  (\+ )?([A-Za-z0-9_]+)[[:space:]]*\([^)]*\)[[:space:]]*:[[:space:]]*(.*)$/, m)) {
      if (m[1] == "+ ") {
        print;
        next;
      }
      name=m[2];
      value=m[3];

      # Recognize SAV enterprise IEs in ipfixDump output without interpreting values.
      pseudo="";
      if (name=="_alienInformationElement") {
        if ($0 ~ /\(6871\/1\)/) pseudo="savRuleType";
        else if ($0 ~ /\(6871\/2\)/) pseudo="savTargetType";
        else if ($0 ~ /\(6871\/3\)/) pseudo="savMatchedContent";
        else if ($0 ~ /\(6871\/4\)/) pseudo="savPolicyAction";
      }

      display=name;
      if (name=="subTemplateList") display="savMatchedContent";
      if (pseudo != "") display=pseudo;

      tag="Attribute";
      if (cls=="A" && is_key_A(name, pseudo)) tag="Flow Key";
      else if (cls=="B" && is_key_B(name, pseudo)) tag="Flow Key";

      print "  [" tag "] " display ": " value;
      next;
    }
    print;
  }
'

popd >/dev/null
