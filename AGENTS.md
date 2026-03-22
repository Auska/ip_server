# IP Geolocation & AS Lookup Service - Agent Guide

## 必须遵守的规则

- 每次提交必须要获得许可
- 每次方案都会参考最佳实现
- 代码必须无警告编译（`-Wall -Wextra -Wpedantic`）

## 项目概述

高性能 IP 地理位置和 AS 信息查询服务端，支持 MAC 地址 OUI 查询、密码生成功能。

- **版本**: 2.0.0
- **语言**: C++23
- **构建系统**: CMake 3.20+
- **功能**: IP 地理位置、AS 信息、MAC 地址 OUI 查询、密码生成
- **测试**: 216 个单元测试，覆盖率 > 90%
- **性能**: Release 模式缓存命中 ~1.56μs（647k QPS）

## 项目结构

```
ip_local/
├── src/                        # 源代码
│   ├── main.cpp               # 主程序入口
│   ├── config.h/cpp           # 配置管理
│   ├── database.h/cpp         # 数据库抽象层（MaxMind 基类）
│   ├── mac_database.h/cpp     # OUI 数据库（SQLite RAII）
│   ├── http_server.h/cpp      # HTTP 服务器
│   ├── logger.h/cpp           # 日志系统
│   ├── password_generator.h/cpp  # 密码生成器（多熵源随机）
│   ├── cache.h                # LRU 缓存（分片+读写锁）
│   ├── rate_limiter.h/cpp     # 速率限制（O(1) LRU）
│   ├── auth.h/cpp             # API 认证（SHA-256 哈希）
│   ├── metrics.h/cpp          # 性能指标
│   └── xdg.h/cpp              # XDG 目录标准
├── tests/                      # 测试代码（9 个测试套件，216 个测试）
├── external/                   # 第三方依赖
├── docs/                       # 文档
└── CMakeLists.txt              # CMake 构建配置
```

## 构建命令

```bash
# Debug 模式
mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)

# Release 模式（-O3 + LTO）
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j$(nproc)

# 运行测试
./tests/ip_server_tests 或 ctest --output-on-failure
```

## 运行服务

```bash
# 基本启动
./build/bin/ip_server

# 自定义配置
./build/bin/ip_server --port 9000 --host 0.0.0.0

# 启用认证（API Key SHA-256 哈希存储）
./build/bin/ip_server --enable-api-auth true --api-keys-file /etc/ip_server/keys.txt

# 配置可信代理（防止 X-Forwarded-For 欺骗）
./build/bin/ip_server --trusted-proxies "127.0.0.1,::1,10.0.0.0/8"
```

## API 接口

| 端点 | 方法 | 描述 |
|------|------|------|
| `/` | GET | 服务信息 |
| `/health` | GET | 健康检查（含缓存/错误统计） |
| `/lookup?ip=<addr>` | GET | IP 查询 |
| `/lookup?mac=<addr>` | GET | MAC 查询 |
| `/lookup` | POST | 批量查询（ips/macs 数组） |
| `/password/generate` | GET/POST | 密码生成 |

## 代码风格

- **命名空间**: `ip_server`
- **类**: PascalCase（如 `IPGeoService`）
- **函数**: camelCase（如 `lookup`）
- **成员变量**: 尾部下划线（如 `city_db_`）
- **常量**: UPPER_CASE 或 namespace constants

## 依赖库

| 库 | 版本 | 用途 |
|---|------|------|
| libmaxminddb | 1.12.2 | MaxMind 数据库 |
| spdlog | 1.17.0 | 日志系统 |
| nlohmann/json | 3.12.0 | JSON 处理 |
| httplib | 0.28.0 | HTTP 服务器 |
| SQLite3 | 3.51.0+0 | OUI 数据库 |
| OpenSSL | - | API Key 哈希 |
| Google Test | - | 单元测试 |

## 性能指标

- **缓存命中**: ~1.56μs（647k QPS）
- **缓存未命中**: ~13.5μs（75k QPS）
- **密码生成**: ~0.042ms
- **速率限制 LRU**: O(1) 驱逐

## 安全特性

- **API Key**: SHA-256 哈希存储，防止内存泄露
- **代理信任**: 可配置可信代理 IP 白名单
- **密码生成**: 8 熵源 seed_seq 增强随机性
- **输入验证**: IP/MAC 格式预验证

## 缓存特性

- **分片**: 8 分片减少锁竞争
- **读写锁**: `std::shared_mutex` 提高读并发
- **差异化 TTL**: IP 1h, ASN 24h, MAC 7d, 负缓存 5min
- **内存限制**: 默认 100MB，内存感知驱逐
- **布隆过滤器**: 防止缓存穿透

## 相关文档

- docs/ARCHITECTURE.md - 架构设计
- docs/API_EXAMPLES.md - API 示例
- docs/DEPLOYMENT.md - 部署指南
- tests/TEST_SUMMARY.md - 测试摘要

## 外部资源

- MaxMind GeoLite2: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data
- IEEE OUI Registry: https://standards-oui.ieee.org/
- cpp-httplib: https://github.com/yhirose/cpp-httplib
