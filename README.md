# Local API

C++17 local HTTP API server built on [cpp-httplib](https://github.com/yhirose/cpp-httplib), targeting Android arm64-v8a via ndk-build.

- **JSON**: parsed and generated with [nlohmann/json](https://github.com/nlohmann/json) (v3.11.3, single header, vendored at build time via `scripts/fetch_deps.sh`).
- **Action dispatch**: table-driven — every action is registered in one `std::unordered_map` (`action_registry()` in `src/actions.cpp`), so routing is a single hash lookup with a unified registration entry.

## Project Structure

```
./
├── jni/
│   ├── Android.mk         # ndk-build module config
│   └── Application.mk     # ndk-build app config (arm64-v8a)
├── src/
│   ├── main.cpp           # Entry point
│   ├── actions.cpp        # Unified action registry (unordered_map)
│   ├── server.cpp         # Server implementation
│   ├── router.cpp         # Route registration + SSE dispatch
│   ├── sse.cpp            # SSE streaming helpers (send/done/event)
│   ├── list_items.cpp     # Action: list_items
│   ├── create_item.cpp    # Action: create_item
│   ├── update_item.cpp    # Action: update_item
│   └── delete_item.cpp    # Action: delete_item
├── include/
│   └── server/
│       ├── server.hpp     # Server wrapper class
│       ├── router.hpp     # Route registration
│       └── actions.hpp    # Action registry + handlers + SSE helpers
├── scripts/
│   └── fetch_deps.sh      # Vendors nlohmann/json at build time
├── third_party/
│   ├── httplib.h          # Header-only HTTP library
│   └── nlohmann/          # Filled by scripts/fetch_deps.sh
├── test_api.sh            # SSE streaming test suite
└── README.md
```

## Build

Prerequisites: [Android NDK](https://developer.android.com/ndk/downloads) (r21+).

```bash
# 1. Vendor nlohmann/json (no-op if already present)
./scripts/fetch_deps.sh

# 2. Build
export NDK_HOME=/path/to/android-ndk-rXX

$NDK_HOME/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk

# Output binary
ls libs/arm64-v8a/local_api
```

## Run

```bash
adb push libs/arm64-v8a/local_api /data/local/tmp/
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

Server starts on `http://localhost:8080`.

## API Endpoint

All actions go through a single streaming endpoint:

| Method | Path                | Response          |
|--------|---------------------|-------------------|
| POST   | /api/v1/execute     | text/event-stream |

Request body: `{"action": "<action_name>"}`

Supported actions: `list_items`, `create_item`, `update_item`, `delete_item`.

## SSE Streaming Response

Responses are Server-Sent Events: one `data: {...}` JSON frame per event,
terminated by a `data: [DONE]` frame.

```text
data: {"message":"开始执行指令"}

data: {"step":"write_log","status":"ok"}

data: {"result":{"created":true},"status":"completed"}

data: [DONE]
```

Event order per action: start (`开始执行指令`) → step events → result (`status:"completed"`) → `[DONE]`.

Invalid requests stay plain JSON:

- non-JSON body → `400` + `{"error":"request body must be a JSON object"}`
- missing `action` field → `400` + `{"error":"missing or invalid 'action' field"}`
- unknown action → `400` + `{"error":"unknown action","action":"..."}`

Example with curl:

```bash
curl -N -X POST http://localhost:8080/api/v1/execute \
     -H "Content-Type: application/json" \
     -d '{"action":"create_item"}'
```

(`-N` disables curl buffering so frames arrive in real time.)

## Test

```bash
# Default: test localhost:8080
./test_api.sh

# Custom host and port
./test_api.sh 192.168.1.100 8080
```

The suite validates every action's stream: HTTP 200, `Content-Type: text/event-stream`,
start frame, result frame, `[DONE]` terminator, frame order, and the JSON error paths.

## Adding New Actions

1. Create `src/my_action.cpp` and implement the streaming handler:

```cpp
#include "server/actions.hpp"

namespace app {

void handle_my_action(const std::string& body, httplib::DataSink& sink) {
    sse_send(sink, sse_event("开始执行指令"));
    // ... do work, stream progress with sse_send(sink, ...):
    sse_send(sink, json{{"step", "..."}, {"status", "ok"}}.dump());
    sse_send(sink, json{{"result", json{{"ok", true}}}, {"status", "completed"}}.dump());
    sse_done(sink);   // must end with data: [DONE]
}

} // namespace app
```

2. Declare it in `include/server/actions.hpp`, register it in the unified registry table
   `action_registry()` (`src/actions.cpp`), and add the file to `jni/Android.mk`.
   Dispatch needs no further changes — the router looks the handler up in the map.
