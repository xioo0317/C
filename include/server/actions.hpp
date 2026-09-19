#pragma once

#include <functional>
#include <string>
#include <httplib.h>

namespace app {

// ── SSE streaming helpers (implemented in src/sse.cpp) ──────────────
// Send one event frame:  data: {json}\n\n
void sse_send(httplib::DataSink& sink, const std::string& json);
// Send the terminating frame  data: [DONE]\n\n  and close the stream.
void sse_done(httplib::DataSink& sink);
// Build a {"message":"..."} event JSON with minimal escaping.
std::string sse_event(const std::string& message);

// Streaming action handler: pushes events on the sink, must end with sse_done().
using ActionHandler = std::function<void(const std::string& body, httplib::DataSink& sink)>;

// Action handler functions (each in its own .cpp file)
void handle_list_items(const std::string& body, httplib::DataSink& sink);
void handle_create_item(const std::string& body, httplib::DataSink& sink);
void handle_update_item(const std::string& body, httplib::DataSink& sink);
void handle_delete_item(const std::string& body, httplib::DataSink& sink);

// Get action handler by name
ActionHandler get_action_handler(const std::string& action_name);

} // namespace app
