# IP 位置信息查询服务端

基于 C++20 开发的高性能 IP 地理位置和 AS（自治系统）信息查询服务端，现已支持 MAC 地址 OUI 查询功能。

## 功能特性

- 查询 IP 地址的地理位置信息（国家、城市、经纬度、时区等）
- 查询 IP 地址的 AS 信息（自治系统编号和组织名称）
- 查询 MAC 地址的 OUI（组织唯一标识符）信息（制造商、注册机构等）
- 提供 RESTful API 接口
- 支持单个 IP/MAC 查询和批量查询
- 支持 CORS 跨域请求
- LRU 缓存提升查询性能
- 速率限制防止 API 滥用
- API 密钥认证支持
- 基于 spdlog 的日志系统，支持文件日志轮转
- 遵循 XDG 目录标准

## 技术栈

- **编程语言**: C++23
- **构建系统**: CMake 3.20+
- **HTTP 服务器**: cpp-httplib
- **日志库**: spdlog 1.17.0
- **数据库**: 
  - MaxMind GeoLite2 (City + ASN)
  - SQLite3 (OUI 数据库)
- **JSON 处理**: nlohmann/json
- **测试框架**: Google Test
- **性能测试**: Google Benchmark

## 依赖库

- **libmaxminddb**: MaxMind 数据库读取库（位于 `external/libmaxminddb-1.12.2`）
- **httplib**: C++ HTTP 服务器库（位于 `external/include/httplib.h`）
- **nlohmann/json**: JSON 处理库（位于 `external/include/nlohmann/json.hpp`）
- **spdlog**: 快速的 C++ 日志库（位于 `external/spdlog-1.17.0/`）
- **SQLite3**: 嵌入式数据库（位于 `external/sqlite-autoconf-3510200/`）

## 编译

### 环境要求

- C++23 兼容的编译器 (GCC 11+, Clang 13+, MSVC 19.30+)
- CMake 3.20+
- pthread
- Google Test (用于单元测试)

### 编译步骤

```bash
# 创建构建目录
mkdir build
cd build

# Debug 模式（默认）
cmake ..
cmake --build . -j$(nproc)

# Release 模式（推荐生产环境）
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

编译完成后，可执行文件位于 `build/bin/ip_server`

### 性能对比

- Debug 模式: 启用缓存 ~13.5μs (75k QPS)
- Release 模式: 启用缓存 ~1.56μs (647k QPS) - **性能提升约 8.6 倍**（使用 -O3 优化）

## 数据库准备

### MaxMind 数据库

从 [MaxMind](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) 下载 GeoLite2 数据库文件：

- `GeoLite2-City.mmdb` - 城市地理位置信息
- `GeoLite2-ASN.mmdb` - AS（自治系统）信息

### OUI 数据库

- `master_oui.db` - IEEE OUI 注册表（包含制造商信息）

**默认数据库路径** (使用 XDG 标准):
- City DB: `~/.local/share/ip-server/databases/GeoLite2-City.mmdb`
- ASN DB: `~/.local/share/ip-server/databases/GeoLite2-ASN.mmdb`
- OUI DB: `~/.local/share/ip-server/databases/master_oui.db`

**传统路径** (使用 `--no-xdg`):
- `db/GeoLite2-City.mmdb`
- `db/GeoLite2-ASN.mmdb`
- `db/master_oui.db`

## 使用方法

### 启动服务

```bash
# 使用默认配置（端口 8080，使用 XDG 目录标准）
./build/bin/ip_server

# 自定义端口
./build/bin/ip_server --port 9000

# 自定义数据库文件路径
./build/bin/ip_server --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb --oui-db /path/to/master_oui.db

# 自定义监听地址
./build/bin/ip_server --host 127.0.0.1 --port 8080

# 禁用 XDG 目录标准
./build/bin/ip_server --no-xdg

# 启用文件日志
./build/bin/ip_server --enable-file-logging true --log-file /var/log/ip_server.log

# 启用 API 认证
./build/bin/ip_server --enable-api-auth true --api-keys-file /etc/ip_server/keys.txt

# 查看帮助
./build/bin/ip_server --help
```

### 配置文件

支持基于文本的配置文件：

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

# 日志配置
enable_file_logging = true
log_file = /var/log/ip_server/ip_server.log
log_rotation = size
log_max_file_size = 10
log_rotation_interval_minutes = 1440
log_max_backup_files = 5
```

使用配置文件启动：

```bash
./build/bin/ip_server --config /path/to/config.txt
```

## API 接口

### 1. 服务信息

**请求:**
```bash
GET /
```

**响应:**
```json
{
  "service": "IP Geolocation & AS Lookup Service",
  "version": "2.0.0",
  "endpoints": ["/", "/lookup", "/mac/lookup", "/health", "/metrics"]
}
```

### 2. 健康检查

**请求:**
```bash
GET /health
```

**响应:**
```json
{
  "status": "ok"
}
```

### 3. 查询接口（支持 IP 和 MAC）

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
  "ips": ["8.8.8.8", "1.1.1.1", "114.114.114.114"]
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

## 测试示例

使用 curl 测试：

```bash
# 健康检查
curl http://localhost:8080/health

# 查询单个 IP
curl "http://localhost:8080/lookup?ip=8.8.8.8"

# 查询源 IP（不带参数）
curl "http://localhost:8080/lookup"

# 批量 IP 查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"ips": ["8.8.8.8", "1.1.1.1"]}'

# 查询单个 MAC
curl "http://localhost:8080/lookup?mac=00:1A:2B:3C:4D:5E"

# 批量 MAC 查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"macs": ["00:1A:2B:3C:4D:5E", "F4:EA:B5:12:34:56"]}'
```

## 运行测试

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

## 项目结构

```
ip_local/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
├── IFLOW.md                    # iFlow 上下文文件
├── docs/                       # 详细文档目录
│   ├── ARCHITECTURE.md         # 架构设计文档
│   ├── API_EXAMPLES.md         # API 使用示例
│   └── DEPLOYMENT.md           # 部署指南
├── src/                        # 源代码目录
│   ├── main.cpp               # 主程序
│   ├── config.h/cpp           # 配置管理
│   ├── database.h/cpp         # 数据库抽象层
│   ├── mac_database.h/cpp     # OUI 数据库
│   ├── http_server.h/cpp      # HTTP 服务器
│   ├── logger.h/cpp           # 日志系统
│   ├── types.h/cpp            # 数据类型
│   ├── cache.h                # LRU 缓存
│   ├── rate_limiter.h/cpp     # 速率限制
│   ├── auth.h/cpp             # API 认证
│   ├── metrics.h/cpp          # 性能指标
│   └── xdg.h/cpp              # XDG 目录标准
├── tests/                      # 测试目录
│   ├── test_main.cpp          # 测试主程序
│   ├── test_config.cpp        # 配置测试
│   ├── test_database.cpp      # 数据库测试
│   ├── test_mac_database.cpp  # MAC 数据库测试
│   ├── test_http_server.cpp   # HTTP 服务器测试
│   ├── test_logger.cpp        # 日志测试
│   ├── test_rate_limiter.cpp  # 速率限制测试
│   ├── test_auth.cpp          # 认证测试
│   ├── test_types.cpp         # 类型测试
│   ├── benchmark_database.cpp # 基准测试
│   ├── BENCHMARK.md           # 基准测试文档
│   ├── TEST_SUMMARY.md        # 测试摘要
│   └── run_benchmarks.sh      # 基准测试脚本
├── external/                   # 外部依赖库
│   ├── include/
│   │   ├── httplib.h          # HTTP 服务器库
│   │   └── nlohmann/          # JSON 库
│   ├── libmaxminddb-1.12.2/   # MaxMind 数据库库
│   ├── spdlog-1.17.0/         # spdlog 日志库
│   └── sqlite-autoconf-3510200/ # SQLite3 源代码
├── db/                         # 数据库文件目录（传统路径）
│   ├── GeoLite2-City.mmdb     # 城市数据库
│   ├── GeoLite2-ASN.mmdb      # ASN 数据库
│   ├── GeoLite2-Country.mmdb  # 国家数据库（可选）
│   └── master_oui.db          # OUI 数据库
└── build/                     # 编译输出目录
    ├── bin/
    │   └── ip_server          # 可执行文件
    └── tests/
        └── ip_server_tests    # 测试可执行文件
```

## 注意事项

1. **数据库文件**: 必须提供有效的数据库文件才能正常工作
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
10. **批量查询**: 建议单次查询不超过 100 个 IP/MAC 地址

## 性能优化

- 使用 LRU 缓存减少数据库查询（默认 10,000 条记录）
- 线程池处理并发请求（默认 4 个线程）
- Release 模式使用 `-O3` 优化
- SQLite3 使用 WAL 模式提高读性能
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
- spdlog: MIT License
- SQLite3: Public Domain

## 相关链接

- MaxMind GeoLite2: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
- IEEE OUI Registry: https://standards-oui.ieee.org/
- cpp-httplib: https://github.com/yhirose/cpp-httplib
- nlohmann/json: https://github.com/nlohmann/json
- spdlog: https://github.com/gabime/spdlog
- XDG Base Directory Specification: https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html