#pragma once

#include <httplib.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace app {

// ── Action handler type ──────────────────────────────────────────────
// Plain JSON handler: receives the request, writes response directly.
using ActionHandler = std::function<void(const httplib::Request& req, httplib::Response& res)>;

// ── Table-driven action registry (src/actions.cpp) ──────────────────
const std::unordered_map<std::string, ActionHandler>& action_registry();
ActionHandler find_action(const std::string& action_name);

// ── CRUD action handlers (each in its own .cpp file) ────────────────
void handle_list_items(const httplib::Request& req, httplib::Response& res);
void handle_create_item(const httplib::Request& req, httplib::Response& res);
void handle_update_item(const httplib::Request& req, httplib::Response& res);
void handle_delete_item(const httplib::Request& req, httplib::Response& res);

} // namespace app
