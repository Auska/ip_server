# IP Geolocation & AS Lookup Service - Agent Guide

## 必须遵守的规则

- 每次提交必须要获得许可
- 每次方案都会参考最佳实现

## 项目概述

高性能 IP 地理位置和 AS（自治系统）信息查询服务端，支持 MAC 地址 OUI 查询和密码生成功能。

- **版本**: 2.0.0
- **语言**: C++23
- **构建系统**: CMake 3.20+ / Xmake
- **功能**: IP 地理位置、AS 信息、MAC 地址 OUI 查询、密码生成

## 项目结构

```
ip_local/
├── src/                        # 源代码
│   ├── main.cpp               # 主程序入口
│   ├── config.h/cpp           # 配置管理
│   ├── database.h/cpp         # 数据库抽象层
│   ├── mac_database.h/cpp     # OUI 数据库
│   ├── http_server.h/cpp      # HTTP 服务器
│   ├── logger.h/cpp           # 日志系统
│   ├── password_generator.h/cpp  # 密码生成器
│   ├── types.h                # 数据类型定义
│   ├── cache.h                # LRU 缓存（仅头文件）
│   ├── rate_limiter.h/cpp     # 速率限制
│   ├── auth.h/cpp             # API 认证
│   ├── metrics.h/cpp          # 性能指标
│   └── xdg.h/cpp              # XDG 目录标准
├── tests/                      # 测试代码
│   ├── test_*.cpp             # 单元测试（含 PasswordGeneratorTest）
│   ├── benchmark_*.cpp        # 基准测试
│   └── run_benchmarks.sh      # 测试脚本
├── external/                   # 第三方依赖
│   ├── include/               # httplib.h, nlohmann/json
│   ├── libmaxminddb-1.12.2/   # MaxMind 数据库库
│   ├── spdlog-1.17.0/         # 日志库
│   ├── sqlite-autoconf-3510200/ # SQLite3
│   └── cxxopts-3.3.1/         # 命令行参数解析
└── db/                         # 数据库文件（传统路径）
```

## 构建命令

### CMake

```bash
# 创建构建目录
mkdir build && cd build

# Debug 模式（默认）
cmake ..
cmake --build . -j$(nproc)

# Release 模式（推荐生产环境，-O3 + LTO）
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Release 模式并启用基准测试
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build . -j$(nproc)
```

**Release 模式性能优化**:
- 编译选项: `-O3 -DNDEBUG -flto`
- 链接选项: `-flto -s`（strip symbols）
- 性能提升: 约 8.6 倍（Debug: ~13.5μs → Release: ~1.56μs）

### Xmake

```bash
# Debug 模式
xmake f -m debug
xmake

# Release 模式
xmake f -m release
xmake

# 清理构建
xmake clean
```

## 测试命令

### 运行所有测试（CMake）

```bash
./build/tests/ip_server_tests
```

### 运行特定测试套件（CMake）

```bash
# Logger 测试
./build/tests/ip_server_tests --gtest_filter="LoggerTest.*"

# Database 测试
./build/tests/ip_server_tests --gtest_filter="DatabaseTest.*"

# MAC Database 测试
./build/tests/ip_server_tests --gtest_filter="MACDatabaseTest.*"

# HTTP Server 测试
./build/tests/ip_server_tests --gtest_filter="HTTPServerTest.*"

# Config 测试
./build/tests/ip_server_tests --gtest_filter="ConfigTest.*"

# Rate Limiter 测试
./build/tests/ip_server_tests --gtest_filter="RateLimiterTest.*"

# Auth 测试
./build/tests/ip_server_tests --gtest_filter="AuthTest.*"

# Password Generator 测试
./build/tests/ip_server_tests --gtest_filter="PasswordGeneratorTest.*"

# Types 测试
./build/tests/ip_server_tests --gtest_filter="TypesTest.*"
```

### 运行测试（Xmake）

```bash
xmake test
```

### 性能基准测试

```bash
# 运行所有基准测试
./build/bin/ip_server_benchmarks

# 使用脚本运行
./tests/run_benchmarks.sh

# 运行性能测试（带迭代控制）
./tests/run_performance_tests.sh --benchmark all --iterations 3

# 运行特定基准测试类别
./tests/run_performance_tests.sh --benchmark database
./tests/run_performance_tests.sh --benchmark cache
./tests/run_performance_tests.sh --benchmark concurrent
```

## 运行服务

### 基本启动

```bash
# 使用默认配置（端口 8080，XDG 目录标准）
./build/bin/ip_server

# 自定义端口
./build/bin/ip_server --port 9000

# 自定义监听地址
./build/bin/ip_server --host 127.0.0.1 --port 8080

# 使用传统路径（不使用 XDG）
./build/bin/ip_server --no-xdg
```

### 数据库配置

```bash
# 自定义数据库路径
./build/bin/ip_server \
  --city-db /path/to/GeoLite2-City.mmdb \
  --asn-db /path/to/GeoLite2-ASN.mmdb \
  --oui-db /path/to/master_oui.db
```

**默认数据库路径（XDG 标准）**:
- City DB: `~/.local/share/ip-server/databases/GeoLite2-City.mmdb`
- ASN DB: `~/.local/share/ip-server/databases/GeoLite2-ASN.mmdb`
- OUI DB: `~/.local/share/ip-server/databases/master_oui.db`

**传统路径**（使用 `--no-xdg`）:
- `db/GeoLite2-City.mmdb`
- `db/GeoLite2-ASN.mmdb`
- `db/master_oui.db`

### 日志配置

```bash
# 启用文件日志
./build/bin/ip_server --enable-file-logging true --log-file /var/log/ip_server.log

# 配置日志轮转
./build/bin/ip_server \
  --log-rotation size \
  --log-max-file-size 10 \
  --log-max-backup-files 5
```

### 认证配置

```bash
# 启用 API 认证
./build/bin/ip_server --enable-api-auth true --api-keys-file /etc/ip_server/keys.txt
```

### 配置文件

```bash
# 使用配置文件启动
./build/bin/ip_server --config /path/to/config.txt
```

配置文件示例（.ini 格式）:
```ini
host = 0.0.0.0
port = 8080
threads = 4

city_db = /path/to/GeoLite2-City.mmdb
asn_db = /path/to/GeoLite2-ASN.mmdb
oui_db = /path/to/master_oui.db

cache_size = 10000
enable_rate_limiter = true
max_requests_per_minute = 100
max_batch_size = 100

enable_api_auth = false
api_keys_file = /etc/ip_server/keys.txt

enable_file_logging = true
log_file = /var/log/ip_server/ip_server.log
log_rotation = size
log_max_file_size = 10
log_rotation_interval_minutes = 1440
log_max_backup_files = 5
```

## API 接口

### 服务信息

```bash
GET /
```

响应:
```json
{
  "service": "IP Geolocation & AS Lookup Service",
  "version": "2.0.0",
  "endpoints": ["/", "/lookup", "/mac/lookup", "/health", "/metrics"]
}
```

### 健康检查

```bash
GET /health
```

### IP 查询

```bash
# 查询指定 IP
GET /lookup?ip=8.8.8.8

# 查询源 IP（不带参数）
GET /lookup
```

响应:
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

### MAC 地址查询

```bash
# 查询指定 MAC 地址
GET /lookup?mac=00:1A:2B:3C:4D:5E
```

支持的 MAC 地址格式:
- `00:1A:2B:3C:4D:5E`（冒号分隔）
- `00-1A-2B-3C-4D-5E`（连字符分隔）
- `001A2B3C4D5E`（无分隔符）
- 大小写不敏感

响应:
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

### 批量查询

```bash
# 批量 IP 查询
POST /lookup
Content-Type: application/json

{
  "ips": ["8.8.8.8", "1.1.1.1", "114.114.114.114"]
}

# 批量 MAC 查询
POST /lookup
Content-Type: application/json

{
  "macs": ["00:1A:2B:3C:4D:5E", "F4:EA:B5:12:34:56"]
}
```

### 性能指标

```bash
GET /metrics
```

### 密码生成

```bash
# 生成单个密码（默认配置）
GET /password/generate

# 生成自定义密码
GET /password/generate?length=24&exclude_similar=true

# 批量生成密码
POST /password/generate
Content-Type: application/json

{
  "count": 5,
  "length": 16,
  "uppercase": true,
  "lowercase": true,
  "digits": true,
  "symbols": true,
  "exclude_similar": true
}
```

响应:
```json
{
  "password": "A8kM#nP2qR5sT9vW",
  "length": 16,
  "entropy": 94.5,
  "strength": "very_strong"
}
```

批量响应:
```json
{
  "count": 5,
  "passwords": [
    {
      "password": "A8kM#nP2qR5sT9vW",
      "length": 16,
      "entropy": 94.5,
      "strength": "very_strong"
    }
  ]
}
```

**密码生成参数**:
- `length`: 密码长度（8-128，默认 16）
- `uppercase`: 包含大写字母（默认 true）
- `lowercase`: 包含小写字母（默认 true）
- `digits`: 包含数字（默认 true）
- `symbols`: 包含特殊符号（默认 true）
- `exclude_similar`: 排除易混淆字符（默认 true）
  - 排除字符：`I, O, i, l, o, 0, 1`

**密码强度评级**:
- `very_weak`: entropy < 28
- `weak`: 28 <= entropy < 36
- `fair`: 36 <= entropy < 60
- `strong`: 60 <= entropy < 80
- `very_strong`: entropy >= 80

## 代码风格指南

### 格式化规则

- **样式**: Google 风格（自定义）
- **缩进**: 4 空格
- **行宽**: 100 字符
- **大括号**: K&R 风格（左大括号不换行）
- **指针对齐**: 左对齐（`int* p` 不是 `int *p`）

### 命名约定

- **命名空间**: `ip_server`
- **类**: PascalCase（如 `IPGeoService`、`OUIDatabase`）
- **函数**: camelCase（如 `lookup`、`set_lookup_handler`）
- **成员变量**: 尾部下划线（如 `city_db_`、`host_`）
- **常量**: UPPER_CASE（如 `LOG_ERROR`）

### 类型指南

- 使用 C++23 特性
- 优先使用 `std::jthread` 而非 `std::thread`
- 对资源密集型类应用移动语义
- 遵循 RAII 模式

### 错误处理

- 运行时错误使用异常（`std::runtime_error`）
- 数据库操作失败抛出异常
- JSON 操作使用 `nlohmann::json::exception`
- HTTP 错误返回适当状态码

### 日志

- 使用 LOG_* 宏（基于 spdlog）:
  - `LOG_DEBUG("message")` - 调试信息
  - `LOG_INFO("message")` - 一般信息
  - `LOG_WARNING("message")` - 警告
  - `LOG_ERROR("message")` - 错误
- 格式: `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message`

## 开发实践

### 代码组织

- 单一职责原则
- 通过构造函数进行依赖注入
- 性能关键组件使用仅头文件实现（`cache.h`）

### 测试要求

- 新功能必须包含单元测试
- 测试文件命名: `test_<module>.cpp`
- 核心模块测试覆盖率 > 90%
- 使用描述性测试名称
- 当前测试套件：9 个测试套件，206 个测试用例
  - PasswordGeneratorTest: 20 个测试
  - LoggerTest: 17 个测试
  - ConfigTest: 31 个测试
  - DatabaseTest: 15 个测试
  - MACDatabaseTest: 12 个测试
  - HTTPServerTest: 25 个测试
  - RateLimiterTest: 12 个测试
  - AuthTest: 22 个测试
  - TypesTest: 52 个测试

### 性能考虑

- 所有数据库操作使用 LRU 缓存
- 资源管理使用移动语义
- 性能测试优先使用 Release 模式（-O3 + LTO）
- 使用 Google Benchmark 进行性能验证
- 密码生成使用 C++23 `<random>` 库，64 位 Mersenne Twister 生成器
- 密码生成平均延迟：~0.042ms

### 配置管理

- 强制执行 XDG 目录标准
- 支持 JSON/INI 配置文件
- 命令行参数覆盖配置文件设置

## 代码质量命令

### 格式化代码

```bash
# 格式化单个文件
clang-format -i src/main.cpp

# 格式化所有源文件
find src tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# 检查格式化（不修改）
find src tests -name "*.cpp" -o -name "*.h" | xargs clang-format -Werror --dry-run
```

### 构建警告

- 启用严格警告: `-Wall -Wextra -Wpedantic`
- 代码应无警告编译
- MSVC 使用 `/W4`

## 依赖库

| 库名 | 版本 | 用途 | 位置 |
|------|------|------|------|
| libmaxminddb | 1.12.2 | MaxMind 数据库读取 | external/ |
| spdlog | 1.17.0 | 日志系统 | external/ |
| nlohmann/json | 3.12.0 | JSON 处理 | external/include/ |
| httplib | 0.28.0 | HTTP 服务器 | external/include/ |
| SQLite3 | 3.51.0+0 | OUI 数据库 | external/ |
| cxxopts | 3.3.1 | 命令行参数 | external/ |
| Google Test | - | 单元测试框架 | 系统依赖 |
| Google Benchmark | - | 性能基准测试 | 系统依赖 |

## 性能指标

### 查询性能（Release 模式）

- **缓存命中**: ~1.56μs（647k QPS）
- **缓存未命中**: ~13.5μs（75k QPS）
- **性能提升**: 约 8.6 倍（vs Debug 模式）
- **密码生成**: ~0.042ms（~24k QPS）

### 缓存效果

- **命中率**: 95%+
- **默认大小**: 10,000 条记录
- **算法**: LRU（最近最少使用）

## 相关文档

- **架构设计**: docs/ARCHITECTURE.md
- **API 示例**: docs/API_EXAMPLES.md
- **部署指南**: docs/DEPLOYMENT.md
- **基准测试**: tests/BENCHMARK.md
- **测试摘要**: tests/TEST_SUMMARY.md

## 最新功能

### 密码生成 API（v2.0.0）

新增密码生成功能，支持：

- **安全随机数生成**: 使用 C++23 `<random>` 库，64 位 Mersenne Twister
- **灵活配置**: 支持大写、小写、数字、符号，可排除易混淆字符
- **批量生成**: 最多一次生成 100 个密码
- **强度评估**: 自动计算熵值并评估密码强度
- **完整集成**: 支持认证、速率限制、性能监控

**使用示例**:
```bash
# 生成 24 位强密码
curl "http://localhost:8080/password/generate?length=24&exclude_similar=true"

# 批量生成 5 个密码
curl -X POST http://localhost:8080/password/generate \
  -H "Content-Type: application/json" \
  -d '{"count": 5, "length": 16, "exclude_similar": true}'
```

## 外部资源

- MaxMind GeoLite2: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
- IEEE OUI Registry: https://standards-oui.ieee.org/
- cpp-httplib: https://github.com/yhirose/cpp-httplib
- nlohmann/json: https://github.com/nlohmann/json
- spdlog: https://github.com/gabime/spdlog
- XDG Base Directory: https://specifications.freedesktop.org/basedir-spec/
