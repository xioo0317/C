// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — HTTP network entry point ONLY ("start the network card").
//
// This file only brings up the cpp-httplib server and wires the routes.
// All KernelSU / APatch / Magisk / SusFS detection lives in detector.cpp.
// There is NO terminal output: after each detection the JSON result is
// written to /data/local/tmp/coverRoot/root_detect.json for other programs
// to consume.

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <string>
#include <fstream>
#include <sys/stat.h>

#include "detector.hpp"

using json = nlohmann::json;

namespace {

// Production defaults; overridable at compile time only for host testing
// (e.g. -DDETECT_PORT=18080 -DDETECT_OUTPUT_DIR=/tmp/coverRoot).
#ifndef DETECT_PORT
#define DETECT_PORT 8080
#endif
#ifndef DETECT_OUTPUT_DIR
#define DETECT_OUTPUT_DIR "/data/local/tmp/coverRoot"
#endif
#ifndef DETECT_OUTPUT_FILE
#define DETECT_OUTPUT_FILE "/data/local/tmp/coverRoot/root_detect.json"
#endif

constexpr const char* kOutputDir  = DETECT_OUTPUT_DIR;
constexpr const char* kOutputFile = DETECT_OUTPUT_FILE;
constexpr int kPort = DETECT_PORT;

// Run the full detector and persist the JSON result to disk.
bool run_detection_to_file() {
    ksu_detector::Detector detector;
    ksu_detector::DetectResult result = detector.run_all();
    const std::string body = ksu_detector::result_to_json_string(result);

    mkdir(kOutputDir, 0755);
    std::ofstream out(kOutputFile, std::ios::trunc);
    if (!out.is_open()) return false;
    out << body;
    return out.good();
}

} // anonymous namespace

int main() {
    httplib::Server svr;

    // POST /api/v1/detect — run detection, write JSON file, report status.
    svr.Post("/api/v1/detect", [](const httplib::Request&, httplib::Response& res) {
        const bool ok = run_detection_to_file();
        json reply;
        if (ok) {
            reply = {
                {"status", "ok"},
                {"result_file", kOutputFile},
            };
            res.status = 200;
        } else {
            reply = {
                {"status", "error"},
                {"error", "failed to write result file"},
            };
            res.status = 500;
        }
        res.set_content(reply.dump(), "application/json");
    });

    // JSON 404 instead of any HTML/console noise.
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(
                json{{"error", "not found"}, {"hint", "use POST /api/v1/detect"}}.dump(),
                "application/json");
        }
    });

    // Bind the network interface. No logger is installed -> fully silent.
    svr.listen("0.0.0.0", kPort);
    return 0;
}
