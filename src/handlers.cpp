#include "server/actions.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace app {

using nlohmann::json;

// ── 小功能注册（POST /in 分发）──────────────────────────────────────
const std::unordered_map<std::string, Handler>& util_registry() {
    static const std::unordered_map<std::string, Handler> registry = {
        {"ping",   &handle_ping},
        {"status", &handle_status},
        {"echo",   &handle_echo},
        {"time",   &handle_time},
    };
    return registry;
}

Handler find_util(const std::string& name) {
    const auto& registry = util_registry();
    const auto it = registry.find(name);
    return it != registry.end() ? it->second : nullptr;
}

// ── ping ────────────────────────────────────────────────────────────
void handle_ping(const httplib::Request&, httplib::Response& res) {
    res.set_content(json{{"status", "ok"}, {"message", "pong"}}.dump(), "application/json");
}

// ── status ──────────────────────────────────────────────────────────
void handle_status(const httplib::Request&, httplib::Response& res) {
    res.set_content(json{
        {"status", "running"},
        {"actions_count", 4},
        {"uptime_hint", "see server logs"}
    }.dump(), "application/json");
}

// ── echo ────────────────────────────────────────────────────────────
void handle_echo(const httplib::Request& req, httplib::Response& res) {
    res.set_content(json{{"echo", json::parse(req.body, nullptr, false)}}.dump(), "application/json");
}

// ── time ────────────────────────────────────────────────────────────
void handle_time(const httplib::Request&, httplib::Response& res) {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    res.set_content(json{{"timestamp", seconds}, {"unit", "unix_seconds"}}.dump(), "application/json");
}

} // namespace app
