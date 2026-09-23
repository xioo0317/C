// SPDX-License-Identifier: GPL-3.0-or-later
//
// Magisk userspace API - manager-level handshake detection.
//
// Magisk runs a userspace daemon (magiskd) that listens on a filesystem
// Unix socket. The daemon protocol uses plain int32 codes.
//
#pragma once

#include <cstdint>
#include <cstddef>

namespace magisk {

// Preferred filesystem socket paths (Magisk 31.x)
constexpr const char* DSOCKET_PATH     = "/debug_ramdisk/.magisk/device/socket";
constexpr const char* SBIN_SOCKET_PATH = "/sbin/.magisk/device/socket";
constexpr const char* LEGACY_SOCKET_PATH = "/dev/socket/magiskd";
constexpr const char* SOCKET_DIR_MARKER = ".magisk/device/socket";

// Filesystem hint paths
constexpr const char* MAGISK_SBIN_DIR         = "/sbin/.magisk";
constexpr const char* MAGISK_DATA_ADB_DIR     = "/data/adb/magisk";
constexpr const char* MAGISK_DB_PATH          = "/data/adb/magisk.db";
constexpr const char* MAGISK_MODULES_DIR      = "/data/adb/modules";
constexpr const char* MAGISK_MODULES_UPDATE_DIR = "/data/adb/modules_update";
constexpr const char* MAGISK_POST_FS_DATA     = "/data/adb/post-fs-data.d";
constexpr const char* MAGISK_SERVICE_D        = "/data/adb/service.d";

constexpr const char* SU_VERSION_MAGIC = "MAGISK";
constexpr const char* ZYGISK_LIB_NAME  = "zygisk";
constexpr const char* ZYGISK_PROPERTY  = "ro.zygisk";

constexpr const char* SUSFS_PROC_PREFIX = "/proc/sys/kernel/susfs_";
constexpr const char* SUSFS_MODULE_DIR  = "/sys/module/susfs";
constexpr const char* SUSFS_KSU_MARKER  = "/data/adb/ksu/modules/susfs";

enum DaemonRequestCode : uint32_t {
    START_DAEMON       = 0,
    CHECK_VERSION      = 1,
    CHECK_VERSION_CODE = 2,
    STOP_DAEMON        = 3,
    SUPERUSER          = 5,
};

enum DaemonRespondCode : int32_t {
    RESP_ERROR         = -1,
    RESP_OK            = 0,
    RESP_ROOT_REQUIRED = 1,
    RESP_ACCESS_DENIED = 2,
};

static inline int version_major(uint32_t ver_code) {
    return static_cast<int>(ver_code / 1000);
}
static inline int version_minor(uint32_t ver_code) {
    return static_cast<int>((ver_code % 1000) / 10);
}

enum class PrivLevel {
    None,
    Unconfirmed,
    DaemonOnly,
    Su,
    Manager,
};

enum class Variant : uint32_t {
    Standard     = 0,
    Zygisk       = (1u << 0),
    Shamiko      = (1u << 1),
    SusFS        = (1u << 2),
    LSPosed      = (1u << 3),
    MagiskHide   = (1u << 4),
    Kitsune      = (1u << 5),
    Alpha        = (1u << 6),
    LSPosedLite  = (1u << 7),
};

inline Variant operator|(Variant a, Variant b) {
    return static_cast<Variant>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline Variant operator&(Variant a, Variant b) {
    return static_cast<Variant>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_variant(Variant v, Variant flag) {
    return (static_cast<uint32_t>(v) & static_cast<uint32_t>(flag)) != 0;
}

} // namespace magisk
