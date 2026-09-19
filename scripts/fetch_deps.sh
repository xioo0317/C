#!/usr/bin/env bash
# fetch_deps.sh - vendor header-only third-party dependencies that are
# not stored in git (keeps the repo small; nlohmann/json is ~900 KB).
set -euo pipefail
cd "$(dirname "$0")/.."

JSON_URL="https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
JSON_DST="third_party/nlohmann/json.hpp"

if [ -s "$JSON_DST" ] && grep -q "NLOHMANN_JSON_VERSION_MAJOR" "$JSON_DST"; then
    echo "nlohmann/json already vendored: $JSON_DST ($(wc -c < "$JSON_DST") bytes)"
    exit 0
fi

mkdir -p "$(dirname "$JSON_DST")"
echo "Downloading nlohmann/json v3.11.3 ..."
curl -fsSL --retry 3 -o "$JSON_DST" "$JSON_URL"
grep -q "NLOHMANN_JSON_VERSION_MAJOR" "$JSON_DST" || { echo "invalid json.hpp downloaded"; exit 1; }
echo "OK: $JSON_DST ($(wc -c < "$JSON_DST") bytes)"
