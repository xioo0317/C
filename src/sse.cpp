#include "server/actions.hpp"

#include <nlohmann/json.hpp>

namespace app {

using nlohmann::json;

void sse_send(httplib::DataSink& sink, const std::string& json) {
    if (!sink.is_writable()) return;
    std::string frame = "data: " + json + "\n\n";
    sink.write(frame.data(), frame.size());
}

void sse_done(httplib::DataSink& sink) {
    static const char kDone[] = "data: [DONE]\n\n";
    if (sink.is_writable()) sink.write(kDone, sizeof(kDone) - 1);
    sink.done(); // tell httplib the chunked response is finished
}

// Build {"message":"..."} — nlohmann/json handles all escaping, so the
// value can never break the SSE frame structure.
std::string sse_event(const std::string& message) {
    return json{{"message", message}}.dump();
}

} // namespace app
