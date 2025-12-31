# IP Geolocation & AS Lookup Service

## 项目概述

这是一个基于 C++20 开发的高性能 IP 地理位置和 AS（自治系统）信息查询服务端。项目采用现代 C++ 设计模式和行业最佳实践，提供 RESTful API 接口，支持单个 IP 查询和批量查询。

### 核心技术栈

- **编程语言**: C++20
- **构建系统**: CMake 3.20+
- **HTTP 服务器**: cpp-httplib
- **数据库**: MaxMind GeoLite2 (City + ASN)
- **JSON 处理**: nlohmann/json
- **加密**: OpenSSL

### 架构设计

项目采用分层架构，遵循 SOLID 原则：

```
┌─────────────────────────────────────┐
│   HTTP Layer (IPGeoHTTPServer)      │  RESTful API 接口
├─────────────────────────────────────┤
│   Service Layer (IPGeoService)      │  业务逻辑层
├─────────────────────────────────────┤
│   Data Layer (City/ASN Database)    │  数据访问层
├─────────────────────────────────────┤
│   MaxMind DB Library                │  底层数据库
└─────────────────────────────────────┘
```

### 主要模块

- **config.h/cpp**: 配置管理，命令行参数解析
- **database.h/cpp**: 数据库抽象层，支持 City 和 ASN 数据库
- **http_server.h/cpp**: HTTP 服务器，路由处理
- **logger.h/cpp**: 线程安全的日志系统
- **types.h/cpp**: 数据类型定义
- **main.cpp**: 应用程序入口

## 构建和运行

### 环境要求

- C++20 兼容的编译器 (GCC 10+, Clang 10+, MSVC 19.28+)
- CMake 3.20+
- OpenSSL 开发库
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

### 构建基准测试

```bash
# 配置项目并启用基准测试
cmake .. -DBUILD_BENCHMARKS=ON

# 编译
cmake --build . -j$(nproc)

# 运行基准测试
./build/bin/ip_server_benchmarks

# 运行特定基准测试
./build/bin/ip_server_benchmarks --benchmark_filter=CityDatabase.*

# 使用便捷脚本运行所有基准测试
./tests/run_benchmarks.sh
```

详细基准测试文档请参考 [tests/BENCHMARK.md](tests/BENCHMARK.md)

### 运行服务

```bash
# 使用默认配置运行
./build/bin/ip_server

# 自定义端口
./build/bin/ip_server --port 9000

# 自定义数据库路径
./build/bin/ip_server --city-db /path/to/GeoLite2-City.mmdb --asn-db /path/to/GeoLite2-ASN.mmdb

# 自定义监听地址
./build/bin/ip_server --host 127.0.0.1 --port 8080

# 查看帮助信息
./build/bin/ip_server --help
```

### 数据库准备

从 [MaxMind](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) 下载 GeoLite2 数据库文件：

- `GeoLite2-City.mmdb` - 城市地理位置信息
- `GeoLite2-ASN.mmdb` - AS（自治系统）信息

默认数据库路径：`db/GeoLite2-City.mmdb` 和 `db/GeoLite2-ASN.mmdb`

## API 接口

### 基础端点

- `GET /` - 服务信息和可用端点列表
- `GET /health` - 健康检查

### IP 查询

#### 单个 IP 查询

```bash
# 查询指定 IP 地址
GET /lookup?ip=8.8.8.8

# 查询源 IP 地址（不带参数）
GET /lookup
```

响应示例：
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

#### 批量查询

```bash
POST /lookup
Content-Type: application/json

{
  "ips": ["8.8.8.8", "1.1.1.1"]
}
```

## 开发规范

### 代码风格

- **命名空间**: 所有代码位于 `ip_server` 命名空间
- **类命名**: PascalCase (如 `IPGeoService`)
- **函数命名**: camelCase (如 `lookup`, `set_lookup_handler`)
- **成员变量**: 尾随下划线 (如 `city_db_`, `host_`)
- **常量**: UPPER_CASE (如 `LOG_ERROR`)

### 设计原则

1. **单一职责原则 (SRP)**
   - 每个类只负责一个功能
   - `MaxMindDatabase` 处理数据库操作
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

## 项目结构

```
ip_local/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目文档
├── IFLOW.md                    # iFlow 上下文文件（本文件）
├── src/                        # 源代码目录
│   ├── main.cpp               # 应用程序入口
│   ├── config.h/cpp           # 配置管理
│   ├── database.h/cpp         # 数据库抽象层
│   ├── http_server.h/cpp      # HTTP 服务器
│   ├── logger.h/cpp           # 日志系统
│   └── types.h/cpp            # 数据类型定义
├── external/                   # 外部依赖库
│   ├── include/
│   │   ├── httplib.h          # HTTP 服务器库
│   │   └── nlohmann/          # JSON 库
│   └── libmaxminddb-1.12.2/   # MaxMind 数据库库
├── db/                         # 数据库文件目录
│   ├── GeoLite2-City.mmdb     # 城市数据库
│   └── GeoLite2-ASN.mmdb      # ASN 数据库
└── build/                      # 构建输出目录
    └── bin/
        └── ip_server          # 可执行文件
```

## 常见任务

### 添加新的 API 端点

在 `src/http_server.cpp` 的 `setup_routes()` 方法中添加新路由：

```cpp
server_.Get("/new-endpoint", [](const httplib::Request& req, httplib::Response& res) {
    // 处理逻辑
    res.set_content(json_result.dump(), "application/json");
});
```

### 修改数据库查询逻辑

在 `src/database.cpp` 中修改 `CityDatabase::lookup()` 或 `ASNDatabase::lookup()` 方法。

### 添加新的配置选项

1. 在 `src/config.h` 的 `ServerConfig` 结构体中添加新字段
2. 在 `src/config.cpp` 的 `parse()` 方法中添加参数解析逻辑
3. 在 `print_help()` 方法中添加帮助信息

### 运行测试

```bash
# 健康检查
curl http://localhost:8080/health

# 单个 IP 查询
curl "http://localhost:8080/lookup?ip=8.8.8.8"

# 批量查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"ips": ["8.8.8.8", "1.1.1.1"]}'
```

## 注意事项

1. **数据库文件**: 必须提供有效的 MaxMind 数据库文件才能正常运行
2. **端口占用**: 默认端口 8080，确保端口未被占用
3. **线程安全**: 日志系统使用互斥锁保证线程安全
4. **资源管理**: 数据库使用 RAII 模式，自动管理资源
5. **CORS 支持**: 服务器已配置 CORS 头，支持跨域请求

## 许可证

本项目使用的第三方库：
- libmaxminddb: Apache License 2.0
- httplib: MIT License
- nlohmann/json: MIT License

## 相关链接

- MaxMind GeoLite2: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
- cpp-httplib: https://github.com/yhirose/cpp-httplib
- nlohmann/json: https://github.com/nlohmann/json