// SPDX-License-Identifier: GPL-3.0-or-later
//
// local_api — Single-file HTTP API server with integrated KernelSU / APatch /
// Magisk detection. All logic in one file, one POST route.
//
// POST /api/v1/detect  { "superkey": "..." }  →  JSON detection result
//
// Build:
//   make                     # host build (g++)
//   make ndk NDK_HOME=...   # Android NDK cross-compile
//

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <signal.h>
#include <sstream>
#include <string>
#include <ucontext.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>

#include "ksu_uapi.hpp"
#include "apatch_uapi.hpp"
#include "magisk_uapi.hpp"

using json = nlohmann::json;

// ===========================================================================
//  Result types  (from ksu-detect detector.hpp)
// ===========================================================================
namespace ksu_detector {

enum class KernelType { Unknown, None, KernelSU, KernelPatch, Magisk, Mixed };

inline const char* kernel_type_str(KernelType t) {
    switch (t) {
        case KernelType::Unknown:     return "unknown";
        case KernelType::None:        return "none";
        case KernelType::KernelSU:    return "kernelsu";
        case KernelType::KernelPatch: return "kernelpatch";
        case KernelType::Magisk:      return "magisk";
        case KernelType::Mixed:       return "mixed";
    }
    return "unknown";
}

// --- KernelSU ---
enum class KsuPrivLevel { None, User, ManagerOrRoot, Manager, Root };

inline const char* ksu_priv_str(KsuPrivLevel p) {
    switch (p) {
        case KsuPrivLevel::None:          return "none";
        case KsuPrivLevel::User:          return "user";
        case KsuPrivLevel::ManagerOrRoot: return "manager_or_root";
        case KsuPrivLevel::Manager:       return "manager";
        case KsuPrivLevel::Root:          return "root";
    }
    return "none";
}

enum class KsuVariant : uint32_t {
    Standard      = 0,
    GKI           = (1u << 0),
    LateLoad      = (1u << 1),
    BuiltIn       = (1u << 2),
    LKM           = (1u << 3),
    SusFS         = (1u << 4),
    PRBuild       = (1u << 5),
    KMICompatible = (1u << 6),
};
inline KsuVariant operator|(KsuVariant a, KsuVariant b) {
    return static_cast<KsuVariant>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool has_ksu_variant(KsuVariant v, KsuVariant flag) {
    return (static_cast<uint32_t>(v) & static_cast<uint32_t>(flag)) != 0;
}

struct KsuResult {
    bool present = false;
    uint32_t version = 0;
    uint32_t uapi_version = 0;
    uint32_t flags = 0;
    uint32_t features = 0;
    KsuPrivLevel priv_level = KsuPrivLevel::None;
    KsuVariant variant = KsuVariant::Standard;
    std::optional<uint32_t> manager_appid;
    std::string mode_str;
    bool susfs_detected = false;
    std::string susfs_detail;
    bool kernel_compromised = false;
    std::string compromise_reason;
};

// --- APatch ---
enum class ApPrivLevel { None, Unconfirmed, SuList, SuperKey };

inline const char* ap_priv_str(ApPrivLevel p) {
    switch (p) {
        case ApPrivLevel::None:        return "none";
        case ApPrivLevel::Unconfirmed: return "unconfirmed";
        case ApPrivLevel::SuList:      return "su_list";
        case ApPrivLevel::SuperKey:    return "superkey";
    }
    return "none";
}

struct ApResult {
    bool present = false;
    ApPrivLevel priv_level = ApPrivLevel::None;
    uint32_t kp_version = 0;
    uint32_t kernel_version = 0;
    std::optional<long> su_uid_count;
    std::optional<long> kpm_count;
    std::optional<long> safemode;
    std::string detected_key_source;
};

// --- Magisk ---
enum class MagiskPrivLevel { None, Unconfirmed, DaemonOnly, Su, Manager };

inline const char* magisk_priv_str(MagiskPrivLevel p) {
    switch (p) {
        case MagiskPrivLevel::None:       return "none";
        case MagiskPrivLevel::Unconfirmed:return "unconfirmed";
        case MagiskPrivLevel::DaemonOnly: return "daemon_only";
        case MagiskPrivLevel::Su:         return "su";
        case MagiskPrivLevel::Manager:    return "manager";
    }
    return "none";
}

struct MagiskResult {
    bool present = false;
    MagiskPrivLevel priv_level = MagiskPrivLevel::None;
    uint32_t version_code = 0;
    std::string version_str;
    std::string socket_path;
    bool zygisk_detected = false;
    std::string zygisk_detail;
    bool su_binary_detected = false;
    std::string su_binary_path;
    bool has_zygisk     = false;
    bool has_shamiko    = false;
    bool has_susfs      = false;
    bool has_lsposed    = false;
    bool has_magiskhide = false;
    bool is_kitsune     = false;
    bool is_alpha       = false;
};

// --- Jailbreak ---
struct JailbreakHint {
    bool detected = false;
    std::vector<std::string> indicators;
};

// --- Top-level ---
struct DetectResult {
    KernelType type = KernelType::None;
    KsuResult ksu;
    ApResult  ap;
    MagiskResult magisk;
    JailbreakHint jailbreak;
    bool susfs_detected = false;
    std::string susfs_source;
};

// ===========================================================================
//  Detector implementation  (from ksu-detect detector.cpp)
// ===========================================================================
class Detector {
public:
    Detector();
    ~Detector();
    void set_ap_superkey(const std::string& key);
    DetectResult run_all();

private:
    std::string ap_superkey_;
    bool sigsys_installed_ = false;
    static volatile bool g_sigsys_hit_;

    static void sigsys_handler(int sig, siginfo_t* si, void* ctx);
    void install_sigsys();
    void uninstall_sigsys();

    // KernelSU
    int ksu_install_fd();
    bool ksu_do_get_info(int fd, ksu::get_info_cmd& info);
    bool ksu_do_get_manager_appid(int fd, uint32_t& appid);
    KsuResult probe_ksu();

    // APatch
    long ap_raw_call(const char* key, uint16_t cmd,
                     long arg3 = 0, long arg4 = 0,
                     long arg5 = 0, long arg6 = 0);
    bool ap_hello(const char* key);
    uint32_t ap_kp_ver(const char* key);
    uint32_t ap_k_ver(const char* key);
    long ap_su_nums(const char* key);
    long ap_kpm_nums(const char* key);
    long ap_safemode(const char* key);
    bool ap_try_skey_get(const char* key, char* buf, size_t buf_len);
    std::string try_find_superkey();
    ApResult probe_apatch();

    // Magisk
    bool magisk_find_socket(std::string& out_path);
    bool magisk_probe_daemon(const std::string& socket_path,
                             uint32_t& out_version_code,
                             std::string& out_version_str);
    bool magisk_check_zygisk();
    bool magisk_check_su_binary(std::string& out_path);
    bool magisk_check_module(const std::string& module_id);
    bool magisk_check_kitsune();
    bool magisk_check_alpha();
    MagiskResult probe_magisk();

    void probe_variants(DetectResult& out);
    void probe_jailbreak(DetectResult& out);
};

// --- Statics ---
volatile bool Detector::g_sigsys_hit_ = false;

void Detector::sigsys_handler(int sig, siginfo_t* si, void* ctx) {
    (void)sig;
    if (!si || si->si_code != 1) return;
    g_sigsys_hit_ = true;
#if defined(__aarch64__)
    ucontext_t* uc = static_cast<ucontext_t*>(ctx);
    uc->uc_mcontext.regs[0] = static_cast<uint64_t>(-EPERM);
#elif defined(__x86_64__)
    ucontext_t* uc = static_cast<ucontext_t*>(ctx);
    uc->uc_mcontext.gregs[REG_RAX] = static_cast<long>(-EPERM);
#else
    (void)ctx;
#endif
}

void Detector::install_sigsys() {
    if (sigsys_installed_) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigsys_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, nullptr);
    sigsys_installed_ = true;
}

void Detector::uninstall_sigsys() {
    if (!sigsys_installed_) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, nullptr);
    sigsys_installed_ = false;
}

Detector::Detector() = default;
Detector::~Detector() { if (sigsys_installed_) uninstall_sigsys(); }
void Detector::set_ap_superkey(const std::string& key) { ap_superkey_ = key; }

// ========================== KernelSU =======================================
int Detector::ksu_install_fd() {
    g_sigsys_hit_ = false;
    int fd = -1;
    long r = syscall(SYS_reboot,
                     static_cast<unsigned int>(ksu::INSTALL_MAGIC1),
                     static_cast<unsigned int>(ksu::INSTALL_MAGIC2),
                     0, &fd);
    if (r < 0 && fd < 0) return -1;
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    return fd;
}

bool Detector::ksu_do_get_info(int fd, ksu::get_info_cmd& info) {
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, ksu::IOCTL_GET_INFO, &info) < 0) return false;
    return info.version != 0;
}

bool Detector::ksu_do_get_manager_appid(int fd, uint32_t& appid) {
    ksu::get_manager_appid_cmd cmd{};
    if (ioctl(fd, ksu::IOCTL_GET_MANAGER_APPID, &cmd) < 0) return false;
    appid = cmd.appid;
    return true;
}

static bool check_proc_susfs() {
    return access("/proc/sys/kernel/susfs_version", F_OK) == 0 ||
           access("/proc/sys/kernel/susfs_booting", F_OK) == 0 ||
           access("/proc/sys/kernel/susfs_magisk_sulist", F_OK) == 0;
}

static bool check_module_susfs() {
    return access("/sys/module/susfs", F_OK) == 0;
}

static bool check_gki_kernel() {
    FILE* f = fopen("/proc/version", "r");
    if (f) {
        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            if (strstr(buf, "android") || strstr(buf, "gki")) return true;
        } else { fclose(f); }
    }
    return false;
}

KsuResult Detector::probe_ksu() {
    KsuResult result;
    install_sigsys();
    int fd = ksu_install_fd();
    if (fd >= 0) {
        ksu::get_info_cmd info;
        if (ksu_do_get_info(fd, info)) {
            result.present = true;
            result.kernel_compromised = true;
            result.compromise_reason = "ksu-driver-fd (reboot-magic install + GET_INFO success)";
            result.version = info.version;
            result.uapi_version = info.uapi_version;
            result.flags = info.flags;
            result.features = info.features;
            result.variant = KsuVariant::Standard;
            if (info.flags & ksu::GET_INFO_FLAG_LATE_LOAD) {
                result.mode_str = "late-load";
                result.variant = result.variant | KsuVariant::LateLoad;
            } else if (info.flags & ksu::GET_INFO_FLAG_LKM) {
                result.mode_str = (info.flags & ksu::GET_INFO_FLAG_BUNDLED) ? "lkm-bundled" : "lkm";
                result.variant = result.variant | KsuVariant::LKM;
                if (check_gki_kernel())
                    result.variant = result.variant | KsuVariant::GKI | KsuVariant::KMICompatible;
            } else {
                result.mode_str = "built-in";
                result.variant = result.variant | KsuVariant::BuiltIn;
            }
            if (info.flags & ksu::GET_INFO_FLAG_PR_BUILD)
                result.variant = result.variant | KsuVariant::PRBuild;
            uid_t uid = getuid();
            if (uid == 0) result.priv_level = KsuPrivLevel::Root;
            else if (info.flags & ksu::GET_INFO_FLAG_MANAGER) result.priv_level = KsuPrivLevel::Manager;
            else {
                uint32_t appid = 0;
                if (ksu_do_get_manager_appid(fd, appid)) {
                    result.priv_level = KsuPrivLevel::ManagerOrRoot;
                    result.manager_appid = appid;
                } else { result.priv_level = KsuPrivLevel::User; }
            }
            if (result.manager_appid == std::nullopt && result.priv_level >= KsuPrivLevel::ManagerOrRoot) {
                uint32_t appid = 0;
                if (ksu_do_get_manager_appid(fd, appid)) result.manager_appid = appid;
            }
            if (check_proc_susfs() || check_module_susfs()) {
                result.susfs_detected = true;
                result.variant = result.variant | KsuVariant::SusFS;
                result.susfs_detail = "kernel-level (proc/sys/module)";
            } else if (access("/data/adb/ksu/modules/susfs", F_OK) == 0) {
                result.susfs_detected = true;
                result.variant = result.variant | KsuVariant::SusFS;
                result.susfs_detail = "module-installed (not active)";
            }
        }
        close(fd);
    }
    if (!result.present) {
        long legacy_ver = prctl(ksu::LEGACY_MAGIC, 0, 0, 0, 0);
        if (legacy_ver >= 0) {
            result.present = true;
            result.kernel_compromised = true;
            result.compromise_reason = "legacy-prctl (prctl(KSU_LEGACY_MAGIC) succeeded)";
            result.version = static_cast<uint32_t>(legacy_ver);
            result.mode_str = "legacy-prctl";
            result.priv_level = KsuPrivLevel::User;
        }
    }
    return result;
}

// ========================== APatch =========================================
long Detector::ap_raw_call(const char* key, uint16_t cmd,
                           long arg3, long arg4, long arg5, long arg6) {
    if (!key || !key[0]) return -EINVAL;
    uint64_t ver_cmd = apatch::make_ver_and_cmd(0, cmd);
    return syscall(static_cast<long>(apatch::NR_SUPERCALL), key,
                   static_cast<long>(ver_cmd), arg3, arg4, arg5, arg6);
}

bool Detector::ap_hello(const char* key) {
    if (!key || !key[0]) return false;
    long ret = ap_raw_call(key, apatch::SUPERCALL_HELLO);
    return static_cast<uint32_t>(ret) == apatch::HELLO_MAGIC;
}

uint32_t Detector::ap_kp_ver(const char* key) {
    return static_cast<uint32_t>(ap_raw_call(key, apatch::SUPERCALL_KERNELPATCH_VER));
}
uint32_t Detector::ap_k_ver(const char* key) {
    return static_cast<uint32_t>(ap_raw_call(key, apatch::SUPERCALL_KERNEL_VER));
}
long Detector::ap_su_nums(const char* key) {
    return ap_raw_call(key, apatch::SUPERCALL_SU_NUMS);
}
long Detector::ap_kpm_nums(const char* key) {
    return ap_raw_call(key, apatch::SUPERCALL_KPM_NUMS);
}
long Detector::ap_safemode(const char* key) {
    return ap_raw_call(key, apatch::SUPERCALL_SU_GET_SAFEMODE);
}

bool Detector::ap_try_skey_get(const char* key, char* buf, size_t buf_len) {
    if (!key || !key[0] || !buf || buf_len < apatch::KEY_MAX_LEN) return false;
    return ap_raw_call(key, apatch::SUPERCALL_SKEY_GET,
                       reinterpret_cast<long>(buf),
                       static_cast<long>(buf_len)) == 0;
}

std::string Detector::try_find_superkey() {
    std::ifstream f(apatch::SUPERKEY_PATH);
    if (f.is_open()) {
        std::string key;
        std::getline(f, key);
        while (!key.empty() && (key.back() == '\n' || key.back() == '\r' || key.back() == ' '))
            key.pop_back();
        if (!key.empty() && key.size() < apatch::KEY_MAX_LEN) return key;
    }
    return {};
}

ApResult Detector::probe_apatch() {
    ApResult result;
    std::string key = ap_superkey_;
    std::string key_source = "user-provided";
    if (key.empty()) { key = try_find_superkey(); if (!key.empty()) key_source = "superkey-file"; }

    std::string su_key = "su";
    if (!key.empty() && ap_hello(key.c_str())) {
        result.present = true;
        result.detected_key_source = key_source;
        char key_buf[apatch::KEY_MAX_LEN + 1] = {0};
        if (ap_try_skey_get(key.c_str(), key_buf, sizeof(key_buf)))
            result.priv_level = ApPrivLevel::SuperKey;
        else
            result.priv_level = ApPrivLevel::SuList;
        result.kp_version = ap_kp_ver(key.c_str());
        result.kernel_version = ap_k_ver(key.c_str());
        long nums = ap_su_nums(key.c_str()); if (nums >= 0) result.su_uid_count = nums;
        long kpms = ap_kpm_nums(key.c_str()); if (kpms >= 0) result.kpm_count = kpms;
        long sm = ap_safemode(key.c_str()); if (sm >= 0) result.safemode = sm;
        return result;
    }
    if (ap_hello(su_key.c_str())) {
        result.present = true;
        result.priv_level = ApPrivLevel::SuList;
        result.detected_key_source = "su-allow-list";
        result.kp_version = ap_kp_ver(su_key.c_str());
        result.kernel_version = ap_k_ver(su_key.c_str());
        long nums = ap_su_nums(su_key.c_str()); if (nums >= 0) result.su_uid_count = nums;
        long kpms = ap_kpm_nums(su_key.c_str()); if (kpms >= 0) result.kpm_count = kpms;
        long sm = ap_safemode(su_key.c_str()); if (sm >= 0) result.safemode = sm;
        return result;
    }
    result.priv_level = key.empty() ? ApPrivLevel::Unconfirmed : ApPrivLevel::None;
    return result;
}

// ========================== Magisk =========================================
bool Detector::magisk_find_socket(std::string& out_path) {
    auto is_sock = [](const char* p) {
        struct stat st;
        return stat(p, &st) == 0 && S_ISSOCK(st.st_mode);
    };
    if (is_sock(magisk::DSOCKET_PATH))     { out_path = magisk::DSOCKET_PATH; return true; }
    if (is_sock(magisk::SBIN_SOCKET_PATH)) { out_path = magisk::SBIN_SOCKET_PATH; return true; }
    FILE* f = fopen("/proc/net/unix", "r");
    if (f) {
        char line[512];
        if (fgets(line, sizeof(line), f)) {
            while (fgets(line, sizeof(line), f)) {
                char* sp = strrchr(line, ' ');
                if (!sp) continue;
                while (*sp == ' ' || *sp == '\n' || *sp == '\r') { *sp = '\0'; if (sp == line) break; --sp; }
                char* path = strrchr(line, ' ');
                if (!path) continue;
                ++path;
                if (*path == '@' || !*path) continue;
                if (strstr(path, magisk::SOCKET_DIR_MARKER)) {
                    out_path = path;
                    fclose(f);
                    return true;
                }
            }
        }
        fclose(f);
    }
    if (is_sock(magisk::LEGACY_SOCKET_PATH)) { out_path = magisk::LEGACY_SOCKET_PATH; return true; }
    return false;
}

bool Detector::magisk_probe_daemon(const std::string& socket_path,
                                   uint32_t& out_version_code,
                                   std::string& out_version_str) {
    auto do_connect = [](const std::string& path) -> int {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return -1;
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        bool is_abstract = path[0] == '@';
        std::string name = is_abstract ? path.substr(1) : path;
        if (name.size() >= sizeof(addr.sun_path)) { close(sock); return -1; }
        if (is_abstract) {
            addr.sun_path[0] = '\0';
            memcpy(addr.sun_path + 1, name.c_str(), name.size());
        } else {
            memcpy(addr.sun_path, name.c_str(), name.size());
        }
        socklen_t addr_len = static_cast<socklen_t>(
            offsetof(struct sockaddr_un, sun_path) +
            (is_abstract ? 1 : 0) + name.size());
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), addr_len) < 0) {
            close(sock); return -1;
        }
        return sock;
    };
    auto write_i32 = [](int sock, int32_t v) -> bool {
        ssize_t n = write(sock, &v, sizeof(v));
        return n == static_cast<ssize_t>(sizeof(v));
    };
    auto read_i32 = [](int sock, int32_t& v) -> bool {
        ssize_t n = read(sock, &v, sizeof(v));
        return n == static_cast<ssize_t>(sizeof(v));
    };
    {
        int sock = do_connect(socket_path);
        if (sock < 0) return false;
        if (!write_i32(sock, magisk::DaemonRequestCode::CHECK_VERSION_CODE)) { close(sock); return false; }
        int32_t resp = -1;
        if (!read_i32(sock, resp)) { close(sock); return false; }
        if (resp != magisk::DaemonRespondCode::RESP_OK) { close(sock); return false; }
        int32_t ver = 0;
        if (!read_i32(sock, ver) || ver <= 0) { close(sock); return false; }
        out_version_code = static_cast<uint32_t>(ver);
        close(sock);
    }
    {
        int sock = do_connect(socket_path);
        if (sock < 0) return true;
        bool ok = write_i32(sock, magisk::DaemonRequestCode::CHECK_VERSION);
        int32_t resp = -1;
        if (!ok || !read_i32(sock, resp) || resp != magisk::DaemonRespondCode::RESP_OK) { close(sock); return true; }
        int32_t len = 0;
        if (read_i32(sock, len) && len > 0 && len < 4096) {
            std::vector<char> buf(static_cast<size_t>(len) + 1, 0);
            ssize_t total = 0;
            while (total < len) {
                ssize_t n = read(sock, buf.data() + total, static_cast<size_t>(len - total));
                if (n <= 0) break;
                total += n;
            }
            buf[total] = '\0';
            out_version_str = std::string(buf.data());
        }
        close(sock);
    }
    return true;
}

bool Detector::magisk_check_zygisk() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[1024]; bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "zygisk") || strstr(line, "Zygisk")) { found = true; break; }
    }
    fclose(f); return found;
}

bool Detector::magisk_check_su_binary(std::string& out_path) {
    static const char* su_paths[] = {
        "/system/bin/su", "/system/xbin/su", "/sbin/su",
        "/su/bin/su", "/magisk/.core/bin/su", nullptr
    };
    for (int i = 0; su_paths[i]; i++) {
        struct stat st;
        if (stat(su_paths[i], &st) == 0) { out_path = su_paths[i]; return true; }
    }
    return false;
}

bool Detector::magisk_check_module(const std::string& module_id) {
    std::string path = std::string(magisk::MAGISK_MODULES_DIR) + "/" + module_id;
    struct stat st; return stat(path.c_str(), &st) == 0;
}

bool Detector::magisk_check_kitsune() {
    struct stat st;
    return stat("/data/adb/magisk_delta", &st) == 0 ||
           stat("/data/adb/delta", &st) == 0 ||
           stat("/sbin/.magisk/config", &st) == 0;
}

bool Detector::magisk_check_alpha() {
    struct stat st;
    return stat("/data/adb/magisk_alpha", &st) == 0 ||
           stat("/data/adb/alpha", &st) == 0;
}

MagiskResult Detector::probe_magisk() {
    MagiskResult result;
    std::string sock_path;
    if (magisk_find_socket(sock_path)) {
        result.socket_path = sock_path;
        uint32_t ver_code = 0; std::string ver_str;
        if (magisk_probe_daemon(sock_path, ver_code, ver_str)) {
            result.present = true;
            result.priv_level = MagiskPrivLevel::DaemonOnly;
            result.version_code = ver_code;
            result.version_str = ver_str;
            if (getuid() == 0) result.priv_level = MagiskPrivLevel::Su;
        }
    }
    result.has_zygisk = magisk_check_zygisk();
    if (result.has_zygisk) { result.zygisk_detected = true; result.zygisk_detail = "maps-scan"; }
    std::string su_path;
    if (magisk_check_su_binary(su_path)) {
        result.su_binary_detected = true;
        result.su_binary_path = su_path;
    }
    if (result.present) {
        result.has_shamiko    = magisk_check_module("shamiko");
        result.has_lsposed    = magisk_check_module("lsposed") ||
                                magisk_check_module("zygisk_lsposed") ||
                                magisk_check_module("riru_lsposed");
        result.has_magiskhide = magisk_check_module("magiskhide");
        result.is_kitsune     = magisk_check_kitsune();
        result.is_alpha       = magisk_check_alpha();
        if (check_proc_susfs() || check_module_susfs()) result.has_susfs = true;
    }
    return result;
}

void Detector::probe_variants(DetectResult& out) {
    if (out.ksu.susfs_detected)       { out.susfs_detected = true; out.susfs_source = "kernelsu"; }
    else if (out.magisk.has_susfs)    { out.susfs_detected = true; out.susfs_source = "magisk"; }
    else if (check_proc_susfs() || check_module_susfs()) {
        out.susfs_detected = true; out.susfs_source = "kernel";
    }
}

void Detector::probe_jailbreak(DetectResult& out) {
    JailbreakHint& jb = out.jailbreak;
    FILE* fp = popen("getprop ro.debuggable 2>/dev/null", "r");
    if (fp) { char v[16]={0}; if (fgets(v, sizeof(v), fp) && atoi(v)==1) jb.indicators.push_back("ro.debuggable=1"); pclose(fp); }
    fp = popen("getprop ro.boot.verifiedbootstate 2>/dev/null", "r");
    if (fp) { char v[32]={0}; if (fgets(v, sizeof(v), fp) && (strstr(v,"orange")||strstr(v,"yellow"))) jb.indicators.push_back("verified_boot_state=orange/yellow"); pclose(fp); }
    fp = popen("getprop ro.build.type 2>/dev/null", "r");
    if (fp) { char v[32]={0}; if (fgets(v, sizeof(v), fp) && (strstr(v,"userdebug")||strstr(v,"eng"))) jb.indicators.push_back("build_type=userdebug/eng"); pclose(fp); }
    fp = popen("getprop ro.build.tags 2>/dev/null", "r");
    if (fp) { char v[64]={0}; if (fgets(v, sizeof(v), fp) && strstr(v,"test-keys")) jb.indicators.push_back("build_tags=test-keys"); pclose(fp); }
    fp = fopen("/sys/fs/selinux/enforce", "r");
    if (fp) { int e=1; if (fscanf(fp,"%d",&e)==1 && e==0) jb.indicators.push_back("selinux=permissive"); fclose(fp); }
    struct stat st;
    if (stat("/data/data/de.robv.android.xposed.installer",&st)==0 ||
        stat("/system/framework/XposedBridge.jar",&st)==0 ||
        stat("/system/framework/edxp",&st)==0)
        jb.indicators.push_back("xposed-framework");
    jb.detected = !jb.indicators.empty();
}

DetectResult Detector::run_all() {
    DetectResult result;
    result.ksu    = probe_ksu();
    result.ap     = probe_apatch();
    result.magisk = probe_magisk();
    probe_variants(result);
    probe_jailbreak(result);
    int count = 0;
    if (result.ksu.present) count++;
    if (result.ap.present)  count++;
    if (result.magisk.present) count++;
    if (count > 1) result.type = KernelType::Mixed;
    else if (result.ksu.present) result.type = KernelType::KernelSU;
    else if (result.ap.present)  result.type = KernelType::KernelPatch;
    else if (result.magisk.present) result.type = KernelType::Magisk;
    else result.type = KernelType::None;
    return result;
}

} // namespace ksu_detector

// ===========================================================================
//  JSON serialisation  (replaces the old printf-based output)
// ===========================================================================
static json result_to_json(const ksu_detector::DetectResult& r) {
    using namespace ksu_detector;
    json j;
    j["detected"] = kernel_type_str(r.type);

    // --- KernelSU ---
    {
        json k;
        k["present"] = r.ksu.present;
        if (r.ksu.present) {
            k["version"]        = r.ksu.version;
            k["uapi_version"]   = r.ksu.uapi_version;
            k["flags"]          = r.ksu.flags;
            k["features"]       = r.ksu.features;
            k["mode"]           = r.ksu.mode_str;
            k["priv_level"]     = ksu_priv_str(r.ksu.priv_level);
            if (r.ksu.manager_appid.has_value()) k["manager_appid"] = r.ksu.manager_appid.value();
            k["is_manager"]     = (r.ksu.flags & ksu::GET_INFO_FLAG_MANAGER) != 0;
            k["is_lkm"]         = (r.ksu.flags & ksu::GET_INFO_FLAG_LKM) != 0;
            k["is_late_load"]   = (r.ksu.flags & ksu::GET_INFO_FLAG_LATE_LOAD) != 0;
            k["is_gki"]         = has_ksu_variant(r.ksu.variant, KsuVariant::GKI);
            k["kernel_compromised"] = r.ksu.kernel_compromised;
            k["compromise_reason"]  = r.ksu.compromise_reason;
            k["has_susfs"]      = r.ksu.susfs_detected;
            k["susfs_detail"]   = r.ksu.susfs_detail;
        }
        j["kernelsu"] = k;
    }

    // --- APatch ---
    {
        json a;
        a["present"]     = r.ap.present;
        a["priv_level"]  = ap_priv_str(r.ap.priv_level);
        if (r.ap.present) {
            a["kp_version"]      = r.ap.kp_version;
            a["kernel_version"]  = r.ap.kernel_version;
            a["key_source"]      = r.ap.detected_key_source;
            if (r.ap.su_uid_count.has_value()) a["su_uid_count"] = r.ap.su_uid_count.value();
            if (r.ap.kpm_count.has_value())    a["kpm_count"]    = r.ap.kpm_count.value();
            if (r.ap.safemode.has_value())     a["safemode"]     = r.ap.safemode.value();
        }
        j["apatch"] = a;
    }

    // --- Magisk ---
    {
        json m;
        m["present"]      = r.magisk.present;
        m["priv_level"]   = magisk_priv_str(r.magisk.priv_level);
        m["version_code"] = r.magisk.version_code;
        m["version_str"]  = r.magisk.version_str;
        m["socket_path"]  = r.magisk.socket_path;
        m["has_zygisk"]      = r.magisk.has_zygisk;
        m["has_shamiko"]     = r.magisk.has_shamiko;
        m["has_susfs"]       = r.magisk.has_susfs;
        m["has_lsposed"]     = r.magisk.has_lsposed;
        m["has_magiskhide"]  = r.magisk.has_magiskhide;
        m["is_kitsune"]      = r.magisk.is_kitsune;
        m["is_alpha"]        = r.magisk.is_alpha;
        m["su_binary_detected"] = r.magisk.su_binary_detected;
        m["su_binary_path"]  = r.magisk.su_binary_path;
        j["magisk"] = m;
    }

    // --- Variants / Jailbreak ---
    j["variants"]["susfs_detected"] = r.susfs_detected;
    j["variants"]["susfs_source"]   = r.susfs_source;

    j["jailbreak"]["detected"] = r.jailbreak.detected;
    j["jailbreak"]["indicators"] = r.jailbreak.indicators;

    return j;
}

// ===========================================================================
//  HTTP server  (single POST route)
// ===========================================================================
int main() {
    httplib::Server svr;

    // ── POST /api/v1/detect ──────────────────────────────────────────
    // Request body (optional):  { "superkey": "..." }
    // Response:  JSON detection result
    svr.Post("/api/v1/detect", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "[POST /api/v1/detect] " << req.body << std::endl;

        // Parse optional superkey from request body
        std::string superkey;
        const json payload = json::parse(req.body, nullptr, false);
        if (payload.is_object()) {
            auto sk = payload.find("superkey");
            if (sk != payload.end() && sk->is_string()) {
                superkey = sk->get<std::string>();
            }
        }

        // Environment fallback
        if (superkey.empty()) {
            const char* env = getenv("AP_SUPERKEY");
            if (env) superkey = env;
        }

        // Run detection
        ksu_detector::Detector detector;
        if (!superkey.empty()) detector.set_ap_superkey(superkey);
        ksu_detector::DetectResult result = detector.run_all();

        // Return JSON
        json response = result_to_json(result);
        res.set_content(response.dump(2), "application/json");
    });

    // ── Error handler ─────────────────────────────────────────────────
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            res.set_content(json{{"error", "not found"}, {"hint", "use POST /api/v1/detect"}}.dump(),
                            "application/json");
        }
    });

    // ── Start server ──────────────────────────────────────────────────
    std::string host = "0.0.0.0";
    int port = 8080;
    std::cout << "local_api listening on http://" << host << ":" << port << std::endl;
    std::cout << "Endpoint: POST /api/v1/detect" << std::endl;
    svr.listen(host.c_str(), port);

    return 0;
}
