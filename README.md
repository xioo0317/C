# Local API

C++17 local HTTP API server built on [cpp-httplib](https://github.com/yhirose/cpp-httplib), targeting Android arm64-v8a via ndk-build.

- **JSON**: parsed and generated with [nlohmann/json](https://github.com/nlohmann/json) (v3.11.3, single header, vendored at build time via `scripts/fetch_deps.sh`).
- **Two entry points**: `POST /out` for CRUD actions, `POST /in` for utility functions.
- **Table-driven dispatch**: both routes use `unordered_map` hash lookup, adding a new handler = adding one row.

## Project Structure

```
./
├── jni/
│   ├── Android.mk
│   └── Application.mk
├── src/
│   ├── main.cpp           # Entry point
│   ├── actions.cpp        # Action registry (for POST /out)
│   ├── server.cpp         # Server implementation
│   ├── router.cpp         # Route registration + dispatch
│   ├── handlers.cpp       # Utility registry + handlers (for POST /in)
│   ├── list_items.cpp     # Action: list_items
│   ├── create_item.cpp    # Action: create_item
│   ├── update_item.cpp    # Action: update_item
│   └── delete_item.cpp    # Action: delete_item
├── include/server/
│   ├── server.hpp
│   ├── router.hpp
│   ├── actions.hpp        # Handler type + action registry
│   └── handlers.hpp       # Utility handler declarations
├── third_party/
│   ├── httplib.h
│   └── nlohmann/
├── scripts/
│   └── fetch_deps.sh
├── test_api.sh
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

## API

### POST /out — Action dispatch (CRUD)

| Action         | Response                          |
|----------------|-----------------------------------|
| list_items     | `{"items": []}`                   |
| create_item    | `{"created": true}`               |
| update_item    | `{"updated": true}`               |
| delete_item    | `{"deleted": true}`               |

```bash
curl -X POST http://localhost:8080/out \
     -H "Content-Type: application/json" \
     -d '{"action":"create_item"}'
```

### POST /in — Utility dispatch

| Command   | Response                                    |
|-----------|---------------------------------------------|
| ping      | `{"status":"ok","message":"pong"}`          |
| status    | `{"status":"running","actions_count":4,...}`|
| echo      | `{"echo": <request body>}`                  |
| time      | `{"timestamp":1234567890,"unit":"unix_seconds"}` |

```bash
curl -X POST http://localhost:8080/in \
     -H "Content-Type: application/json" \
     -d '{"command":"ping"}'
```

### Errors

| Condition         | Status | Response                                          |
|-------------------|--------|---------------------------------------------------|
| non-JSON body     | 400    | `{"error":"request body must be a JSON object"}`  |
| missing field     | 400    | `{"error":"missing or invalid 'action' field"}`   |
| unknown name      | 400    | `{"error":"unknown action","action":"..."}`        |

## Test

```bash
./test_api.sh
./test_api.sh 192.168.1.100 8080
```

## Adding New Actions (POST /out)

1. Create `src/my_action.cpp`
2. Declare in `include/server/actions.hpp`
3. Register in `action_registry()` in `src/actions.cpp`
4. Add to `jni/Android.mk`

## Adding New Utilities (POST /in)

1. Implement handler in `src/handlers.cpp`
2. Declare in `include/server/handlers.hpp`
3. Register in `util_registry()` in `src/handlers.cpp`
