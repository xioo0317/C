// SPDX-License-Identifier: GPL-3.0-or-later
//
// APatch / KernelPatch supercall UAPI definitions.
//
// Everything is done through syscall(__NR_supercall=45, key, ver_and_cmd, ...).
// The kernel hooks the `truncate` syscall entry and intercepts calls matching
// the 0x1158 magic in bits [31:16] of the second argument.
//
// Authentication is via the superkey string. If the caller's UID is on the
// "su allowed list" AND the key is "su", the call is also accepted.
//
#pragma once

#include <cstdint>
#include <sys/types.h>
#include <unistd.h>

namespace apatch {

constexpr long NR_SUPERCALL = 45;  // __NR_truncate on arm64/arm/x86

constexpr uint32_t HELLO_MAGIC = 0x11581158u;

constexpr uint16_t SUPERCALL_HELLO              = 0x1000;
constexpr uint16_t SUPERCALL_KLOG               = 0x1004;
constexpr uint16_t SUPERCALL_BUILD_TIME         = 0x1007;
constexpr uint16_t SUPERCALL_KERNELPATCH_VER    = 0x1008;
constexpr uint16_t SUPERCALL_KERNEL_VER         = 0x1009;
constexpr uint16_t SUPERCALL_SKEY_GET           = 0x100a;
constexpr uint16_t SUPERCALL_SKEY_SET           = 0x100b;
constexpr uint16_t SUPERCALL_SKEY_ROOT_ENABLE   = 0x100c;
constexpr uint16_t SUPERCALL_SU                 = 0x1010;
constexpr uint16_t SUPERCALL_SU_TASK            = 0x1011;
constexpr uint16_t SUPERCALL_KPM_LOAD          = 0x1020;
constexpr uint16_t SUPERCALL_KPM_UNLOAD        = 0x1021;
constexpr uint16_t SUPERCALL_KPM_CONTROL       = 0x1022;
constexpr uint16_t SUPERCALL_KPM_NUMS          = 0x1030;
constexpr uint16_t SUPERCALL_KPM_LIST          = 0x1031;
constexpr uint16_t SUPERCALL_KPM_INFO          = 0x1032;
constexpr uint16_t SUPERCALL_KSTORAGE_WRITE    = 0x1041;
constexpr uint16_t SUPERCALL_KSTORAGE_READ     = 0x1042;
constexpr uint16_t SUPERCALL_KSTORAGE_LIST_IDS = 0x1043;
constexpr uint16_t SUPERCALL_KSTORAGE_REMOVE   = 0x1044;
constexpr uint16_t SUPERCALL_CONTROL_FEATURE   = 0x1046;
constexpr uint16_t SUPERCALL_BOOTLOG           = 0x10fd;
constexpr uint16_t SUPERCALL_PANIC             = 0x10fe;
constexpr uint16_t SUPERCALL_TEST              = 0x10ff;
constexpr uint16_t SUPERCALL_SU_GRANT_UID      = 0x1100;
constexpr uint16_t SUPERCALL_SU_REVOKE_UID     = 0x1101;
constexpr uint16_t SUPERCALL_SU_NUMS           = 0x1102;
constexpr uint16_t SUPERCALL_SU_LIST           = 0x1103;
constexpr uint16_t SUPERCALL_SU_PROFILE        = 0x1104;
constexpr uint16_t SUPERCALL_SU_GET_ALLOW_SCTX = 0x1105;
constexpr uint16_t SUPERCALL_SU_SET_ALLOW_SCTX = 0x1106;
constexpr uint16_t SUPERCALL_SU_GET_PATH       = 0x1110;
constexpr uint16_t SUPERCALL_SU_RESET_PATH     = 0x1111;
constexpr uint16_t SUPERCALL_SU_GET_SAFEMODE   = 0x1112;
constexpr uint16_t SUPERCALL_MAX               = 0x1200;

constexpr size_t KEY_MAX_LEN      = 0x40;
constexpr size_t SCONTEXT_LEN     = 0x60;
constexpr size_t SU_PATH_MAX_LEN  = 128;

constexpr int KSTORAGE_SU_LIST_GROUP      = 0;
constexpr int KSTORAGE_EXCLUDE_LIST_GROUP = 1;

constexpr const char* APD_PATH       = "/data/adb/apd";
constexpr const char* AP_DIR         = "/data/adb/ap";
constexpr const char* SUPERKEY_PATH  = "/data/adb/ap/superkey";
constexpr const char* SU_PATH_FILE   = "/data/adb/ap/su_path";

struct su_profile {
    uid_t  uid;
    uid_t  to_uid;
    char   scontext[SCONTEXT_LEN];
};

static inline uint64_t make_ver_and_cmd(uint32_t version_code, uint16_t cmd) {
    return (static_cast<uint64_t>(version_code) << 32) |
           (static_cast<uint64_t>(0x1158) << 16) |
           static_cast<uint64_t>(cmd);
}

static inline long hash_key(const char* key) {
    long hash = 1000000007;
    for (int i = 0; key[i]; i++) {
        hash = hash * 31 + key[i];
    }
    return hash;
}

enum class PrivLevel {
    None,
    Unconfirmed,
    SuList,
    SuperKey,
};

} // namespace apatch
