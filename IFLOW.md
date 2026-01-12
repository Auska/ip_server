# IP Geolocation & AS Lookup Service

## 项目概述

这是一个基于 C++20 开发的高性能 IP 地理位置和 AS（自治系统）信息查询服务端。项目采用现代 C++ 设计模式和行业最佳实践，提供 RESTful API 接口，支持单个 IP 查询和批量查询。

**新增功能**: 现在支持 MAC 地址 OUI（组织唯一标识符）查询功能！

### 核心技术栈

- **编程语言**: C++20
- **构建系统**: CMake 3.20+
- **HTTP 服务器**: cpp-httplib
- **数据库**: 
  - MaxMind GeoLite2 (City + ASN)
  - SQLite3 (OUI 数据库)
- **JSON 处理**: nlohmann/json
- **加密**: OpenSSL（可选，已从构建系统移除）

### 架构设计

项目采用分层架构，遵循 SOLID 原则：

```
┌─────────────────────────────────────┐
│   HTTP Layer (IPGeoHTTPServer)      │  RESTful API 接口
├─────────────────────────────────────┤
│   Service Layer                     │  业务逻辑层
│   ├── IPGeoService                  │  IP 查询服务
│   └── MACLookupService              │  MAC 查询服务
├─────────────────────────────────────┤
│   Cache Layer (LRU Cache)           │  缓存层
├─────────────────────────────────────┤
│   Data Layer                        │  数据访问层
│   ├── City/ASN Database             │  MaxMind 数据库
│   └── OUIDatabase                   │  OUI 数据库
├─────────────────────────────────────┤
│   Database Libraries                │  底层数据库
│   ├── MaxMind DB Library            │
│   └── SQLite3                       │
└─────────────────────────────────────┘
```

### 主要模块

- **config.h/cpp**: 配置管理，命令行参数解析，XDG 目录标准支持
- **database.h/cpp**: 数据库抽象层，支持 City、ASN 和 OUI 数据库
- **mac_database.h/cpp**: OUI 数据库实现（SQLite3）
- **http_server.h/cpp**: HTTP 服务器，路由处理，速率限制，API 认证
- **logger.h/cpp**: 线程安全的日志系统，支持文件日志轮转
- **types.h/cpp**: 数据类型定义
- **cache.h**: LRU 缓存实现
- **rate_limiter.h/cpp**: 速率限制器
- **auth.h/cpp**: API 认证模块
- **metrics.h/cpp**: 性能指标收集
- **xdg.h/cpp**: XDG 目录标准实现
- **main.cpp**: 应用程序入口

### 功能特性

- ✅ **IP 地理位置查询**: 国家、城市、经纬度、时区等信息
- ✅ **AS 信息查询**: 自治系统编号和组织名称
- ✅ **MAC 地址 OUI 查询**: 制造商、注册机构等信息（新增）
- ✅ **日志文件轮转**: 支持按大小、按时间、混合轮转
- ✅ **速率限制**: 防止 API 滥用
- ✅ **API 认证**: 基于 API 密钥的身份验证
- ✅ **性能指标**: 实时监控查询性能
- ✅ **优雅关闭**: 支持 SIGINT/SIGTERM 信号处理
- ✅ **XDG 目录标准**: 遵循 Linux 桌面环境规范
- ✅ **批量查询限制**: 防止过大的批量请求
- ✅ **源 IP 查询**: 支持查询客户端源 IP 地址

## 构建和运行

### 环境要求

- C++20 兼容的编译器 (GCC 10+, Clang 10+, MSVC 19.28+)
- CMake 3.20+
- pthread
- Google Test (用于单元测试)
- Google Benchmark (用于性能测试，可选)

### 构建步骤

```bash
# 创建构建目录
mkdir build
cd build

# Debug 模式（默认，无优化）
cmake ..
cmake --build . -j$(nproc)

# Release 模式（-O2 优化，推荐生产环境）
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 可执行文件位置
# build/bin/ip_server
```

**性能对比**:
- Debug 模式: 启用缓存 ~13.5μs (75k QPS)
- Release 模式: 启用缓存 ~1.56μs (647k QPS) - **性能提升约 8.6 倍**

### 运行测试

```bash
# 运行所有单元测试
./build/tests/ip_server_tests

# 运行特定测试套件
./build/tests/ip_server_tests --gtest_filter="LoggerTest.*"

# 运行基准测试
cmake .. -DBUILD_BENCHMARKS=ON
cmake --build . -j$(nproc)
./build/bin/ip_server_benchmarks

# 使用便捷脚本运行所有基准测试
./tests/run_benchmarks.sh
```

详细基准测试文档请参考 [tests/BENCHMARK.md](tests/BENCHMARK.md)

### 运行服务

```bash
# 使用默认配置运行（使用 XDG 目录标准）
./build/bin/ip_server

# 自定义端口
./build/bin/ip_server --port 9000

# 自定义数据库路径
./build/bin/ip_server --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb --oui-db /path/to/master_oui.db

# 自定义监听地址
./build/bin/ip_server --host 127.0.0.1 --port 8080

# 禁用 XDG 目录标准
./build/bin/ip_server --no-xdg

# 启用文件日志
./build/bin/ip_server --enable-file-logging true --log-file /var/log/ip_server.log

# 启用 API 认证
./build/bin/ip_server --enable-api-auth true --api-keys-file /etc/ip_server/keys.txt

# 查看帮助信息
./build/bin/ip_server --help
```

### 数据库准备

从 [MaxMind](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) 下载 GeoLite2 数据库文件：

- `GeoLite2-City.mmdb` - 城市地理位置信息
- `GeoLite2-ASN.mmdb` - AS（自治系统）信息

**OUI 数据库**:
- `master_oui.db` - IEEE OUI 注册表（包含制造商信息）

**默认数据库路径** (使用 XDG 标准):
- City DB: `~/.local/share/ip-server/databases/GeoLite2-City.mmdb`
- ASN DB: `~/.local/share/ip-server/databases/GeoLite2-ASN.mmdb`
- OUI DB: `~/.local/share/ip-server/databases/master_oui.db`

**传统路径** (使用 `--no-xdg`):
- `db/GeoLite2-City.mmdb`
- `db/GeoLite2-ASN.mmdb`
- `db/master_oui.db`

## API 接口

### 基础端点

- `GET /` - 服务信息和可用端点列表
- `GET /health` - 健康检查（包含性能指标和数据库状态）

### 查询接口（支持 IP 和 MAC）

#### 单个查询

**IP 查询:**
```bash
# 查询指定 IP 地址
GET /lookup?ip=8.8.8.8

# 查询源 IP 地址（不带参数）
GET /lookup
```

**MAC 查询:**
```bash
# 查询指定 MAC 地址
GET /lookup?mac=00:1A:2B:3C:4D:5E
```

支持的 MAC 地址格式：
- `00:1A:2B:3C:4D:5E` (冒号分隔)
- `00-1A-2B-3C-4D-5E` (连字符分隔)
- `001A2B3C4D5E` (无分隔符)
- 大小写不敏感

**IP 查询响应示例:**
```json
{
  "ip": "8.8.8.8",
  "found": true,
  "country": "United States",
  "country_code": "US",
  "city": "Mountain View",
  "continent": "North America",
  "latitude": 37.4223,
  "longitude": -122.085,
  "timezone": "America/Los_Angeles",
  "as_organization": "Google LLC",
  "as_number": 15169
}
```

**MAC 查询响应示例:**
```json
{
  "mac": "00:1A:2B:3C:4D:5E",
  "oui": "00:1A:2B",
  "found": true,
  "manufacturer": "Example Manufacturer Inc.",
  "registry": "MA-L",
  "short_name": "EXAMPLE",
  "device_type": "Network Interface",
  "registered_date": "2010-01-15",
  "address": "123 Example Street, City, Country",
  "sources": "IEEE Registration Authority"
}
```

#### 批量查询

**批量 IP 查询:**
```bash
POST /lookup
Content-Type: application/json

{
  "ips": ["8.8.8.8", "1.1.1.1"]
}
```

**批量 MAC 查询:**
```bash
POST /lookup
Content-Type: application/json

{
  "macs": ["00:1A:2B:3C:4D:5E", "F4:EA:B5:12:34:56"]
}
```

**注意**: 不能在同一个请求中同时提供 `ips` 和 `macs`。

**批量 IP 查询响应示例:**
```json
[
  {
    "ip": "8.8.8.8",
    "found": true,
    "country": "United States",
    "country_code": "US",
    "city": "Mountain View",
    "as_organization": "Google LLC",
    "as_number": 15169
  },
  {
    "ip": "1.1.1.1",
    "found": true,
    "country": "Australia",
    "country_code": "AU",
    "city": "Sydney",
    "as_organization": "Cloudflare, Inc.",
    "as_number": 13335
  }
]
```

**批量 MAC 查询响应示例:**
```json
[
  {
    "mac": "00:1A:2B:3C:4D:5E",
    "oui": "00:1A:2B",
    "found": true,
    "manufacturer": "Example Manufacturer Inc.",
    "registry": "MA-L"
  },
  {
    "mac": "F4:EA:B5:12:34:56",
    "oui": "F4:EA:B5",
    "found": true,
    "manufacturer": "Another Company Ltd.",
    "registry": "MA-L"
  }
]
```

## 配置选项

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--config <path>` | 配置文件路径 | - |
| `--city-db <path>` | City 数据库路径 | XDG 路径 |
| `--asn-db <path>` | ASN 数据库路径 | XDG 路径 |
| `--oui-db <path>` | OUI 数据库路径 | XDG 路径 |
| `--host <address>` | 监听地址 | 0.0.0.0 |
| `--port <port>` | 监听端口 | 8080 |
| `--threads <count>` | 线程池大小 | 4 |
| `--enable-rate-limiter <true\|false>` | 启用速率限制 | true |
| `--max-requests-per-minute <count>` | 每分钟最大请求数 | 100 |
| `--max-batch-size <count>` | 批量查询最大数量 | 100 |
| `--enable-api-auth <true\|false>` | 启用 API 认证 | false |
| `--api-keys-file <path>` | API 密钥文件路径 | - |
| `--default-api-key <key>` | 默认 API 密钥 | - |
| `--enable-file-logging <true\|false>` | 启用文件日志 | false |
| `--log-file <path>` | 日志文件路径 | logs/ip_server.log |
| `--log-rotation <type>` | 日志轮转类型 | size |
| `--log-max-size <MB>` | 日志文件最大大小 | 10 |
| `--log-rotation-interval <minutes>` | 时间轮转间隔 | 1440 |
| `--log-max-backups <count>` | 最大备份文件数 | 5 |
| `--no-xdg` | 禁用 XDG 目录标准 | false |
| `--help, -h` | 显示帮助信息 | - |

### 配置文件

支持基于文本的配置文件（每行一个配置）：

```ini
# 服务器配置
host = 0.0.0.0
port = 8080
threads = 4

# 数据库路径
city_db = /path/to/GeoLite2-City.mmdb
asn_db = /path/to/GeoLite2-ASN.mmdb
oui_db = /path/to/master_oui.db

# 缓存配置
cache_size = 10000

# 速率限制
enable_rate_limiter = true
max_requests_per_minute = 100

# 批量查询
max_batch_size = 100

# API 认证
enable_api_auth = false
api_keys_file = /etc/ip_server/keys.txt
default_api_key =

# 日志配置
enable_file_logging = true
log_file = /var/log/ip_server/ip_server.log
log_rotation = size
log_max_file_size = 10
log_rotation_interval_minutes = 1440
log_max_backup_files = 5
```

### XDG 目录标准

当启用 XDG 标准时，配置和数据文件存储在标准位置：

- **配置文件**: `$XDG_CONFIG_HOME/ip-server/config.toml` (默认: `~/.config/ip-server/config.toml`)
- **数据库**: `$XDG_DATA_HOME/ip-server/databases/` (默认: `~/.local/share/ip-server/databases/`)
- **缓存**: `$XDG_CACHE_HOME/ip-server/` (默认: `~/.cache/ip-server/`)

## 开发规范

### 代码风格

- **命名空间**: 所有代码位于 `ip_server` 命名空间
- **类命名**: PascalCase (如 `IPGeoService`, `OUIDatabase`)
- **函数命名**: camelCase (如 `lookup`, `set_lookup_handler`)
- **成员变量**: 尾随下划线 (如 `city_db_`, `host_`)
- **常量**: UPPER_CASE (如 `LOG_ERROR`)

### 设计原则

1. **单一职责原则 (SRP)**
   - 每个类只负责一个功能
   - `OUIDatabase` 处理 OUI 数据库操作
   - `MACLookupService` 处理 MAC 查询业务逻辑
   - `IPGeoHTTPServer` 处理 HTTP 请求
   - `ConfigParser` 处理配置解析

2. **依赖注入**
   - HTTP 服务器通过函数注入查询处理器
   - 各模块通过构造函数注入依赖

3. **RAII (资源获取即初始化)**
   - 数据库自动管理资源生命周期
   - 使用移动语义，禁止拷贝操作

4. **异常安全**
   - 使用异常处理错误
   - 资源自动释放

### 日志规范

使用日志宏记录关键操作：

```cpp
LOG_DEBUG("Debug message");    // 调试信息
LOG_INFO("Info message");      // 一般信息
LOG_WARNING("Warning message"); // 警告信息
LOG_ERROR("Error message");    // 错误信息
```

日志格式：`[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message`

### 错误处理

- 使用异常处理运行时错误
- 数据库操作失败抛出 `std::runtime_error`
- JSON 解析错误使用 `nlohmann::json::exception`
- HTTP 请求错误返回适当的 HTTP 状态码

### 编译器警告

项目启用严格编译警告：
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC: `/W4`

代码应无警告编译通过。

### 测试规范

- 所有新功能必须包含单元测试
- 测试文件命名: `test_<module>.cpp`
- 使用 Google Test 框架
- 测试覆盖率目标: >80%

## 项目结构

```
ip_local/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目文档
├── IFLOW.md                    # iFlow 上下文文件（本文件）
├── docs/                       # 详细文档目录
│   ├── ARCHITECTURE.md         # 架构设计文档
│   ├── API_EXAMPLES.md         # API 使用示例
│   └── DEPLOYMENT.md           # 部署指南
├── src/                        # 源代码目录
│   ├── main.cpp               # 应用程序入口
│   ├── config.h/cpp           # 配置管理
│   ├── database.h/cpp         # 数据库抽象层（IP 和 MAC）
│   ├── mac_database.h/cpp     # OUI 数据库实现
│   ├── http_server.h/cpp      # HTTP 服务器
│   ├── logger.h/cpp           # 日志系统（支持轮转）
│   ├── types.h/cpp            # 数据类型定义
│   ├── cache.h                # LRU 缓存
│   ├── rate_limiter.h/cpp     # 速率限制器
│   ├── auth.h/cpp             # API 认证
│   ├── metrics.h/cpp          # 性能指标
│   └── xdg.h/cpp              # XDG 目录标准
├── tests/                      # 测试目录
│   ├── test_main.cpp          # 测试主程序
│   ├── test_config.cpp        # 配置测试
│   ├── test_database.cpp      # 数据库测试
│   ├── test_mac_database.cpp  # MAC 数据库测试
│   ├── test_http_server.cpp   # HTTP 服务器测试
│   ├── test_logger.cpp        # 日志系统测试
│   ├── test_rate_limiter.cpp  # 速率限制测试
│   ├── test_auth.cpp          # 认证测试
│   ├── test_types.cpp         # 类型测试
│   ├── benchmark_database.cpp # 数据库基准测试
│   ├── BENCHMARK.md           # 基准测试文档
│   ├── TEST_SUMMARY.md        # 测试摘要
│   └── run_benchmarks.sh      # 基准测试脚本
├── external/                   # 外部依赖库
│   ├── include/
│   │   ├── httplib.h          # HTTP 服务器库
│   │   └── nlohmann/          # JSON 库
│   ├── libmaxminddb-1.12.2/   # MaxMind 数据库库
│   └── sqlite-autoconf-3510200/ # SQLite3 源代码
├── db/                         # 数据库文件目录（传统路径）
│   ├── GeoLite2-City.mmdb     # 城市数据库
│   ├── GeoLite2-ASN.mmdb      # ASN 数据库
│   ├── GeoLite2-Country.mmdb  # 国家数据库（可选）
│   └── master_oui.db          # OUI 数据库
└── build/                      # 构建输出目录
    ├── bin/
    │   └── ip_server          # 可执行文件
    └── tests/
        └── ip_server_tests    # 测试可执行文件
```

## 常见任务

### 添加新的 API 端点

在 `src/http_server.cpp` 的 `setup_routes()` 方法中添加新路由：

```cpp
server_.Get("/new-endpoint", [this](const httplib::Request& req, httplib::Response& res) {
    // 处理逻辑
    json result = {{"status", "ok"}};
    res.set_content(result.dump(), "application/json");
});
```

### 修改数据库查询逻辑

在 `src/database.cpp` 中修改 `CityDatabase::lookup()` 或 `ASNDatabase::lookup()` 方法。
在 `src/mac_database.cpp` 中修改 `OUIDatabase::lookup()` 方法。

### 添加新的配置选项

1. 在 `src/config.h` 的 `ServerConfig` 结构体中添加新字段
2. 在 `src/config.cpp` 的 `parse()` 方法中添加参数解析逻辑
3. 在 `validate()` 方法中添加验证逻辑
4. 在 `print_help()` 方法中添加帮助信息

### 添加新的日志级别

1. 在 `src/logger.h` 的 `LogLevel` 枚举中添加新级别
2. 在 `src/logger.cpp` 的 `log()` 方法中添加级别处理
3. 添加对应的日志宏

### 运行测试

```bash
# 运行所有测试
./build/tests/ip_server_tests

# 运行特定测试
./build/tests/ip_server_tests --gtest_filter="LoggerTest.*"

# 健康检查
curl http://localhost:8080/health

# 单个 IP 查询
curl "http://localhost:8080/lookup?ip=8.8.8.8"

# 查询源 IP（不带参数）
curl "http://localhost:8080/lookup"

# 批量 IP 查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"ips": ["8.8.8.8", "1.1.1.1"]}'

# 单个 MAC 查询
curl "http://localhost:8080/lookup?mac=00:1A:2B:3C:4D:5E"

# 批量 MAC 查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"macs": ["00:1A:2B:3C:4D:5E", "F4:EA:B5:12:34:56"]}'
```

## 注意事项

1. **数据库文件**: 必须提供有效的数据库文件才能正常运行
   - MaxMind 数据库（City 和 ASN）
   - OUI 数据库（master_oui.db）
2. **端口占用**: 默认端口 8080，确保端口未被占用
3. **线程安全**: 日志系统和数据库查询使用互斥锁保证线程安全
4. **资源管理**: 数据库使用 RAII 模式，自动管理资源
5. **CORS 支持**: 服务器已配置 CORS 头，支持跨域请求
6. **速率限制**: 默认启用，防止 API 滥用
7. **日志轮转**: 启用文件日志时自动轮转，避免日志文件过大
8. **优雅关闭**: 支持 SIGINT/SIGTERM 信号，优雅关闭服务
9. **MAC 地址格式**: 支持多种格式（冒号、连字符、无分隔符），大小写不敏感

## 性能优化

### 缓存策略

- 使用 LRU 缓存减少数据库查询
- 默认缓存大小: 10,000 条记录
- 缓存命中率目标: >80%
- IP 查询和 MAC 查询都支持缓存

### 并发处理

- 线程池处理并发请求
- 默认线程数: 4
- 可根据 CPU 核心数调整

### 编译优化

- Release 模式使用 `-O2` 优化
- 性能提升约 8.6 倍（相比 Debug 模式）

### SQLite3 优化

- OUI 数据库使用 WAL 模式提高读性能
- 只读模式打开数据库
- 使用预编译语句提高查询效率

## 文档

- **架构设计**: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - 详细的架构设计和模块说明
- **API 示例**: [docs/API_EXAMPLES.md](docs/API_EXAMPLES.md) - API 使用示例和最佳实践
- **部署指南**: [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md) - 部署、配置和维护指南
- **基准测试**: [tests/BENCHMARK.md](tests/BENCHMARK.md) - 性能基准测试结果
- **测试摘要**: [tests/TEST_SUMMARY.md](tests/TEST_SUMMARY.md) - 测试覆盖率和结果

## 许可证

本项目使用的第三方库：
- libmaxminddb: Apache License 2.0
- httplib: MIT License
- nlohmann/json: MIT License
- SQLite3: Public Domain

## 相关链接

- MaxMind GeoLite2: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
- IEEE OUI Registry: https://standards-oui.ieee.org/
- cpp-httplib: https://github.com/yhirose/cpp-httplib
- nlohmann/json: https://github.com/nlohmann/json
- XDG Base Directory Specification: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html

## 提交规范

使用英文提交信息，格式：

```
<type>: <subject>

<body>

<footer>
```

类型（type）:
- `feat`: 新功能
- `fix`: 修复 bug
- `perf`: 性能优化
- `refactor`: 重构
- `test`: 测试相关
- `docs`: 文档更新
- `chore`: 构建/工具相关

示例：
```
feat: add MAC address OUI lookup functionality

- Add OUIDatabase class for SQLite3-based OUI lookups
- Add MACLookupService with LRU cache support
- Integrate MAC lookup into unified /lookup endpoint
- Support multiple MAC address formats (colon, hyphen, no separator)
- Add comprehensive unit tests for MAC lookup functionality
- Update metrics to track OUI database status
- All tests passing
```