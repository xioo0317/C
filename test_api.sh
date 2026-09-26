#!/usr/bin/env bash
# test_api.sh - API test suite for local_api (root detection)
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"
DETECT_URL="${BASE_URL}/api/v1/detect"
RESULT_FILE="/data/local/tmp/coverRoot/root_detect.json"

PASS=0
FAIL=0
TOTAL=0

check() {
    TOTAL=$((TOTAL + 1))
    if [ "$2" = "1" ]; then
        PASS=$((PASS + 1))
        echo "  [PASS] $1"
    else
        FAIL=$((FAIL + 1))
        echo "  [FAIL] $1${3:+ — $3}"
    fi
}

echo "==================================================="
echo "  local_api Root-Detection Test Suite"
echo "  Target: ${BASE_URL}"
echo "==================================================="
echo ""

# ── POST /api/v1/detect ────────────────────────────────────
echo "[1/4] POST /api/v1/detect"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST "$DETECT_URL")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "contains status:ok" "$(echo "$BODY" | grep -q '"status":"ok"' && echo 1 || echo 0)"
check "contains result_file" "$(echo "$BODY" | grep -q '"result_file"' && echo 1 || echo 0)"

echo "[2/4] Result JSON file written"
check "file exists: ${RESULT_FILE}" "$([ -f "$RESULT_FILE" ] && echo 1 || echo 0)"
if [ -f "$RESULT_FILE" ]; then
    check "valid JSON" "$(python3 -c "import json,sys;json.load(open('${RESULT_FILE}'))" 2>/dev/null && echo 1 || echo 0)"
    check "has detected field" "$(grep -q '"detected"' "$RESULT_FILE" && echo 1 || echo 0)"
fi

# ── 404 path ───────────────────────────────────────────────
echo "[3/4] Unknown path returns JSON 404"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" "${BASE_URL}/nope")
check "HTTP 404" "$([ "${HTTP_CODE}" = "404" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "JSON error body" "$(grep -q '"error"' /tmp/api_body && echo 1 || echo 0)"

echo "[4/4] Server stays silent (no special terminal output to assert here)"
check "endpoint reachable" "$([ "${HTTP_CODE}" = "404" ] && echo 1 || echo 0)"

echo ""
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

[ "${FAIL}" -gt 0 ] && exit 1
exit 0
