# Local API

C++17 本地 HTTP 服务，集成 Root 检测能力（KernelSU / APatch / Magisk / SusFS），基于 [cpp-httplib](https://github.com/yhirose/cpp-httplib) 构建。

## 功能概述

- **网卡与检测分离**：`main.cpp` 只负责启动 HTTP 服务和路由分发；所有业务逻辑在 `detector.cpp`
- **极简路由**：`POST /` — 执行 Root 检测；`GET /version` — 前端检查版本更新
- **无路径依赖**：客户端只需 POST 到本地地址即可，无需拼接路径
- **落盘而非打印**：服务全程不向终端输出任何内容；检测结果以 JSON 写入 `/data/local/tmp/coverRoot/root_detect.json`
- **真实握手协议**：复刻各官方管理器的内核握手，而非简单文件探测
- **版本管理**：前端 UI 可通过 `GET /version` 检查服务器版本，判断是否需要更新

## 项目结构

```
./
├── Makefile                     # 主机 + NDK 构建脚本
├── src/
│   ├── main.cpp                 # 仅 HTTP 网卡 + 路由分发
│   └── detector.cpp             # 检测逻辑 + 文件写入 + 响应构建
├── include/
│   ├── version.hpp              # 版本号 + 配置常量 + 版本信息接口
│   ├── detector.hpp             # DetectResult / Detector / handle_detect 声明
│   ├── ksu_uapi.hpp             # KernelSU 用户态 API
│   ├── apatch_uapi.hpp          # APatch / KernelPatch supercall
│   ├── magisk_uapi.hpp          # Magisk daemon 协议
│   └── susfs_uapi.hpp           # SusFS 内核握手 ABI
├── third_party/
│   ├── httplib.h                # cpp-httplib 单头库
│   └── nlohmann/json.hpp        # nlohmann/json 单头库
├── jni/
│   ├── Android.mk               # ndk-build 脚本
│   └── Application.mk           # arm64-v8a / c++_static / android-24
├── test_api.sh                  # API 测试脚本
└── README.md
```

## 版本管理

版本号定义在 `include/version.hpp`：

| 常量 | 当前值 | 说明 |
|------|--------|------|
| `SERVER_VERSION` | `1.1.0` | 服务器版本，发版时递增 |
| `API_VERSION` | `2.0` | API 协议版本，接口格式变更时递增 |

前端 UI 调用 `GET /version` 获取版本号，与服务端对比判断是否需要更新。

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
make ndk NDK_HOME=/path/to/ndk
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

### POST / — 执行 Root 检测

无需请求体，客户端直接 POST 到本地地址即可：

```bash
curl -X POST http://localhost:8080/
```

HTTP 响应：

```json
{
  "status": "ok",
  "result_file": "/data/local/tmp/coverRoot/root_detect.json"
}
```

### GET /version — 版本信息

前端 UI 用于检查服务端版本，判断是否需要更新：

```bash
curl http://localhost:8080/version
```

响应：

```json
{
  "server_version": "1.1.0",
  "api_version": "2.0",
  "app_name": "Local-api"
}
```

### 检测结果文件（root_detect.json）

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
| `kernelsu.mode` | 运行模式：`lkm-bundled` / `lkm` / `built-in` / `late-load` / `legacy-prctl` |
| `apatch.present` | APatch 是否正在运行 |
| `magisk.present` | Magisk 守护进程是否正在运行 |
| `susfs.present` | SusFS 内核接口是否握手成功 |

### 脚本读取示例

```bash
# 触发检测
curl -s -X POST http://localhost:8080/

# 从落盘文件读取当前 Root 方案
jq -r '.detected' /data/local/tmp/coverRoot/root_detect.json

# 检查 KernelSU 模式
jq -r '.kernelsu.mode' /data/local/tmp/coverRoot/root_detect.json

# 检查服务端版本
curl -s http://localhost:8080/version | jq -r '.server_version'
```

## 测试

```bash
./test_api.sh
./test_api.sh 192.168.1.100 8080
```

## 技术说明

- **KernelSU 检测**：reboot syscall hook 取得驱动 fd，ioctl `GET_INFO` 获取运行模式
- **APatch 检测**：supercall 用固定 key `su` 发 `SUPERCALL_HELLO`，回报 magic 即存在
- **Magisk 检测**：Unix domain socket 与 magiskd 握手
- **SusFS 检测**：两阶段握手——reboot 识别 v2.0.0+，prctl 识别 v1.5.3–v1.5.12

## 许可证

本项目集成代码遵循 GPL-3.0-or-later 许可证。
