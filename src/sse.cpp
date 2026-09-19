#include "server/actions.hpp"

#include <cstdio>
#include <string>

namespace app {

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

// Build {"message":"..."} JSON with minimal escaping so the value
// can never break the SSE frame structure.
std::string sse_event(const std::string& message) {
    std::string out;
    out.reserve(message.size() + 16);
    out += "{\"message\":\"";
    for (char c : message) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += "\"}";
    return out;
}

} // namespace app
