# 部署指南

本文档提供了 IP 位置信息查询服务的详细部署指南。

## 目录

1. [系统要求](#系统要求)
2. [编译安装](#编译安装)
3. [配置说明](#配置说明)
4. [部署方式](#部署方式)
5. [生产环境优化](#生产环境优化)
6. [监控和维护](#监控和维护)
7. [故障排查](#故障排查)

## 系统要求

### 硬件要求

**最低配置**:
- CPU: 2 核心
- 内存: 2 GB
- 磁盘: 1 GB 可用空间

**推荐配置**:
- CPU: 4 核心
- 内存: 4 GB
- 磁盘: 10 GB 可用空间（用于日志文件）
- SSD 存储（提高数据库读取性能）

### 软件要求

**操作系统**:
- Linux (推荐: Ubuntu 20.04+, CentOS 8+, Debian 11+)
- macOS 10.15+
- Windows 10+ (需要 WSL2)

**必需软件**:
- C++20 兼容编译器
  - GCC 10+
  - Clang 10+
  - MSVC 19.28+ (Windows)
- CMake 3.20+
- OpenSSL 开发库
- pthread 库

**可选软件**:
- Google Test (用于单元测试)
- Google Benchmark (用于性能测试)
- Docker (用于容器化部署)
- Nginx (用于反向代理)
- Systemd (用于服务管理)

### 依赖库

所有必需的依赖库已包含在项目中：
- libmaxminddb (位于 `external/libmaxminddb-1.12.2`)
- cpp-httplib (位于 `external/include/httplib.h`)
- nlohmann/json (位于 `external/include/nlohmann/`)

## 编译安装

### 1. 安装编译工具

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    libssl-dev \
    libgtest-dev \
    libbenchmark-dev
```

#### CentOS/RHEL

```bash
sudo yum install -y \
    gcc-c++ \
    cmake \
    openssl-devel \
    gtest-devel \
    gbenchmark-devel
```

#### macOS

```bash
brew install cmake openssl
```

### 2. 编译项目

```bash
# 克隆仓库（如果还没有）
git clone <repository-url>
cd ip_local

# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译（Debug 模式）
cmake --build . -j$(nproc)

# 编译（Release 模式，推荐生产环境）
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### 3. 运行测试

```bash
# 运行单元测试
./tests/ip_server_tests

# 运行性能测试（可选）
./bin/ip_server_benchmarks
```

### 4. 安装

```bash
# 创建安装目录
sudo mkdir -p /opt/ip-server
sudo mkdir -p /etc/ip-server
sudo mkdir -p /var/log/ip-server
sudo mkdir -p /var/lib/ip-server

# 复制可执行文件
sudo cp bin/ip_server /opt/ip-server/
sudo chmod +x /opt/ip-server/ip_server

# 创建符号链接（可选）
sudo ln -s /opt/ip-server/ip_server /usr/local/bin/ip-server
```

## 配置说明

### 1. 下载 MaxMind 数据库

```bash
# 创建数据库目录
sudo mkdir -p /var/lib/ip-server/databases

# 下载 GeoLite2 数据库
# 注意：需要从 MaxMind 网站获取免费许可证密钥
# 访问: https://dev.maxmind.com/geoip/geolite2-free-geolocation-data

# 使用 wget 下载（替换 LICENSE_KEY）
wget -O /tmp/GeoLite2-City.tar.gz \
  "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-City&license_key=YOUR_LICENSE_KEY&suffix=tar.gz"

wget -O /tmp/GeoLite2-ASN.tar.gz \
  "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-ASN&license_key=YOUR_LICENSE_KEY&suffix=tar.gz"

# 解压
tar -xzf /tmp/GeoLite2-City.tar.gz -C /tmp/
tar -xzf /tmp/GeoLite2-ASN.tar.gz -C /tmp/

# 复制数据库文件
sudo cp /tmp/GeoLite2-City_*/GeoLite2-City.mmdb /var/lib/ip-server/databases/
sudo cp /tmp/GeoLite2-ASN_*/GeoLite2-ASN.mmdb /var/lib/ip-server/databases/

# 设置权限
sudo chmod 644 /var/lib/ip-server/databases/*.mmdb
```

### 2. 创建配置文件

```bash
sudo nano /etc/ip-server/config.ini
```

配置文件内容：

```ini
# IP Server Configuration

# Server settings
host = 0.0.0.0
port = 8080
threads = 4

# Database paths
city_db = /var/lib/ip-server/databases/GeoLite2-City.mmdb
asn_db = /var/lib/ip-server/databases/GeoLite2-ASN.mmdb

# Cache settings
cache_size = 10000

# Rate limiting
enable_rate_limiter = true
max_requests_per_minute = 100

# Batch query settings
max_batch_size = 100

# API authentication
enable_api_auth = false
api_keys_file = /etc/ip-server/api_keys.txt
default_api_key =

# Logging
enable_file_logging = true
log_file = /var/log/ip-server/ip_server.log
log_rotation = size
log_max_file_size = 10
log_rotation_interval_minutes = 1440
log_max_backup_files = 5
```

### 3. 创建 API 密钥文件（可选）

```bash
sudo nano /etc/ip-server/api_keys.txt
```

每行一个 API 密钥：

```
secret_key_1
secret_key_2
secret_key_3
```

设置权限：

```bash
sudo chmod 600 /etc/ip-server/api_keys.txt
```

## 部署方式

### 方式 1: 直接运行

```bash
# 使用默认配置
/opt/ip-server/ip_server

# 使用配置文件
/opt/ip-server/ip_server --config /etc/ip-server/config.ini

# 自定义参数
/opt/ip-server/ip_server \
  --host 0.0.0.0 \
  --port 8080 \
  --city-db /var/lib/ip-server/databases/GeoLite2-City.mmdb \
  --asn-db /var/lib/ip-server/databases/GeoLite2-ASN.mmdb \
  --threads 4 \
  --enable-file-logging true \
  --log-file /var/log/ip-server/ip_server.log
```

### 方式 2: 使用 Systemd 服务

创建服务文件：

```bash
sudo nano /etc/systemd/system/ip-server.service
```

服务文件内容：

```ini
[Unit]
Description=IP Geolocation & AS Lookup Service
After=network.target

[Service]
Type=simple
User=ipserver
Group=ipserver
WorkingDirectory=/opt/ip-server
ExecStart=/opt/ip-server/ip-server --config /etc/ip-server/config.ini
Restart=always
RestartSec=10

# 安全设置
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/ip-server /var/lib/ip-server

# 资源限制
LimitNOFILE=65536
MemoryMax=1G
CPUQuota=200%

[Install]
WantedBy=multi-user.target
```

创建用户：

```bash
sudo useradd -r -s /bin/false ipserver
sudo chown -R ipserver:ipserver /opt/ip-server
sudo chown -R ipserver:ipserver /var/log/ip-server
sudo chown -R ipserver:ipserver /var/lib/ip-server
```

启用并启动服务：

```bash
sudo systemctl daemon-reload
sudo systemctl enable ip-server
sudo systemctl start ip-server

# 查看状态
sudo systemctl status ip-server

# 查看日志
sudo journalctl -u ip-server -f
```

### 方式 3: 使用 Docker

#### 3.1 创建 Dockerfile

```dockerfile
# 使用官方 Ubuntu 基础镜像
FROM ubuntu:22.04

# 设置环境变量
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 复制源代码
COPY . /app/

# 编译项目
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

# 创建必要的目录
RUN mkdir -p /var/lib/ip-server/databases \
    /var/log/ip-server \
    /etc/ip-server

# 复制可执行文件
RUN cp build/bin/ip_server /usr/local/bin/

# 设置权限
RUN chmod +x /usr/local/bin/ip_server

# 创建非 root 用户
RUN useradd -r -s /bin/false ipserver && \
    chown -R ipserver:ipserver /var/lib/ip-server /var/log/ip-server

# 切换到非 root 用户
USER ipserver

# 暴露端口
EXPOSE 8080

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# 启动服务
CMD ["/usr/local/bin/ip-server", "--config", "/etc/ip-server/config.ini"]
```

#### 3.2 创建 docker-compose.yml

```yaml
version: '3.8'

services:
  ip-server:
    build: .
    container_name: ip-server
    ports:
      - "8080:8080"
    volumes:
      - ./config:/etc/ip-server:ro
      - ./databases:/var/lib/ip-server/databases:ro
      - ./logs:/var/log/ip-server
    restart: unless-stopped
    environment:
      - TZ=Asia/Shanghai
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 5s
    networks:
      - ip-server-network

networks:
  ip-server-network:
    driver: bridge
```

#### 3.3 构建和运行

```bash
# 构建镜像
docker-compose build

# 启动服务
docker-compose up -d

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down

# 重启服务
docker-compose restart
```

### 方式 4: 使用 Nginx 反向代理

#### 4.1 安装 Nginx

```bash
sudo apt install nginx
```

#### 4.2 配置 Nginx

```bash
sudo nano /etc/nginx/sites-available/ip-server
```

配置文件内容：

```nginx
upstream ip_server {
    server 127.0.0.1:8080;
    # 如果有多个实例，可以添加更多服务器
    # server 127.0.0.1:8081;
    # server 127.0.0.1:8082;
}

server {
    listen 80;
    server_name your-domain.com;

    # 日志
    access_log /var/log/nginx/ip-server-access.log;
    error_log /var/log/nginx/ip-server-error.log;

    # 客户端上传大小限制
    client_max_body_size 10M;

    # 代理设置
    location / {
        proxy_pass http://ip_server;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # 超时设置
        proxy_connect_timeout 60s;
        proxy_send_timeout 60s;
        proxy_read_timeout 60s;

        # 缓冲设置
        proxy_buffering on;
        proxy_buffer_size 4k;
        proxy_buffers 8 4k;
        proxy_busy_buffers_size 8k;
    }

    # 健康检查端点
    location /health {
        proxy_pass http://ip_server/health;
        access_log off;
    }
}
```

启用配置：

```bash
sudo ln -s /etc/nginx/sites-available/ip-server /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

#### 4.3 配置 HTTPS（使用 Let's Encrypt）

```bash
# 安装 Certbot
sudo apt install certbot python3-certbot-nginx

# 获取证书
sudo certbot --nginx -d your-domain.com

# 自动续期
sudo certbot renew --dry-run
```

## 生产环境优化

### 1. 性能优化

#### 1.1 编译优化

使用 Release 模式编译：

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

#### 1.2 线程池配置

根据 CPU 核心数调整线程池大小：

```ini
threads = 8  # 对于 8 核 CPU
```

#### 1.3 缓存大小

根据内存大小调整缓存：

```ini
cache_size = 50000  # 50,000 条记录
```

#### 1.4 数据库优化

- 使用 SSD 存储
- 确保数据库文件在内存中有足够的缓存
- 定期更新数据库文件

### 2. 安全加固

#### 2.1 防火墙配置

```bash
# 使用 UFW
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw enable

# 使用 iptables
sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT
sudo iptables -A INPUT -j DROP
```

#### 2.2 启用 API 认证

```ini
enable_api_auth = true
api_keys_file = /etc/ip-server/api_keys.txt
```

#### 2.3 速率限制

```ini
enable_rate_limiter = true
max_requests_per_minute = 100
```

#### 2.4 日志轮转

```ini
enable_file_logging = true
log_rotation = both
log_max_file_size = 10
log_rotation_interval_minutes = 1440
log_max_backup_files = 5
```

### 3. 高可用部署

#### 3.1 负载均衡

使用 Nginx 或 HAProxy 进行负载均衡：

```nginx
upstream ip_server {
    # 使用最少连接算法
    least_conn;
    
    server 10.0.0.1:8080;
    server 10.0.0.2:8080;
    server 10.0.0.3:8080;
    
    # 健康检查
    check interval=3000 rise=2 fall=3 timeout=1000;
}
```

#### 3.2 集群部署

部署多个实例，使用共享数据库或独立的数据库副本：

```
┌─────────────┐
│ Load Balancer│
└──────┬──────┘
       │
  ┌────┴────┐
  │         │
┌─▼───┐  ┌─▼───┐  ┌─▼───┐
│Node 1│  │Node 2│  │Node 3│
└──────┘  └──────┘  └──────┘
```

#### 3.3 数据库同步

如果使用多个实例，确保所有实例使用相同的数据库文件：

```bash
# 使用 rsync 同步数据库
rsync -avz /var/lib/ip-server/databases/ user@node2:/var/lib/ip-server/databases/
rsync -avz /var/lib/ip-server/databases/ user@node3:/var/lib/ip-server/databases/
```

### 4. 监控和告警

#### 4.1 健康检查

定期检查服务健康状态：

```bash
#!/bin/bash
# health_check.sh

HEALTH_URL="http://localhost:8080/health"
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" $HEALTH_URL)

if [ $RESPONSE -eq 200 ]; then
    echo "Service is healthy"
    exit 0
else
    echo "Service is unhealthy (HTTP $RESPONSE)"
    exit 1
fi
```

#### 4.2 性能监控

使用 `/metrics` 端点监控性能指标：

```bash
#!/bin/bash
# monitor.sh

METRICS_URL="http://localhost:8080/metrics"
METRICS=$(curl -s $METRICS_URL)

# 提取关键指标
TOTAL_REQUESTS=$(echo $METRICS | jq '.total_requests')
CACHE_HIT_RATE=$(echo $METRICS | jq '.cache_hit_rate')
AVG_RESPONSE_TIME=$(echo $METRICS | jq '.avg_response_time_ms')

echo "Total Requests: $TOTAL_REQUESTS"
echo "Cache Hit Rate: $CACHE_HIT_RATE"
echo "Avg Response Time: $AVG_RESPONSE_TIME ms"
```

#### 4.3 日志监控

监控日志文件大小和错误：

```bash
#!/bin/bash
# log_monitor.sh

LOG_FILE="/var/log/ip-server/ip_server.log"

# 检查日志文件大小
FILE_SIZE=$(du -h $LOG_FILE | cut -f1)
echo "Log file size: $FILE_SIZE"

# 检查错误数量
ERROR_COUNT=$(grep -c "ERROR" $LOG_FILE)
echo "Error count: $ERROR_COUNT"

# 检查最近的错误
echo "Recent errors:"
tail -n 10 $LOG_FILE | grep "ERROR"
```

## 监控和维护

### 1. 日志管理

#### 1.1 查看日志

```bash
# 实时查看日志
tail -f /var/log/ip-server/ip_server.log

# 查看最近的日志
tail -n 100 /var/log/ip-server/ip_server.log

# 搜索错误
grep "ERROR" /var/log/ip-server/ip_server.log

# 搜索特定 IP
grep "8.8.8.8" /var/log/ip-server/ip_server.log
```

#### 1.2 日志轮转

服务会自动轮转日志文件，但也可以手动配置 logrotate：

```bash
sudo nano /etc/logrotate.d/ip-server
```

配置文件内容：

```
/var/log/ip-server/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
    create 0644 ipserver ipserver
    sharedscripts
    postrotate
        systemctl reload ip-server > /dev/null 2>&1 || true
    endscript
}
```

### 2. 数据库更新

#### 2.1 自动更新脚本

```bash
#!/bin/bash
# update_database.sh

DB_DIR="/var/lib/ip-server/databases"
LICENSE_KEY="YOUR_LICENSE_KEY"
BACKUP_DIR="/var/lib/ip-server/backups"

# 创建备份目录
mkdir -p $BACKUP_DIR

# 备份当前数据库
cp $DB_DIR/GeoLite2-City.mmdb $BACKUP_DIR/GeoLite2-City.mmdb.bak
cp $DB_DIR/GeoLite2-ASN.mmdb $BACKUP_DIR/GeoLite2-ASN.mmdb.bak

# 下载新数据库
wget -O /tmp/GeoLite2-City.tar.gz \
  "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-City&license_key=$LICENSE_KEY&suffix=tar.gz"

wget -O /tmp/GeoLite2-ASN.tar.gz \
  "https://download.maxmind.com/app/geoip_download?edition_id=GeoLite2-ASN&license_key=$LICENSE_KEY&suffix=tar.gz"

# 解压
tar -xzf /tmp/GeoLite2-City.tar.gz -C /tmp/
tar -xzf /tmp/GeoLite2-ASN.tar.gz -C /tmp/

# 更新数据库
mv /tmp/GeoLite2-City_*/GeoLite2-City.mmdb $DB_DIR/
mv /tmp/GeoLite2-ASN_*/GeoLite2-ASN.mmdb $DB_DIR/

# 设置权限
chmod 644 $DB_DIR/*.mmdb

# 重启服务
systemctl restart ip-server

echo "Database updated successfully"
```

#### 2.2 设置定时任务

```bash
# 编辑 crontab
sudo crontab -e

# 每周更新一次数据库
0 3 * * 0 /usr/local/bin/update_database.sh >> /var/log/ip-server/update.log 2>&1
```

### 3. 备份和恢复

#### 3.1 备份配置

```bash
#!/bin/bash
# backup_config.sh

BACKUP_DIR="/var/backups/ip-server"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# 备份配置文件
tar -czf $BACKUP_DIR/config_$DATE.tar.gz \
    /etc/ip-server/config.ini \
    /etc/ip-server/api_keys.txt

echo "Configuration backed up to $BACKUP_DIR/config_$DATE.tar.gz"
```

#### 3.2 恢复配置

```bash
#!/bin/bash
# restore_config.sh

BACKUP_FILE=$1

if [ -z "$BACKUP_FILE" ]; then
    echo "Usage: $0 <backup_file>"
    exit 1
fi

# 恢复配置文件
tar -xzf $BACKUP_FILE -C /

# 重启服务
systemctl restart ip-server

echo "Configuration restored from $BACKUP_FILE"
```

### 4. 性能调优

#### 4.1 系统参数调优

编辑 `/etc/sysctl.conf`：

```ini
# 网络参数
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.tcp_tw_reuse = 1

# 文件描述符限制
fs.file-max = 1000000
```

应用配置：

```bash
sudo sysctl -p
```

#### 4.2 文件描述符限制

编辑 `/etc/security/limits.conf`：

```
ipserver soft nofile 65535
ipserver hard nofile 65535
```

## 故障排查

### 1. 服务无法启动

#### 检查日志

```bash
sudo journalctl -u ip-server -n 50
```

#### 常见问题

**数据库文件不存在**:
```
Error: City database file does not exist
```

**解决方案**:
```bash
# 检查数据库文件
ls -la /var/lib/ip-server/databases/

# 重新下载数据库
# 参考前面的数据库下载步骤
```

**端口被占用**:
```
Error: Failed to bind to port 8080
```

**解决方案**:
```bash
# 查找占用端口的进程
sudo lsof -i :8080

# 杀死进程或更改端口
sudo kill -9 <PID>
```

### 2. 查询失败

#### 检查服务状态

```bash
curl http://localhost:8080/health
```

#### 检查数据库状态

```bash
curl http://localhost:8080/metrics
```

#### 检查日志

```bash
tail -f /var/log/ip-server/ip_server.log
```

### 3. 性能问题

#### 检查系统资源

```bash
# CPU 使用率
top

# 内存使用
free -h

# 磁盘 I/O
iostat -x 1

# 网络连接
netstat -an | grep :8080 | wc -l
```

#### 检查缓存命中率

```bash
curl http://localhost:8080/metrics | jq '.cache_hit_rate'
```

如果缓存命中率低于 80%，考虑增加缓存大小。

### 4. 内存泄漏

#### 监控内存使用

```bash
# 查看进程内存
ps aux | grep ip_server

# 持续监控
watch -n 1 'ps aux | grep ip_server'
```

#### 使用 Valgrind 检测泄漏

```bash
valgrind --leak-check=full /opt/ip-server/ip_server
```

## 升级指南

### 1. 备份

在升级前，备份所有重要数据：

```bash
# 备份配置
./backup_config.sh

# 备份数据库
cp /var/lib/ip-server/databases/* /var/backups/ip-server/
```

### 2. 停止服务

```bash
sudo systemctl stop ip-server
```

### 3. 更新代码

```bash
# 拉取最新代码
git pull origin main

# 重新编译
cd build
cmake ..
cmake --build . -j$(nproc)
```

### 4. 更新可执行文件

```bash
sudo cp bin/ip_server /opt/ip-server/
sudo chmod +x /opt/ip-server/ip_server
```

### 5. 启动服务

```bash
sudo systemctl start ip-server
```

### 6. 验证

```bash
# 检查服务状态
sudo systemctl status ip-server

# 测试 API
curl http://localhost:8080/health
curl "http://localhost:8080/lookup?ip=8.8.8.8"
```

## 总结

本文档提供了 IP 位置信息查询服务的完整部署指南，包括：

- 系统要求和依赖安装
- 编译和配置步骤
- 多种部署方式（直接运行、Systemd、Docker、Nginx）
- 生产环境优化建议
- 监控和维护方法
- 故障排查指南
- 升级流程

按照本指南，您可以成功部署并运行 IP 位置信息查询服务。

如有任何问题或建议，请参考项目的 GitHub 仓库或联系开发团队。