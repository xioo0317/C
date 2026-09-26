// SPDX-License-Identifier: GPL-3.0-or-later
//
// router.cpp — Request dispatcher implementation.
//
// POST / with {"action": "xxx"} dispatches to tools::execute("xxx").
// GET /debug returns tool info, versions, and dry-run output.

#include "router.hpp"
#include "tools.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace router {

void handle_post(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body);
    } catch (...) {
        json err = {{"status", "error"}, {"error", "invalid JSON body"}};
        res.set_content(err.dump(), "application/json");
        return;
    }

    std::string action = body.value("action", "");
    if (action.empty()) {
        json err = {
            {"status", "error"},
            {"error", "missing 'action' field"},
            {"available_actions", tools::list_actions()}
        };
        res.set_content(err.dump(), "application/json");
        return;
    }

    res.set_content(tools::execute(action, body), "application/json");
}

void handle_debug(const httplib::Request&, httplib::Response& res) {
    res.set_content(tools::debug_info(), "application/json");
}

} // namespace router
