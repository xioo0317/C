#include "server/actions.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace app {

using nlohmann::json;

// ── ping: health check ──────────────────────────────────────────
void handle_ping(const httplib::Request&, httplib::Response& res) {
    res.set_content(json{{"status", "ok"}, {"message", "pong"}}.dump(), "application/json");
}

// ── status: server info ─────────────────────────────────────────
void handle_status(const httplib::Request&, httplib::Response& res) {
    res.set_content(json{
        {"status", "running"},
        {"actions_count", 4},
        {"uptime_hint", "see server logs"}
    }.dump(), "application/json");
}

// ── echo: mirror request body ───────────────────────────────────
void handle_echo(const httplib::Request& req, httplib::Response& res) {
    res.set_content(json{{"echo", json::parse(req.body, nullptr, false)}}.dump(), "application/json");
}

// ── time: current server time ───────────────────────────────────
void handle_time(const httplib::Request&, httplib::Response& res) {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
    res.set_content(json{{"timestamp", seconds}, {"unit", "unix_seconds"}}.dump(), "application/json");
}

} // namespace app
