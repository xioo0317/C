// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — HTTP network entry point ONLY ("the network card").
//
// This file only starts the httplib server and wires routes to router.cpp.
// All business logic lives in tools.cpp, dispatched by router.cpp.

#include <httplib.h>
#include "version.hpp"
#include "router.hpp"

int main() {
    httplib::Server svr;

    // POST / — all requests go here; body {"action":"..."} selects the tool
    svr.Post("/", router::handle_post);

    // GET /debug — tool info, versions, dry-run for debugging
    svr.Get("/debug", router::handle_debug);

    // JSON 404 instead of any HTML/console noise.
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            nlohmann::json err = {
                {"error", "not found"},
                {"hint", "POST / with {\"action\":\"...\"}  |  GET /debug"}
            };
            res.set_content(err.dump(), "application/json");
        }
    });

    // Bind the network interface. No logger -> fully silent.
    svr.listen("0.0.0.0", local_api::SERVER_PORT);
    return 0;
}
