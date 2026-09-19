#!/usr/bin/env bash
# test_api.sh - SSE streaming test suite for local_api
# Tests that POST /api/v1/execute returns Server-Sent Events in the format:
#   data: {"message":"开始执行指令"}
#   ...
#   data: [DONE]
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"
EXEC_URL="${BASE_URL}/api/v1/execute"

TMP_BODY="$(mktemp /tmp/sse_body.XXXXXX)"
TMP_HEAD="$(mktemp /tmp/sse_head.XXXXXX)"
trap 'rm -f "$TMP_BODY" "$TMP_HEAD"' EXIT

PASS=0
FAIL=0
TOTAL=0

check() {   # check <test_name> <0|1> [detail]
    TOTAL=$((TOTAL + 1))
    if [ "$2" = "1" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ PASS  $1"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ FAIL  $1${3:+ — $3}"
    fi
}

# fetch_sse <json-body>
# Sets: HTTP_CODE, CONTENT_TYPE; fills TMP_BODY (stream) and TMP_HEAD (headers)
fetch_sse() {
    HTTP_CODE=$(curl -sN -o "$TMP_BODY" -D "$TMP_HEAD" -w "%{http_code}" \
        -X POST "$EXEC_URL" \
        -H "Content-Type: application/json" \
        -d "$1")
    CONTENT_TYPE=$(tr -d '\r' < "$TMP_HEAD" | grep -i '^content-type:' | head -1 | cut -d' ' -f2-)
}

# assert_sse_stream <action> <result_substring>
# Runs one action and validates the whole SSE frame structure.
assert_sse_stream() {
    local action="$1" result_expect="$2"

    fetch_sse "{\"action\":\"${action}\"}"

    check "${action}: HTTP 200" \
        "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
    check "${action}: Content-Type is text/event-stream" \
        "$(echo "${CONTENT_TYPE}" | grep -q 'text/event-stream' && echo 1 || echo 0)" "got ${CONTENT_TYPE}"

    # Collect all data frames (strip the "data: " prefix)
    local frames first last n_frames
    frames=$(grep '^data: ' "$TMP_BODY" | sed 's/^data: //')
    n_frames=$(printf '%s\n' "$frames" | grep -c . || true)
    first=$(printf '%s\n' "$frames" | head -n 1)
    last=$(printf '%s\n' "$frames" | tail -n 1)

    check "${action}: got >= 3 data frames" \
        "$([ "${n_frames}" -ge 3 ] && echo 1 || echo 0)" "got ${n_frames}"
    check "${action}: first frame contains 开始执行指令" \
        "$(echo "${first}" | grep -q '开始执行指令' && echo 1 || echo 0)" "got: ${first}"
    check "${action}: last frame is [DONE]" \
        "$([ "${last}" = "[DONE]" ] && echo 1 || echo 0)" "got: ${last}"
    check "${action}: result frame contains ${result_expect}" \
        "$(printf '%s\n' "$frames" | grep -F -- "${result_expect}" | grep -q '"completed"' && echo 1 || echo 0)"
    check "${action}: start frame comes before [DONE]" \
        "$(printf '%s\n' "$frames" | grep -n '开始执行指令\|^\[DONE\]' | head -1 | grep -q '开始执行指令' && echo 1 || echo 0)"

    # Every frame except the last must be valid JSON starting with '{'
    local bad_json
    bad_json=$(printf '%s\n' "$frames" | head -n -1 | grep -cv '^{' || true)
    check "${action}: all non-final frames are JSON objects" \
        "$([ "${bad_json}" = "0" ] && echo 1 || echo 0)" "${bad_json} bad frame(s)"

    # Soft info: how many network chunks arrived (real streaming indicator)
    local chunks
    chunks=$(curl -sN --trace-ascii - -o /dev/null -X POST "$EXEC_URL" \
        -H "Content-Type: application/json" -d "{\"action\":\"${action}\"}" \
        | grep -c 'Recv data' || true)
    echo "    · info: ${n_frames} frames over ${chunks} network chunk(s)"
    echo ""
}

# assert_json_error <body> <expected_code>
# Non-stream error paths must stay plain JSON.
assert_json_error() {
    local body="$1" expected="$2"
    fetch_sse "$body"
    check "HTTP ${expected}" \
        "$([ "${HTTP_CODE}" = "${expected}" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
    check "response stays plain JSON (not SSE)" \
        "$(echo "${CONTENT_TYPE}" | grep -q 'application/json' && echo 1 || echo 0)" "got ${CONTENT_TYPE}"
    check "body contains \"error\"" \
        "$(grep -q '"error"' "$TMP_BODY" && echo 1 || echo 0)"
}

echo "==================================================="
echo "  local_api SSE Streaming Test Suite"
echo "  Target: ${BASE_URL}"
echo "==================================================="
echo ""

# ── 1. list_items ─────────────────────────────────────────
echo "[1/6] SSE stream: list_items"
assert_sse_stream "list_items" '"items":[]'

# ── 2. create_item ────────────────────────────────────────
echo "[2/6] SSE stream: create_item"
assert_sse_stream "create_item" '"created":true'

# ── 3. update_item ────────────────────────────────────────
echo "[3/6] SSE stream: update_item"
assert_sse_stream "update_item" '"updated":true'

# ── 4. delete_item ────────────────────────────────────────
echo "[4/6] SSE stream: delete_item"
assert_sse_stream "delete_item" '"deleted":true'

# ── 5. Missing action ─────────────────────────────────────
echo "[5/6] Error path: missing action"
assert_json_error '{}' 400
echo ""

# ── 6. Unknown action ─────────────────────────────────────
echo "[6/6] Error path: unknown action"
assert_json_error '{"action":"foobar"}' 400
echo ""

# ── Summary ───────────────────────────────────────────────
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

if [ "${FAIL}" -gt 0 ]; then
    exit 1
fi
exit 0
