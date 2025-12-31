# IP 位置信息查询服务端

基于 C++20 开发的 IP 地理位置和 AS（自治系统）信息查询服务端。

## 功能特性

- 查询 IP 地址的地理位置信息（国家、城市、经纬度、时区等）
- 查询 IP 地址的 AS 信息（自治系统编号和组织名称）
- 提供 RESTful API 接口
- 支持单个 IP 查询和批量查询
- 支持 CORS 跨域请求

## 依赖库

- **libmaxminddb**: MaxMind 数据库读取库（位于 `external/libmaxminddb-1.12.2`）
- **httplib**: C++ HTTP 服务器库（位于 `external/include/httplib.h`）
- **nlohmann/json**: JSON 处理库（位于 `external/include/nlohmann/json.hpp`）
- **OpenSSL**: HTTPS 支持（系统安装）

## 编译

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

编译完成后，可执行文件位于 `build/bin/ip_server`

## 使用方法

### 准备数据库文件

从 [MaxMind](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data) 下载 GeoLite2 数据库文件（推荐使用 `GeoLite2-City.mmdb`）。

### 启动服务

```bash
# 使用默认配置（端口 8080，数据库文件：GeoLite2-City.mmdb）
./build/bin/ip_server

# 自定义端口
./build/bin/ip_server --port 9000

# 自定义数据库文件路径
./build/bin/ip_server --db /path/to/GeoLite2-City.mmdb

# 自定义监听地址
./build/bin/ip_server --host 127.0.0.1 --port 8080

# 查看帮助
./build/bin/ip_server --help
```

## API 接口

### 1. 查询单个 IP 地址

**请求:**
```bash
GET /lookup?ip=8.8.8.8
```

**响应示例:**
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

### 2. 批量查询 IP 地址

**请求:**
```bash
POST /lookup
Content-Type: application/json

{
  "ips": ["8.8.8.8", "1.1.1.1", "114.114.114.114"]
}
```

**响应示例:**
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

### 3. 健康检查

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

### 4. 服务信息

**请求:**
```bash
GET /
```

**响应:**
```json
{
  "service": "IP Geolocation & AS Lookup Service",
  "version": "1.0.0",
  "endpoints": ["/", "/lookup", "/health"]
}
```

## 测试示例

使用 curl 测试：

```bash
# 查询单个 IP
curl "http://localhost:8080/lookup?ip=8.8.8.8"

# 批量查询
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{"ips": ["8.8.8.8", "1.1.1.1"]}'

# 健康检查
curl http://localhost:8080/health
```

## 项目结构

```
ip_local/
├── CMakeLists.txt              # CMake 构建配置
├── src/
│   └── main.cpp               # 主程序源码
├── external/                  # 外部依赖库
│   ├── include/
│   │   ├── httplib.h          # HTTP 服务器库
│   │   └── nlohmann/          # JSON 库
│   └── libmaxminddb-1.12.2/   # MaxMind 数据库库
└── build/                     # 编译输出目录
    └── bin/
        └── ip_server          # 可执行文件
```

## 注意事项

1. 需要有效的 MaxMind 数据库文件才能正常工作
2. 服务默认监听所有网络接口（0.0.0.0）
3. 数据库文件路径可以是相对路径或绝对路径
4. 批量查询时，建议单次查询不超过 100 个 IP 地址

## 许可证

本项目使用的第三方库：
- libmaxminddb: Apache License 2.0
- httplib: MIT License
- nlohmann/json: MIT License