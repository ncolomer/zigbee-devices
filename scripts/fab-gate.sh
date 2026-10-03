#!/usr/bin/env bash
# Fab export gate: refill all zones, save the board, then run DRC (errors only).
# Usage: fab-gate.sh <board.kicad_pcb>, or as a Claude Code PreToolUse hook (tool input JSON on stdin).
# Exit 2 blocks the hook and sends stderr back to the agent.
set -u

board="${1:-}"
if [ -z "$board" ] && [ ! -t 0 ]; then
  # the board path sits under a different key depending on the Konnect tool
  board=$(jq -r '[.tool_input | .. | strings | select(endswith(".kicad_pcb"))][0] // empty' 2>/dev/null)
fi

fail() { echo "fab-gate: $*" >&2; exit 2; }

[ -n "$board" ] && [ -f "$board" ] || fail "no board file to check (got '${board}')"

# --save-board rewrites the file, which would clobber or be clobbered by an open editor
if pgrep -x kicad >/dev/null || pgrep -x pcbnew >/dev/null; then
  fail "KiCad is open; close it first, the gate saves the refilled board"
fi

report=$(mktemp)
trap 'rm -f "$report"' EXIT
if ! kicad-cli pcb drc --refill-zones --save-board --severity-error --exit-code-violations -o "$report" "$board" >/dev/null 2>&1; then
  cat "$report" >&2 2>/dev/null
  fail "DRC failed after refilling zones; fix it before exporting fab files ($board)"
fi
echo "fab-gate: zones refilled and saved, DRC clean ($board)"
