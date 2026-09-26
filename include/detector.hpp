// SPDX-License-Identifier: GPL-3.0-or-later
//
// Manager-level kernel root detector for KernelSU, APatch,
// Magisk, and SusFS handshake verification.
//
// Only checks if each root manager is present via handshake.
// For KernelSU, also reports the mode (e.g. "lkm-bundled").

#pragma once

#include <string>
#include <cstdint>
#include <signal.h>

namespace ksu {
struct get_info_cmd;
}  // namespace ksu

namespace ksu_detector {

// --- Result types ---

enum class KernelType {
    Unknown,
    None,
    KernelSU,
    KernelPatch,
    Magisk,
    Mixed,
};

// --- KernelSU ---

struct KsuResult {
    bool present = false;
    std::string mode_str;  // "lkm-bundled", "lkm", "built-in", "late-load", "legacy-prctl"
};

// --- APatch ---

struct ApResult {
    bool present = false;
};

// --- Magisk ---

struct MagiskResult {
    bool present = false;
};

// --- SusFS ---

struct SusfsResult {
    bool detected = false;
};

// --- Top-level result ---

struct DetectResult {
    KernelType type = KernelType::None;
    KsuResult ksu;
    ApResult  ap;
    MagiskResult magisk;
    SusfsResult susfs;
};

// Serialize a DetectResult to a pretty-printed JSON string.
std::string result_to_json_string(const DetectResult& r);

class Detector {
public:
    Detector();
    ~Detector();
    void enable_sigsys_handler(bool enable);
    DetectResult run_all();
    KsuResult probe_ksu();
    ApResult  probe_apatch();
    MagiskResult probe_magisk();
    SusfsResult probe_susfs();

private:
    bool sigsys_installed_ = false;
    static volatile bool g_sigsys_hit_;
    static void sigsys_handler(int sig, siginfo_t* si, void* ctx);
    void install_sigsys();
    void uninstall_sigsys();
    int ksu_install_fd();
    bool ksu_do_get_info(int fd, ksu::get_info_cmd& info);
    long ap_raw_call(const char* key, uint16_t cmd,
                     long arg3 = 0, long arg4 = 0,
                     long arg5 = 0, long arg6 = 0);
    bool ap_hello(const char* key);
    bool magisk_find_socket(std::string& out_path);
    bool magisk_probe_daemon(const std::string& socket_path,
                             uint32_t& out_version_code,
                             std::string& out_version_str);
};

} // namespace ksu_detector
