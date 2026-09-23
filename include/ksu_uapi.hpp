// SPDX-License-Identifier: GPL-3.0-or-later
//
// KernelSU userspace API - aligned with uapi/supercall.h from the main repo.
//
// The handshake between KernelSU kernel module and its manager works like this:
//
//   1. Kernel hooks `__NR_reboot` via kprobe (reboot_handler_pre in supercall.c).
//      When userspace calls reboot(MAGIC1=0xDEADBEEF, MAGIC2=0xCAFEBABE, ...),
//      the handler queues a task_work that installs an anonymous inode fd named
//      "[ksu_driver]" into the current process.
//
//   2. The manager (KernelSU app) is UID-whitelisted by the kernel side
//      "throne tracker". When that UID goes through setresuid (zygote fork),
//      the kernel calls ksu_install_fd() directly.
//
//   3. On the installed fd, userspace can call ioctl() commands.
//      KSU_IOCTL_GET_INFO returns KSU_GET_INFO_FLAG_MANAGER only when the
//      caller's UID matches the manager app id.
//
//   4. Legacy (older KernelSU) used prctl(KSU_LEGACY_MAGIC, ...) as the probe.
//
#pragma once

#include <cstdint>
#include <linux/ioctl.h>

namespace ksu {

// --- Magic numbers for fd installation via reboot syscall hook ---
constexpr uint32_t INSTALL_MAGIC1 = 0xDEADBEEF;
constexpr uint32_t INSTALL_MAGIC2 = 0xCAFEBABE;

// --- Legacy prctl magic (older KernelSU versions) ---
constexpr uint32_t LEGACY_MAGIC = 0x4B535500u;  // "KSU\0"

// --- Feature / flag bits returned in get_info_cmd::flags ---
constexpr uint32_t GET_INFO_FLAG_LKM        = (1u << 0);
constexpr uint32_t GET_INFO_FLAG_MANAGER    = (1u << 1);
constexpr uint32_t GET_INFO_FLAG_LATE_LOAD  = (1u << 2);
constexpr uint32_t GET_INFO_FLAG_PR_BUILD   = (1u << 3);
constexpr uint32_t GET_INFO_FLAG_BUNDLED    = (1u << 4);

// --- UAPI version ---
constexpr uint32_t UAPI_VERSION = 4;

// --- IOCTL command structures ---
struct get_info_cmd {
    uint32_t version;
    uint32_t flags;
    uint32_t features;
    uint32_t uapi_version;
};

struct get_manager_appid_cmd {
    uint32_t appid;
};

// --- IOCTL command opcodes ---
constexpr uint32_t IOCTL_GRANT_ROOT        = _IOC(_IOC_NONE, 'K', 1, 0);
constexpr uint32_t IOCTL_GET_INFO          = _IOR('K', 2, struct get_info_cmd);
constexpr uint32_t IOCTL_GET_INFO_LEGACY   = _IOC(_IOC_READ, 'K', 2, 0);
constexpr uint32_t IOCTL_REPORT_EVENT      = _IOC(_IOC_WRITE, 'K', 3, 0);
constexpr uint32_t IOCTL_SET_SEPOLICY      = _IOC(_IOC_READ | _IOC_WRITE, 'K', 4, 0);
constexpr uint32_t IOCTL_CHECK_SAFEMODE    = _IOC(_IOC_READ, 'K', 5, 0);
constexpr uint32_t IOCTL_GET_MANAGER_APPID = _IOC(_IOC_READ, 'K', 10, 0);

enum class PermClass {
    AlwaysAllow,
    ManagerOrRoot,
    OnlyManager,
    OnlyRoot,
};

} // namespace ksu
