# Local API

C++17 本地 HTTP 服务，基于请求体 action 字段分发工具调用，集成 Root 检测（KernelSU / APatch / Magisk / SusFS）。

## 架构

```
main.cpp      → HTTP 网卡（只启动服务、绑定路由）
router.cpp    → 请求分发（解析 action，路由到对应工具函数）
tools.cpp     → 工具函数集合（detect / version / debug / 占位工具）
detector.cpp  → 纯检测库（KSU / APatch / Magisk / SusFS 握手）
version.hpp   → 版本号 + 配置常量
```

客户端只需 `POST /`，请求体 `{"action": "..."}` 决定调用哪个工具。

## 项目结构

```
./
├── Makefile                     # 主机 + NDK 构建脚本
├── src/
│   ├── main.cpp                 # 仅 HTTP 网卡（服务绑定）
│   ├── router.cpp               # 请求分发（action → tool）
│   ├── tools.cpp                # 工具函数集合
│   └── detector.cpp             # 纯检测库
├── include/
│   ├── version.hpp              # 版本号 + 配置
│   ├── router.hpp               # 路由分发接口
│   ├── tools.hpp                # 工具函数注册表
│   ├── detector.hpp             # 检测库接口
│   ├── ksu_uapi.hpp             # KernelSU 用户态 API
│   ├── apatch_uapi.hpp          # APatch supercall
│   ├── magisk_uapi.hpp          # Magisk daemon 协议
│   └── susfs_uapi.hpp           # SusFS 握手 ABI
├── third_party/
│   ├── httplib.h                # cpp-httplib
│   └── nlohmann/json.hpp        # nlohmann/json
├── jni/                         # NDK 构建配置
├── test_api.sh                  # 测试脚本
└── README.md
```

## API

### POST / — 所有请求入口

请求体 JSON，`action` 字段决定调用哪个工具：

#### action: detect — Root 检测

```bash
curl -X POST http://localhost:8080/ -H "Content-Type: application/json" -d '{"action":"detect"}'
```

```json
{
  "status": "ok",
  "result_file": "/data/local/tmp/coverRoot/root_detect.json",
  "result": {
    "detected": "kernelsu",
    "kernelsu": {"present": true, "mode": "lkm-bundled"},
    "apatch": {"present": false},
    "magisk": {"present": false},
    "susfs": {"present": true}
  }
}
```

#### action: version — 版本信息

```bash
curl -X POST http://localhost:8080/ -d '{"action":"version"}'
```

```json
{"status":"ok","server_version":"1.1.0","api_version":"2.0","app_name":"Local-api"}
```

#### action: debug — 调试信息 + 工具试运行

```bash
curl -X POST http://localhost:8080/ -d '{"action":"debug"}'
```

返回所有工具列表、版本、状态、以及检测试运行结果。

### GET /debug — 调试端点

等同 `action: debug`，方便浏览器直接访问：

```bash
curl http://localhost:8080/debug
```

### 已注册工具

| action | 状态 | 说明 |
|--------|------|------|
| `detect` | ✅ 已实现 | Root 检测（KSU/APatch/Magisk/SusFS 握手） |
| `version` | ✅ 已实现 | 服务器版本 + API 版本 |
| `debug` | ✅ 已实现 | 工具信息 + 检测试运行 + 环境信息 |
| `sysinfo` | 🔲 占位 | 系统信息（设备型号、Android 版本、内核等） |
| `modules` | 🔲 占位 | 模块管理（列表、启用/禁用） |
| `config` | 🔲 占位 | 配置读写 |

### 未知 action

```json
{"status":"error","error":"unknown action: xxx","available_actions":["detect","version","debug","sysinfo","modules","config"]}
```

## 构建

```bash
# 主机编译
make

# Android NDK
make ndk NDK_HOME=/path/to/ndk

# 清理
make clean
```

## 运行

```bash
adb push build/local_api_arm64-v8a /data/local/tmp/local_api
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

## 版本管理

| 常量 | 当前值 | 说明 |
|------|--------|------|
| `SERVER_VERSION` | `1.1.0` | 发版递增 |
| `API_VERSION` | `2.0` | 接口格式变更递增 |

前端调用 `{"action":"version"}` 检查是否需要更新。

## 许可证

GPL-3.0-or-later
