#pragma once

#include <httplib.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace app {

// ── Handler type ─────────────────────────────────────────────────────
using Handler = std::function<void(const httplib::Request& req, httplib::Response& res)>;

// ── Action registry (src/actions.cpp) ────────────────────────────────
const std::unordered_map<std::string, Handler>& action_registry();
Handler find_action(const std::string& name);

// ── Utility registry (src/handlers.cpp) ──────────────────────────────
const std::unordered_map<std::string, Handler>& util_registry();
Handler find_util(const std::string& name);

// ── CRUD action handlers (each in its own .cpp file) ────────────────
void handle_list_items(const httplib::Request& req, httplib::Response& res);
void handle_create_item(const httplib::Request& req, httplib::Response& res);
void handle_update_item(const httplib::Request& req, httplib::Response& res);
void handle_delete_item(const httplib::Request& req, httplib::Response& res);

} // namespace app
