#pragma once

#include <httplib.h>

namespace app {

// ── Utility handlers (src/handlers.cpp) ──────────────────────────────
void handle_ping(const httplib::Request& req, httplib::Response& res);
void handle_status(const httplib::Request& req, httplib::Response& res);
void handle_echo(const httplib::Request& req, httplib::Response& res);
void handle_time(const httplib::Request& req, httplib::Response& res);

} // namespace app
