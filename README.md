# Local API

C++17 本地 HTTP API 服务器，集成了 [ksu-detect](https://github.com/xioo0317/ksu-detect) 的 Root 检测能力。基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 构建。

## 功能概述

- **单文件架构**：所有逻辑（HTTP 服务器 + Root 检测）集中在 `src/main.cpp` 一个文件中
- **单路由**：`POST /api/v1/detect` — 一键检测 KernelSU / APatch / Magisk
- **JSON 输出**：使用 [nlohmann/json](https://github.com/nlohmann/json) 序列化检测结果，方便脚本解析当前设备使用的是哪个面具
- **双构建模式**：支持主机直接编译（g++）和 Android NDK 交叉编译

## 项目结构

```
./
├── Makefile              # 构建脚本（主机 + NDK）
├── src/
│   └── main.cpp          # 全部代码：HTTP 服务器 + ksu-detect 检测逻辑
├── include/
│   ├── detector.hpp      # （已合并到 main.cpp，保留供参考）
│   ├── ksu_uapi.hpp      # KernelSU 用户态 API 定义
│   ├── apatch_uapi.hpp   # APatch / KernelPatch supercall 定义
│   └── magisk_uapi.hpp   # Magisk daemon 协议定义
├── include/server/       # 旧的头文件（已弃用，可删除）
├── third_party/
│   ├── httplib.h         # cpp-httplib 单头文件 HTTP 库
│   └── json.hpp          # nlohmann/json 单头文件 JSON 库
├── jni/                  # 旧的 NDK 构建配置（已弃用）
├── test_api.sh           # API 测试脚本
└── README.md
```

## 构建

### 主机编译（Linux / 桌面测试）

```bash
make
# 产出: build/local_api
```

### Android NDK 交叉编译

```bash
make ndk NDK_HOME=/path/to/android-ndk-rXX
# 默认 arm64-v8a

make ndk NDK_HOME=/path/to/android-ndk-rXX ABI=armeabi-v7a

make all-abis NDK_HOME=/path/to/android-ndk-rXX
# 编译全部架构
```

### 清理

```bash
make clean
```

## 运行

```bash
# 主机
./build/local_api

# Android
adb push build/local_api_arm64-v8a /data/local/tmp/local_api
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

服务默认监听 `0.0.0.0:8080`。

## API

### POST /api/v1/detect — Root 检测

**请求体**（可选）：

```json
{
  "superkey": "你的APatch超级密钥"
}
```

也可以通过环境变量传入：`AP_SUPERKEY=<key>`

**响应示例**：

```json
{
  "detected": "magisk",
  "kernelsu": {
    "present": false
  },
  "apatch": {
    "present": false,
    "priv_level": "none"
  },
  "magisk": {
    "present": true,
    "priv_level": "daemon_only",
    "version_code": 27000,
    "version_str": "27.0",
    "socket_path": "/debug_ramdisk/.magisk/device/socket",
    "has_zygisk": true,
    "has_shamiko": false,
    "has_susfs": false,
    "has_lsposed": false,
    "has_magiskhide": false,
    "is_kitsune": false,
    "is_alpha": false,
    "su_binary_detected": true,
    "su_binary_path": "/system/bin/su"
  },
  "variants": {
    "susfs_detected": false,
    "susfs_source": ""
  },
  "jailbreak": {
    "detected": true,
    "indicators": [
      "ro.debuggable=1",
      "build_tags=test-keys"
    ]
  }
}
```

### 关键字段说明

| 字段 | 说明 |
|------|------|
| `detected` | 检测到的 Root 方案：`none` / `kernelsu` / `kernelpatch` / `magisk` / `mixed` |
| `kernelsu.present` | KernelSU 是否正在运行 |
| `apatch.present` | APatch 是否正在运行 |
| `magisk.present` | Magisk 守护进程是否正在运行 |
| `magisk.is_kitsune` | 是否为 Kitsune（Delta 版）Magisk |
| `magisk.is_alpha` | 是否为 Magisk Alpha 版 |
| `magisk.has_zygisk` | Zygisk 是否启用 |
| `magisk.has_shamiko` | Shamiko 模块是否安装 |
| `magisk.has_susfs` | SusFS 是否检测到 |
| `variants.susfs_detected` | 全局 SusFS 检测 |

### 脚本解析示例

```bash
# 检测当前设备使用的 Root 方案
curl -s -X POST http://localhost:8080/api/v1/detect | jq -r '.detected'

# 检查是否为 Magisk
curl -s -X POST http://localhost:8080/api/v1/detect | jq '.magisk.present'

# 获取 Magisk 版本
curl -s -X POST http://localhost:8080/api/v1/detect | jq -r '.magisk.version_str'

# 检查是否为 Kitsune/Delta
curl -s -X POST http://localhost:8080/api/v1/detect | jq '.magisk.is_kitsune'
```

## 测试

```bash
./test_api.sh
./test_api.sh 192.168.1.100 8080
```

## 技术说明

- **KernelSU 检测**：通过 reboot syscall hook 获取驱动 fd，调用 IOCTL_GET_INFO 获取详细信息，支持 legacy prctl 回退
- **APatch 检测**：通过 supercall (syscall #45) 进行 hello 探测，支持超级密钥和 su 列表两种认证路径
- **Magisk 检测**：通过 Unix domain socket 与 magiskd 守护进程握手，支持 Magisk 31.x 的文件系统 socket 路径
- **SusFS 检测**：检查 /proc/sys/kernel/susfs_* 和 /sys/module/susfs
- **Jailbreak 指标**：ro.debuggable、verified boot state、SELinux 状态、Xposed 框架等

## 许可证

本项目集成代码遵循 GPL-3.0-or-later 许可证。
