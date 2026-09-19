#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <httplib.h>

namespace app {

// ── SSE streaming helpers (implemented in src/sse.cpp) ──────────────
// Send one event frame:  data: {json}\n\n
void sse_send(httplib::DataSink& sink, const std::string& json);
// Send the terminating frame  data: [DONE]\n\n  and close the stream.
void sse_done(httplib::DataSink& sink);
// Build a {"message":"..."} event JSON (nlohmann/json handles escaping).
std::string sse_event(const std::string& message);

// Streaming action handler: pushes events on the sink, must end with sse_done().
using ActionHandler = std::function<void(const std::string& body, httplib::DataSink& sink)>;

// ── Table-driven action registry (src/actions.cpp) ─────────────────
// 统一动作注册入口：所有流式动作集中注册进一张 unordered_map，
// 分发退化为一次哈希查找。新增动作只需在 action_registry() 的
// 注册表里加一行，无需再维护 if-else 分发链。
const std::unordered_map<std::string, ActionHandler>& action_registry();

// Look up an action handler by name (returns nullptr if unknown).
ActionHandler find_action(const std::string& action_name);

// Action handler functions (each in its own .cpp file)
void handle_list_items(const std::string& body, httplib::DataSink& sink);
void handle_create_item(const std::string& body, httplib::DataSink& sink);
void handle_update_item(const std::string& body, httplib::DataSink& sink);
void handle_delete_item(const std::string& body, httplib::DataSink& sink);

} // namespace app
