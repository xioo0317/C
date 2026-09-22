# Local API

C++17 local HTTP API server built on [cpp-httplib](https://github.com/yhirose/cpp-httplib), targeting Android arm64-v8a via ndk-build.

- **JSON**: parsed and generated with [nlohmann/json](https://github.com/nlohmann/json) (v3.11.3, single header, vendored at build time via `scripts/fetch_deps.sh`).
- **Action dispatch**: table-driven — every action is registered in one `std::unordered_map` (`action_registry()` in `src/actions.cpp`), so routing is a single hash lookup.
- **Response format**: plain JSON (`application/json`), no SSE streaming.

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
│   ├── router.cpp         # Route registration + action dispatch
│   ├── handlers.cpp       # Utility handlers (ping, status, echo, time)
│   ├── list_items.cpp     # Action: list_items
│   ├── create_item.cpp    # Action: create_item
│   ├── update_item.cpp    # Action: update_item
│   └── delete_item.cpp    # Action: delete_item
├── include/
│   └── server/
│       ├── server.hpp     # Server wrapper class
│       ├── router.hpp     # Route registration
│       ├── actions.hpp    # Action registry + handler declarations
│       └── handlers.hpp   # Utility handler declarations
├── third_party/
│   ├── httplib.h          # Header-only HTTP library
│   └── nlohmann/          # Filled by scripts/fetch_deps.sh
├── scripts/
│   └── fetch_deps.sh      # Vendors nlohmann/json at build time
├── test_api.sh            # JSON API test suite
└── README.md
```

## Build

```bash
./scripts/fetch_deps.sh
export NDK_HOME=/path/to/android-ndk-rXX
$NDK_HOME/ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=jni/Android.mk NDK_APPLICATION_MK=jni/Application.mk
```

## Run

```bash
adb push libs/arm64-v8a/local_api /data/local/tmp/
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

Server starts on `http://localhost:8080`.

## API Endpoints

### Action Dispatch

| Method | Path            | Response         |
|--------|-----------------|------------------|
| POST   | /api/v1/execute | application/json |

Request body: `{"action": "<action_name>"}`

Supported actions: `list_items`, `create_item`, `update_item`, `delete_item`.

### Utility Endpoints

| Method | Path           | Response         | Description         |
|--------|----------------|------------------|---------------------|
| GET    | /api/v1/ping   | application/json | Health check (pong) |
| GET    | /api/v1/status | application/json | Server status info  |
| POST   | /api/v1/echo   | application/json | Echo request body   |
| GET    | /api/v1/time   | application/json | Current server time |

### Error Responses

- non-JSON body → `400` + `{"error":"request body must be a JSON object"}`
- missing `action` → `400` + `{"error":"missing or invalid 'action' field"}`
- unknown action → `400` + `{"error":"unknown action","action":"..."}`

## Test

```bash
./test_api.sh              # default localhost:8080
./test_api.sh 192.168.1.100 8080  # custom host/port
```

## Adding New Actions

1. Create `src/my_action.cpp`:

```cpp
#include "server/actions.hpp"
#include <nlohmann/json.hpp>

namespace app {
void handle_my_action(const httplib::Request& req, httplib::Response& res) {
    const json payload = json::parse(req.body, nullptr, false);
    // ... do work ...
    res.status = 200;
    res.set_content(json{{"result", "ok"}}.dump(), "application/json");
}
} // namespace app
```

2. Declare in `include/server/actions.hpp`
3. Register in `action_registry()` (`src/actions.cpp`)
4. Add to `jni/Android.mk`

## Adding Utility Functions

Small utility endpoints go in `src/handlers.cpp`:

1. Implement handler in `src/handlers.cpp`
2. Declare in `include/server/handlers.hpp`
3. Register route in `src/router.cpp`
