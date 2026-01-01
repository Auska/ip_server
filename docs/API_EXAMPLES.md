# API 使用示例

本文档提供了 IP 位置信息查询服务的详细 API 使用示例。

## 目录

1. [基础信息](#基础信息)
2. [认证方式](#认证方式)
3. [API 端点](#api-端点)
4. [使用示例](#使用示例)
5. [错误处理](#错误处理)
6. [最佳实践](#最佳实践)

## 基础信息

### Base URL

```
http://localhost:8080
```

### 响应格式

所有 API 响应均为 JSON 格式。

### 字符编码

UTF-8

## 认证方式

如果启用了 API 认证，需要在请求中提供 API 密钥。

### 方式 1: HTTP Header

```http
X-API-Key: your_api_key_here
```

### 方式 2: Query Parameter

```
?api_key=your_api_key_here
```

### 示例

```bash
# 使用 Header
curl -H "X-API-Key: your_key" http://localhost:8080/lookup?ip=8.8.8.8

# 使用 Query Parameter
curl "http://localhost:8080/lookup?ip=8.8.8.8&api_key=your_key"
```

## API 端点

### 1. 服务信息

获取服务基本信息和可用端点列表。

**端点**: `GET /`

**认证**: 不需要

**响应示例**:
```json
{
  "service": "IP Geolocation & AS Lookup Service",
  "version": "1.0.0",
  "endpoints": [
    "/",
    "/health",
    "/metrics",
    "/lookup"
  ]
}
```

**示例**:
```bash
curl http://localhost:8080/
```

### 2. 健康检查

检查服务健康状态。

**端点**: `GET /health`

**认证**: 不需要

**响应示例**:
```json
{
  "status": "ok"
}
```

**示例**:
```bash
curl http://localhost:8080/health
```

### 3. 性能指标

获取服务性能指标。

**端点**: `GET /metrics`

**认证**: 不需要

**响应示例**:
```json
{
  "uptime_seconds": 3600,
  "total_requests": 10000,
  "successful_requests": 9950,
  "failed_requests": 50,
  "cache_hits": 8000,
  "cache_misses": 2000,
  "cache_hit_rate": 0.8,
  "avg_response_time_ms": 1.5,
  "city_db_status": "open",
  "asn_db_status": "open"
}
```

**示例**:
```bash
curl http://localhost:8080/metrics
```

### 4. 单个 IP 查询

查询单个 IP 地址的地理位置和 AS 信息。

**端点**: `GET /lookup?ip=<ip_address>`

**认证**: 可选（如果启用）

**参数**:
- `ip` (必需): IP 地址（IPv4 或 IPv6）

**响应示例**:
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

**响应字段说明**:
- `ip`: 查询的 IP 地址
- `found`: 是否找到信息
- `country`: 国家名称
- `country_code`: 国家代码（ISO 3166-1 alpha-2）
- `city`: 城市名称
- `continent`: 大洲名称
- `latitude`: 纬度
- `longitude`: 经度
- `timezone`: 时区
- `as_organization`: AS 组织名称
- `as_number`: AS 编号

**示例**:
```bash
# 查询 Google DNS
curl "http://localhost:8080/lookup?ip=8.8.8.8"

# 查询 Cloudflare DNS
curl "http://localhost:8080/lookup?ip=1.1.1.1"

# 查询 114 DNS
curl "http://localhost:8080/lookup?ip=114.114.114.114"

# 使用 API 密钥
curl -H "X-API-Key: your_key" "http://localhost:8080/lookup?ip=8.8.8.8"
```

### 5. 批量查询

批量查询多个 IP 地址的信息。

**端点**: `POST /lookup`

**认证**: 可选（如果启用）

**请求头**:
```
Content-Type: application/json
```

**请求体**:
```json
{
  "ips": ["8.8.8.8", "1.1.1.1", "114.114.114.114"]
}
```

**参数**:
- `ips` (必需): IP 地址数组，最多 100 个

**响应示例**:
```json
[
  {
    "ip": "8.8.8.8",
    "found": true,
    "country": "United States",
    "country_code": "US",
    "city": "Mountain View",
    "latitude": 37.4223,
    "longitude": -122.085,
    "as_organization": "Google LLC",
    "as_number": 15169
  },
  {
    "ip": "1.1.1.1",
    "found": true,
    "country": "Australia",
    "country_code": "AU",
    "city": "Sydney",
    "latitude": -33.8688,
    "longitude": 151.2093,
    "as_organization": "Cloudflare, Inc.",
    "as_number": 13335
  },
  {
    "ip": "114.114.114.114",
    "found": true,
    "country": "China",
    "country_code": "CN",
    "city": "Beijing",
    "latitude": 39.9042,
    "longitude": 116.4074,
    "as_organization": "China Telecom",
    "as_number": 4134
  }
]
```

**示例**:
```bash
# 批量查询多个 IP
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -d '{
    "ips": ["8.8.8.8", "1.1.1.1", "114.114.114.114"]
  }'

# 使用 API 密钥
curl -X POST http://localhost:8080/lookup \
  -H "Content-Type: application/json" \
  -H "X-API-Key: your_key" \
  -d '{
    "ips": ["8.8.8.8", "1.1.1.1"]
  }'
```

## 使用示例

### Python 示例

#### 单个 IP 查询

```python
import requests

# 查询单个 IP
response = requests.get('http://localhost:8080/lookup?ip=8.8.8.8')
data = response.json()

print(f"IP: {data['ip']}")
print(f"Country: {data['country']}")
print(f"City: {data['city']}")
print(f"AS Organization: {data['as_organization']}")
print(f"AS Number: {data['as_number']}")
```

#### 批量查询

```python
import requests

# 批量查询
response = requests.post(
    'http://localhost:8080/lookup',
    json={'ips': ['8.8.8.8', '1.1.1.1', '114.114.114.114']}
)
results = response.json()

for result in results:
    print(f"{result['ip']}: {result['country']}, {result['city']}")
```

#### 使用 API 密钥

```python
import requests

headers = {
    'X-API-Key': 'your_api_key_here'
}

response = requests.get(
    'http://localhost:8080/lookup?ip=8.8.8.8',
    headers=headers
)
data = response.json()
print(data)
```

### JavaScript 示例

#### 使用 fetch

```javascript
// 单个 IP 查询
fetch('http://localhost:8080/lookup?ip=8.8.8.8')
  .then(response => response.json())
  .then(data => {
    console.log(`IP: ${data.ip}`);
    console.log(`Country: ${data.country}`);
    console.log(`City: ${data.city}`);
    console.log(`AS: ${data.as_organization} (${data.as_number})`);
  });

// 批量查询
fetch('http://localhost:8080/lookup', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    ips: ['8.8.8.8', '1.1.1.1', '114.114.114.114']
  })
})
  .then(response => response.json())
  .then(results => {
    results.forEach(result => {
      console.log(`${result.ip}: ${result.country}, ${result.city}`);
    });
  });
```

#### 使用 Axios

```javascript
const axios = require('axios');

// 单个 IP 查询
axios.get('http://localhost:8080/lookup?ip=8.8.8.8')
  .then(response => {
    const data = response.data;
    console.log(`IP: ${data.ip}`);
    console.log(`Country: ${data.country}`);
  });

// 批量查询
axios.post('http://localhost:8080/lookup', {
  ips: ['8.8.8.8', '1.1.1.1', '114.114.114.114']
})
  .then(response => {
    response.data.forEach(result => {
      console.log(`${result.ip}: ${result.country}, ${result.city}`);
    });
  });
```

### Java 示例

```java
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.List;

public class IpLookupClient {
    private static final String BASE_URL = "http://localhost:8080";
    private final HttpClient client;

    public IpLookupClient() {
        this.client = HttpClient.newHttpClient();
    }

    // 单个 IP 查询
    public String lookupSingleIp(String ip) throws Exception {
        String url = BASE_URL + "/lookup?ip=" + ip;
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(url))
            .GET()
            .build();

        HttpResponse<String> response = client.send(
            request,
            HttpResponse.BodyHandlers.ofString()
        );

        return response.body();
    }

    // 批量查询
    public String lookupBatch(List<String> ips) throws Exception {
        String jsonBody = String.format("{\"ips\": %s}", ips.toString());
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(BASE_URL + "/lookup"))
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(jsonBody))
            .build();

        HttpResponse<String> response = client.send(
            request,
            HttpResponse.BodyHandlers.ofString()
        );

        return response.body();
    }

    public static void main(String[] args) throws Exception {
        IpLookupClient client = new IpLookupClient();

        // 单个 IP 查询
        String result = client.lookupSingleIp("8.8.8.8");
        System.out.println(result);

        // 批量查询
        String batchResult = client.lookupBatch(
            List.of("8.8.8.8", "1.1.1.1", "114.114.114.114")
        );
        System.out.println(batchResult);
    }
}
```

### Go 示例

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "io"
    "net/http"
)

const baseURL = "http://localhost:8080"

type LookupRequest struct {
    IPs []string `json:"ips"`
}

type LookupResult struct {
    IP             string  `json:"ip"`
    Found          bool    `json:"found"`
    Country        string  `json:"country"`
    CountryCode    string  `json:"country_code"`
    City           string  `json:"city"`
    Latitude       float64 `json:"latitude"`
    Longitude      float64 `json:"longitude"`
    ASOrganization string  `json:"as_organization"`
    ASNumber       int     `json:"as_number"`
}

// 单个 IP 查询
func lookupSingleIP(ip string) (*LookupResult, error) {
    url := fmt.Sprintf("%s/lookup?ip=%s", baseURL, ip)
    resp, err := http.Get(url)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()

    body, err := io.ReadAll(resp.Body)
    if err != nil {
        return nil, err
    }

    var result LookupResult
    err = json.Unmarshal(body, &result)
    return &result, err
}

// 批量查询
func lookupBatch(ips []string) ([]LookupResult, error) {
    reqBody := LookupRequest{IPs: ips}
    jsonData, err := json.Marshal(reqBody)
    if err != nil {
        return nil, err
    }

    resp, err := http.Post(
        baseURL+"/lookup",
        "application/json",
        bytes.NewBuffer(jsonData),
    )
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()

    body, err := io.ReadAll(resp.Body)
    if err != nil {
        return nil, err
    }

    var results []LookupResult
    err = json.Unmarshal(body, &results)
    return results, err
}

func main() {
    // 单个 IP 查询
    result, err := lookupSingleIP("8.8.8.8")
    if err != nil {
        panic(err)
    }
    fmt.Printf("%s: %s, %s\n", result.IP, result.Country, result.City)

    // 批量查询
    results, err := lookupBatch([]string{"8.8.8.8", "1.1.1.1"})
    if err != nil {
        panic(err)
    }
    for _, r := range results {
        fmt.Printf("%s: %s, %s\n", r.IP, r.Country, r.City)
    }
}
```

### PHP 示例

```php
<?php

class IpLookupClient {
    private $baseUrl;

    public function __construct($baseUrl = 'http://localhost:8080') {
        $this->baseUrl = $baseUrl;
    }

    // 单个 IP 查询
    public function lookupSingleIp($ip) {
        $url = $this->baseUrl . '/lookup?ip=' . urlencode($ip);
        $response = file_get_contents($url);
        return json_decode($response, true);
    }

    // 批量查询
    public function lookupBatch($ips) {
        $url = $this->baseUrl . '/lookup';
        $data = json_encode(['ips' => $ips]);

        $options = [
            'http' => [
                'method' => 'POST',
                'header' => 'Content-Type: application/json',
                'content' => $data
            ]
        ];

        $context = stream_context_create($options);
        $response = file_get_contents($url, false, $context);
        return json_decode($response, true);
    }
}

// 使用示例
$client = new IpLookupClient();

// 单个 IP 查询
$result = $client->lookupSingleIp('8.8.8.8');
echo "{$result['ip']}: {$result['country']}, {$result['city']}\n";

// 批量查询
$results = $client->lookupBatch(['8.8.8.8', '1.1.1.1', '114.114.114.114']);
foreach ($results as $result) {
    echo "{$result['ip']}: {$result['country']}, {$result['city']}\n";
}
```

## 错误处理

### 错误响应格式

```json
{
  "error": "错误描述",
  "code": 400
}
```

### 常见错误

#### 1. 无效的 IP 地址

**HTTP 状态码**: 400

**响应**:
```json
{
  "error": "Invalid IP address",
  "code": 400
}
```

#### 2. 未授权

**HTTP 状态码**: 401

**响应**:
```json
{
  "error": "Unauthorized",
  "code": 401
}
```

#### 3. 速率限制

**HTTP 状态码**: 429

**响应**:
```json
{
  "error": "Rate limit exceeded",
  "code": 429
}
```

#### 4. 服务器错误

**HTTP 状态码**: 500

**响应**:
```json
{
  "error": "Internal server error",
  "code": 500
}
```

### 错误处理示例

#### Python

```python
import requests

try:
    response = requests.get('http://localhost:8080/lookup?ip=invalid_ip')
    response.raise_for_status()
    data = response.json()
    print(data)
except requests.exceptions.HTTPError as e:
    if response.status_code == 400:
        print(f"Bad Request: {response.json()['error']}")
    elif response.status_code == 401:
        print("Unauthorized: Invalid API key")
    elif response.status_code == 429:
        print("Rate limit exceeded")
    else:
        print(f"Error: {e}")
except requests.exceptions.RequestException as e:
    print(f"Request failed: {e}")
```

#### JavaScript

```javascript
fetch('http://localhost:8080/lookup?ip=invalid_ip')
  .then(response => {
    if (!response.ok) {
      if (response.status === 400) {
        throw new Error('Bad Request');
      } else if (response.status === 401) {
        throw new Error('Unauthorized');
      } else if (response.status === 429) {
        throw new Error('Rate limit exceeded');
      } else {
        throw new Error('Server error');
      }
    }
    return response.json();
  })
  .then(data => console.log(data))
  .catch(error => console.error('Error:', error));
```

## 最佳实践

### 1. 缓存响应

对于频繁查询的 IP 地址，建议在客户端缓存响应，减少服务器负载。

```python
import requests
from functools import lru_cache

@lru_cache(maxsize=1000)
def lookup_ip(ip):
    response = requests.get(f'http://localhost:8080/lookup?ip={ip}')
    return response.json()

# 第一次查询会访问服务器
result1 = lookup_ip('8.8.8.8')

# 第二次查询会使用缓存
result2 = lookup_ip('8.8.8.8')
```

### 2. 批量查询

对于多个 IP 地址，使用批量查询而不是多次单个查询，提高效率。

```python
import requests

# 不推荐：多次单个查询
for ip in ['8.8.8.8', '1.1.1.1', '114.114.114.114']:
    response = requests.get(f'http://localhost:8080/lookup?ip={ip}')
    print(response.json())

# 推荐：批量查询
response = requests.post(
    'http://localhost:8080/lookup',
    json={'ips': ['8.8.8.8', '1.1.1.1', '114.114.114.114']}
)
for result in response.json():
    print(result)
```

### 3. 错误重试

对于临时性错误（如网络问题），实现重试机制。

```python
import requests
import time

def lookup_ip_with_retry(ip, max_retries=3):
    for attempt in range(max_retries):
        try:
            response = requests.get(f'http://localhost:8080/lookup?ip={ip}')
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            if attempt == max_retries - 1:
                raise
            time.sleep(2 ** attempt)  # 指数退避
```

### 4. 超时设置

设置合理的超时时间，避免长时间等待。

```python
import requests

response = requests.get(
    'http://localhost:8080/lookup?ip=8.8.8.8',
    timeout=5  # 5 秒超时
)
```

### 5. 连接池

使用连接池提高性能。

```python
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry

session = requests.Session()

# 配置重试策略
retry_strategy = Retry(
    total=3,
    backoff_factor=1,
    status_forcelist=[429, 500, 502, 503, 504]
)

adapter = HTTPAdapter(max_retries=retry_strategy)
session.mount("http://", adapter)
session.mount("https://", adapter)

# 使用 session 发送请求
response = session.get('http://localhost:8080/lookup?ip=8.8.8.8')
```

### 6. 异步请求

对于高并发场景，使用异步请求提高吞吐量。

```python
import asyncio
import aiohttp

async def lookup_ip(session, ip):
    async with session.get(f'http://localhost:8080/lookup?ip={ip}') as response:
        return await response.json()

async def main():
    ips = ['8.8.8.8', '1.1.1.1', '114.114.114.114']
    
    async with aiohttp.ClientSession() as session:
        tasks = [lookup_ip(session, ip) for ip in ips]
        results = await asyncio.gather(*tasks)
        
        for result in results:
            print(f"{result['ip']}: {result['country']}")

asyncio.run(main())
```

### 7. 监控和日志

记录 API 调用和响应时间，便于监控和调试。

```python
import requests
import time
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def lookup_ip(ip):
    start_time = time.time()
    
    try:
        response = requests.get(f'http://localhost:8080/lookup?ip={ip}')
        elapsed_time = time.time() - start_time
        
        logger.info(f"Lookup {ip} - Status: {response.status_code}, Time: {elapsed_time:.3f}s")
        
        response.raise_for_status()
        return response.json()
    except Exception as e:
        logger.error(f"Lookup {ip} failed: {e}")
        raise
```

### 8. 速率限制处理

尊重服务器的速率限制，避免被拒绝访问。

```python
import requests
import time

def lookup_ip_with_rate_limit(ip, max_requests_per_second=10):
    response = requests.get(f'http://localhost:8080/lookup?ip={ip}')
    
    # 检查速率限制
    if response.status_code == 429:
        retry_after = int(response.headers.get('Retry-After', 1))
        time.sleep(retry_after)
        return lookup_ip_with_rate_limit(ip)
    
    response.raise_for_status()
    return response.json()
```

## 更多示例

### 完整的 Python 客户端类

```python
import requests
from typing import List, Dict, Optional
import time
from functools import lru_cache

class IpLookupClient:
    def __init__(
        self,
        base_url: str = 'http://localhost:8080',
        api_key: Optional[str] = None,
        timeout: int = 5,
        max_retries: int = 3
    ):
        self.base_url = base_url
        self.api_key = api_key
        self.timeout = timeout
        self.max_retries = max_retries
        self.session = requests.Session()
        
        if api_key:
            self.session.headers.update({'X-API-Key': api_key})

    def lookup_single_ip(self, ip: str) -> Dict:
        """查询单个 IP 地址"""
        url = f'{self.base_url}/lookup?ip={ip}'
        
        for attempt in range(self.max_retries):
            try:
                response = self.session.get(url, timeout=self.timeout)
                response.raise_for_status()
                return response.json()
            except requests.exceptions.RequestException as e:
                if attempt == self.max_retries - 1:
                    raise
                time.sleep(2 ** attempt)

    def lookup_batch(self, ips: List[str]) -> List[Dict]:
        """批量查询 IP 地址"""
        url = f'{self.base_url}/lookup'
        
        for attempt in range(self.max_retries):
            try:
                response = self.session.post(
                    url,
                    json={'ips': ips},
                    timeout=self.timeout
                )
                response.raise_for_status()
                return response.json()
            except requests.exceptions.RequestException as e:
                if attempt == self.max_retries - 1:
                    raise
                time.sleep(2 ** attempt)

    def get_health(self) -> Dict:
        """获取服务健康状态"""
        response = self.session.get(f'{self.base_url}/health', timeout=self.timeout)
        response.raise_for_status()
        return response.json()

    def get_metrics(self) -> Dict:
        """获取性能指标"""
        response = self.session.get(f'{self.base_url}/metrics', timeout=self.timeout)
        response.raise_for_status()
        return response.json()

# 使用示例
if __name__ == '__main__':
    client = IpLookupClient(api_key='your_api_key')
    
    # 单个 IP 查询
    result = client.lookup_single_ip('8.8.8.8')
    print(f"{result['ip']}: {result['country']}, {result['city']}")
    
    # 批量查询
    results = client.lookup_batch(['8.8.8.8', '1.1.1.1'])
    for r in results:
        print(f"{r['ip']}: {r['country']}, {r['city']}")
    
    # 获取健康状态
    health = client.get_health()
    print(f"Health: {health['status']}")
    
    # 获取性能指标
    metrics = client.get_metrics()
    print(f"Total requests: {metrics['total_requests']}")
    print(f"Cache hit rate: {metrics['cache_hit_rate']:.2%}")
```

## 总结

本文档提供了 IP 位置信息查询服务的完整 API 使用指南，包括：

- 所有 API 端点的详细说明
- 多种编程语言的使用示例
- 错误处理和最佳实践
- 完整的客户端实现示例

如有任何问题或建议，请参考项目的 GitHub 仓库或联系开发团队。