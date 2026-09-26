// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — HTTP network entry point ONLY.
//
// This file only brings up the httplib server and wires POST / to the
// detection handler. All detection logic lives in detector.cpp.
// Version info is served at GET /version for frontend update checks.

#include <httplib.h>
#include "version.hpp"
#include "detector.hpp"

int main() {
    httplib::Server svr;

    // POST / — run root detection (dispatched to detector.cpp)
    svr.Post("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(ksu_detector::handle_detect(), "application/json");
    });

    // GET /version — frontend checks server version for updates
    svr.Get("/version", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(local_api::get_version_info(), "application/json");
    });

    // JSON 404 instead of any HTML/console noise.
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            nlohmann::json err = {
                {"error", "not found"},
                {"hint", "POST / for detection, GET /version for version info"}
            };
            res.set_content(err.dump(), "application/json");
        }
    });

    // Bind the network interface. No logger -> fully silent.
    svr.listen("0.0.0.0", local_api::SERVER_PORT);
    return 0;
}
