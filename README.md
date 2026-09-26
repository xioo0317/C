# Local API

C++17 本地 HTTP 服务，集成 [ksu-detect](https://github.com/xioo0317/ksu-detect) 的 Root 检测能力（KernelSU / APatch / Magisk / SusFS），基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 构建。

## 功能概述

- **网卡与检测分离**：`src/main.cpp` 只负责启动 HTTP 网卡；所有检测逻辑在 `src/detector.cpp`
- **单路由**：`POST /api/v1/detect` — 一键检测 KernelSU / APatch / Magisk / SusFS
- **落盘而非打印**：服务全程不向终端输出任何内容；检测结果以 JSON 写入 `/data/local/tmp/coverRoot/root_detect.json`，供其它程序读取
- **真实握手协议**：复刻各官方管理器的内核握手，而非简单文件探测；SusFS 无兜底（接口不应答就是没有）
- **精简输出**：JSON 仅报告各 Root 管理器是否存在，KernelSU 额外报告运行模式

## 项目结构

```
./
├── Makefile                     # 主机 + NDK 构建脚本
├── src/
│   ├── main.cpp                 # 仅启动 HTTP 网卡 + 路由，写结果文件
│   └── detector.cpp             # 全部检测逻辑 + JSON 序列化
├── include/
│   ├── detector.hpp             # DetectResult / Detector 声明
│   ├── ksu_uapi.hpp             # KernelSU 用户态 API
│   ├── apatch_uapi.hpp          # APatch / KernelPatch supercall
│   ├── magisk_uapi.hpp          # Magisk daemon 协议
│   └── susfs_uapi.hpp           # SusFS 内核握手 ABI
├── third_party/
│   ├── httplib.h                # cpp-httplib 单头库
│   └── nlohmann/json.hpp        # nlohmann/json 单头库
├── jni/
│   ├── Android.mk               # ndk-build 脚本（只编译 main + detector）
│   └── Application.mk           # arm64-v8a / c++_static / android-24
├── test_api.sh                  # API 测试脚本
└── README.md
```

## 构建

### 主机编译（Linux 桌面测试）

```bash
make
# 产出: build/local_api
```

### Android NDK（ndk-build，与 CI 一致）

```bash
$NDK/ndk-build NDK_PROJECT_PATH=. \
  APP_BUILD_SCRIPT=jni/Android.mk \
  NDK_APPLICATION_MK=jni/Application.mk
# 产出: libs/arm64-v8a/local_api
```

也可以用 Makefile 交叉编译：

```bash
make ndk NDK_HOME=/path/to/android-ndk-rXX
```

### 清理

```bash
make clean
```

## 运行

```bash
# Android
adb push libs/arm64-v8a/local_api /data/local/tmp/local_api
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

服务默认监听 `0.0.0.0:8080`，启动后静默运行。

## API

### POST /api/v1/detect — 执行 Root 检测

无需请求体。服务运行完整检测，把结果写入 `/data/local/tmp/coverRoot/root_detect.json`，HTTP 仅返回状态：

```json
{
  "status": "ok",
  "result_file": "/data/local/tmp/coverRoot/root_detect.json"
}
```

### 结果文件示例（root_detect.json）

```json
{
  "detected": "kernelsu",
  "kernelsu": {
    "present": true,
    "mode": "lkm-bundled"
  },
  "apatch": {
    "present": false
  },
  "magisk": {
    "present": false
  },
  "susfs": {
    "present": true
  }
}
```

### 字段说明

| 字段 | 说明 |
|------|------|
| `detected` | 主 Root 方案：`none` / `kernelsu` / `kernelpatch` / `magisk` / `mixed` |
| `kernelsu.present` | KernelSU 是否正在运行 |
| `kernelsu.mode` | KernelSU 运行模式：`lkm-bundled` / `lkm` / `built-in` / `late-load` / `legacy-prctl` |
| `apatch.present` | APatch 是否正在运行 |
| `magisk.present` | Magisk 守护进程是否正在运行 |
| `susfs.present` | SusFS 内核接口是否握手成功 |

### 脚本读取示例

```bash
# 触发检测
curl -s -X POST http://localhost:8080/api/v1/detect

# 从落盘文件读取当前 Root 方案
jq -r '.detected' /data/local/tmp/coverRoot/root_detect.json

# 检查 KernelSU 模式
jq -r '.kernelsu.mode' /data/local/tmp/coverRoot/root_detect.json

# 检查 SusFS 是否存在
jq -r '.susfs.present' /data/local/tmp/coverRoot/root_detect.json
```

## 测试

```bash
./test_api.sh
./test_api.sh 192.168.1.100 8080
```

## 技术说明

- **KernelSU 检测**：reboot syscall hook（`0xDEADBEEF / 0xCAFEBABE`）取得驱动 fd，ioctl `GET_INFO` 获取运行模式
- **APatch 检测**：经 supercall（syscall #45，伪装 truncate）用固定 key `su` 发 `SUPERCALL_HELLO`，回报 `0x11581158` 即存在；无需超级密钥
- **Magisk 检测**：Unix domain socket 与 magiskd 握手，支持 `/debug_ramdisk/.magisk/device/socket` 等文件系统 socket 路径
- **SusFS 检测**：两阶段握手——reboot（第二魔术 `0xFAFAFAFA`，`SHOW_VERSION`）识别 v2.0.0+，prctl（`0xDEADBEEF`）识别 v1.5.3–v1.5.12；任一阶段成功才确认，**无兜底**

## 许可证

本项目集成代码遵循 GPL-3.0-or-later 许可证。
