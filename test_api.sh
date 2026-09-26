#!/usr/bin/env bash
# test_api.sh - API test suite for local_api
#
# Usage: ./test_api.sh [host] [port]
# Default: host=127.0.0.1, port=8080

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
BASE_URL="http://${HOST}:${PORT}"
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
echo "  local_api Test Suite"
echo "  Target: ${BASE_URL}"
echo "==================================================="
echo ""

# ── 1. POST / action=detect ──────────────────────────────
echo "[1/5] POST / action=detect"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"detect"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "status ok" "$(echo "$BODY" | grep -q '"status":"ok"' && echo 1 || echo 0)"
check "has result" "$(echo "$BODY" | grep -q '"result"' && echo 1 || echo 0)"
check "has detected" "$(echo "$BODY" | grep -q '"detected"' && echo 1 || echo 0)"

# ── 2. POST / action=version ──────────────────────────────
echo "[2/5] POST / action=version"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"version"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has server_version" "$(echo "$BODY" | grep -q '"server_version"' && echo 1 || echo 0)"

# ── 3. POST / action=debug ──────────────────────────────
echo "[3/5] POST / action=debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"debug"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tools" "$(echo "$BODY" | grep -q '"tools"' && echo 1 || echo 0)"
check "has detect_dry_run" "$(echo "$BODY" | grep -q '"detect_dry_run"' && echo 1 || echo 0)"

# ── 4. GET /debug ────────────────────────────────────────
echo "[4/5] GET /debug"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" "$BASE_URL/debug")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "has tool_status" "$(echo "$BODY" | grep -q '"tool_status"' && echo 1 || echo 0)"

# ── 5. Unknown action ───────────────────────────────────
echo "[5/5] POST / unknown action"
HTTP_CODE=$(curl -s -o /tmp/api_body -w "%{http_code}" -X POST -H "Content-Type: application/json" -d '{"action":"nope"}' "$BASE_URL/")
BODY=$(cat /tmp/api_body)
check "HTTP 200" "$([ "${HTTP_CODE}" = "200" ] && echo 1 || echo 0)" "got ${HTTP_CODE}"
check "error response" "$(echo "$BODY" | grep -q '"error"' && echo 1 || echo 0)"
check "lists available_actions" "$(echo "$BODY" | grep -q '"available_actions"' && echo 1 || echo 0)"

echo ""
echo "==================================================="
echo "  Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "==================================================="

[ "${FAIL}" -gt 0 ] && exit 1
exit 0
