// SPDX-License-Identifier: GPL-3.0-or-later
//
// router.hpp — Request dispatcher.
//
// Parses POST body JSON, extracts "action" field,
// dispatches to the corresponding tool function in tools.cpp.

#pragma once

#include <httplib.h>

namespace router {

// Handle POST / — parse body, dispatch to tool by action name.
void handle_post(const httplib::Request& req, httplib::Response& res);

// Handle GET /debug — tool info, version, dry-run for debugging.
void handle_debug(const httplib::Request& req, httplib::Response& res);

} // namespace router
