# 性能测试指南

本文档说明如何使用 Google Benchmark 对 IP 地理位置查询服务进行性能测试。

## 安装 Google Benchmark

### Ubuntu/Debian

```bash
sudo apt-get install libbenchmark-dev
```

### macOS (使用 Homebrew)

```bash
brew install google-benchmark
```

### 从源码编译

```bash
git clone https://github.com/google/benchmark.git
cd benchmark
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

## 构建基准测试

使用 `-DBUILD_BENCHMARKS=ON` 选项启用基准测试构建：

```bash
cd build
cmake .. -DBUILD_BENCHMARKS=ON
cmake --build . -j$(nproc)
```

## 运行基准测试

### 运行所有基准测试

```bash
./build/bin/ip_server_benchmarks
```

### 运行特定基准测试

```bash
# 只运行 CityDatabase 相关测试
./build/bin/ip_server_benchmarks --benchmark_filter=CityDatabase.*

# 只运行缓存相关测试
./build/bin/ip_server_benchmarks --benchmark_filter=.*Cache.*

# 运行 IPGeoService 测试
./build/bin/ip_server_benchmarks --benchmark_filter=IPGeoService.*
```

### 常用选项

```bash
# 指定迭代次数
./build/bin/ip_server_benchmarks --benchmark_repetitions=5

# 指定最小运行时间
./build/bin/ip_server_benchmarks --benchmark_min_time=2.0

# 输出为 JSON 格式
./build/bin/ip_server_benchmarks --benchmark_out=results.json --benchmark_out_format=json

# 输出为 CSV 格式
./build/bin/ip_server_benchmarks --benchmark_out=results.csv --benchmark_out_format=console

# 显示基准测试列表
./build/bin/ip_server_benchmarks --benchmark_list_tests

# 不运行基准测试，只显示时间
./build/bin/ip_server_benchmarks --benchmark_dry_run
```

## 基准测试说明

### 数据库查询测试

- `CityDatabase_SingleLookup`: 测试城市数据库单次查询性能
- `CityDatabase_MultipleLookup`: 测试城市数据库多 IP 查询性能
- `ASNDatabase_SingleLookup`: 测试 ASN 数据库单次查询性能
- `ASNDatabase_MultipleLookup`: 测试 ASN 数据库多 IP 查询性能
- `CityDatabase_IPv6Lookup`: 测试 IPv6 地址查询性能

### 服务层测试

- `IPGeoService_WithCache`: 测试启用缓存的服务查询性能
- `IPGeoService_WithoutCache`: 测试禁用缓存的服务查询性能
- `IPGeoService_CacheHit`: 测试缓存命中时的查询性能
- `IPGeoService_CacheOperations`: 测试缓存操作性能
- `IPGeoService_CacheSize`: 测试不同缓存大小的影响 (100-100000)
- `IPGeoService_ConcurrentLookups`: 测试并发查询性能 (1-10)

### 数据库操作测试

- `CityDatabase_OpenClose`: 测试数据库打开/关闭性能
- `ASNDatabase_OpenClose`: 测试 ASN 数据库打开/关闭性能
- `IPGeoService_Initialization`: 测试服务初始化性能

## 性能指标说明

基准测试输出包含以下指标：

- **Time**: 单次操作的平均时间
- **CPU**: CPU 时间
- **Iterations**: 迭代次数
- **Items/s**: 每秒处理的项目数

示例输出：

```
------------------------------------------------------------------------------
Benchmark                                    Time             CPU   Iterations
------------------------------------------------------------------------------
CityDatabase_SingleLookup                 45.2 ns         45.1 ns     15345678
ASNDatabase_SingleLookup                  38.7 ns         38.6 ns     18023456
IPGeoService_WithCache                    52.3 ns         52.2 ns     13456789
IPGeoService_WithoutCache                125.6 ns        125.4 ns      5600000
```

## 优化建议

根据基准测试结果，可以考虑以下优化：

1. **缓存优化**: 对比启用和禁用缓存的结果，确定缓存带来的性能提升
2. **缓存大小**: 根据使用场景调整缓存大小，在内存和性能之间取得平衡
3. **并发处理**: 评估并发查询的性能，考虑使用线程池优化
4. **数据库选择**: 比较不同数据库格式的查询性能

## 注意事项

1. 确保数据库文件存在于 `db/` 目录下
2. 首次运行基准测试时，数据库可能需要加载到内存，结果可能不稳定
3. 建议多次运行基准测试以获得稳定的平均值
4. 运行基准测试时关闭其他资源密集型应用
5. 确保系统处于稳定状态（无大量后台进程）