#!/usr/bin/env bash
# test_api.sh - JSON API test suite for local_api
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"
OUT_URL="${BASE_URL}/out"
IN_URL="${BASE_URL}/in"

PASS=0
FAIL=0
TOTAL=0

check() {
    TOTAL=$((TOTAL + 1))
    if [ "$2" = "1" ]; then
        PASS=$((PASS + 1))
        echo "  ✓ PASS  $1"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ FAIL  $1${3:+ — $3}"
    fi
}

post_json() {
    HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" \
        -X POST "$1" \
        -H "Content-Type: application/json" \
        -d "$2")
    BODY=$(cat /tmp/api_body)
}

echo "==================================================="
echo "  local_api JSON API Test Suite"
echo "  Target: ${BASE_URL}"
echo "==================================================="
echo ""

# ── POST /out — action dispatch ─────────────────────────────
echo "[1/8] POST /out — list_items"
post_json "$OUT_URL" '{"action":"list_items"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains items" "$(echo "$BODY" | grep -q '\"items\"' && echo 1 || echo 0)"

echo "[2/8] POST /out — create_item"
post_json "$OUT_URL" '{"action":"create_item"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains created:true" "$(echo "$BODY" | grep -q '\"created\":true' && echo 1 || echo 0)"

echo "[3/8] POST /out — update_item"
post_json "$OUT_URL" '{"action":"update_item"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains updated:true" "$(echo "$BODY" | grep -q '\"updated\":true' && echo 1 || echo 0)"

echo "[4/8] POST /out — delete_item"
post_json "$OUT_URL" '{"action":"delete_item"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains deleted:true" "$(echo "$BODY" | grep -q '\"deleted\":true' && echo 1 || echo 0)"

# ── POST /in — utility dispatch ─────────────────────────────
echo "[5/8] POST /in — ping"
post_json "$IN_URL" '{"command":"ping"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains pong" "$(echo "$BODY" | grep -q '\"pong\"' && echo 1 || echo 0)"

echo "[6/8] POST /in — time"
post_json "$IN_URL" '{"command":"time"}'
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains timestamp" "$(echo "$BODY" | grep -q '\"timestamp\"' && echo 1 || echo 0)"

# ── Error paths ─────────────────────────────────────────────
echo "[7/8] Error: unknown action"
post_json "$OUT_URL" '{"action":"foobar"}'
check "HTTP 400" "$([ "${HTTP_CODE}" = "400" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains error" "$(echo "$BODY" | grep -q '\"error\"' && echo 1 || echo 0)"

echo "[8/8] Error: missing command"
post_json "$IN_URL" '{}'
check "HTTP 400" "$([ "${HTTP_CODE}" = "400" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains error" "$(echo "$BODY" | grep -q '\"error\"' && echo 1 || echo 0)"

echo ""
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

[ "${FAIL}" -gt 0 ] && exit 1
exit 0
