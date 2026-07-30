# Technical Debt Analysis — IP Geolocation & AS Lookup Service

**Audit Date**: 2026-07-30  
**Version**: 2.0.0  
**Total Source**: 9,020 LOC (4,130 src/ + 4,890 tests/)  
**Test Coverage Ratio**: 118% test lines vs source lines

---

## Debt Score Summary

| Category | Score | Trend | Severity |
|----------|-------|-------|----------|
| Architecture Debt | 6/10 | Worsening | High |
| Infrastructure Debt | 9/10 | Stable | Critical |
| Testing Debt | 4/10 | Stable | Medium |
| Security Debt | 3/10 | Stable | Low |
| Performance Debt | 3/10 | Stable | Low |
| Documentation Debt | 5/10 | Worsening | Medium |
| Code Structure Debt | 4/10 | Improving | Low |

**Overall Debt Score**: **5/10** (Moderate)

---

## 1. Architecture Debt (Severity: High)

### 1.1 God Class: `IPGeoHTTPServer`

| Metric | Value | Threshold | Verdict |
|--------|-------|-----------|---------|
| Total lines | 767 | >300 | **Critical** |
| Public methods | 10 | — | OK |
| Private methods | 16 | — | OK |
| Member variables | 17 | — | OK |

**Responsibility breakdown** (6 distinct concerns in one class):

| Responsibility | Lines | Should be |
|---------------|-------|-----------|
| HTTP routing & request handling | 338-648 | Core class |
| CORS headers | 241-250 | Middleware/filter |
| Authentication (Bearer token) | 169-199 | Delegate to APIAuth |
| Rate limiting | 216-238, 741-764 | Delegate to RateLimiter |
| Input validation (IP/MAC format) | 23-99 | Separate utility/composable |
| Thread lifecycle management | 650-730 | Orchestrator role |
| Password generation HTTP binding | 521-648 | Separate handler |

**Impact**: Any change to routing, auth, rate limiting, or thread management requires modifying this single file, causing merge conflicts and high risk of regressions. Estimated cost: **8-12 hours/month** in coordination overhead.

### 1.2 Singleton Pattern (Logger, XDGPaths)

**Classes**: `Logger` (singleton via `static` local), `XDGPaths` (singleton)

**Impact**: 
- Unit tests cannot mock or replace the logger — all tests that exercise log output must rely on side effects
- Integration tests requiring custom XDG paths must set real environment variables
- Estimated cost: **4-6 hours/month** working around testability limitations

**Recommendation**: Introduce dependency injection via interface/abstract base, with a static default for production use.

### 1.3 Static Utility Classes (ConfigParser, PasswordGenerator)

**Classes**: `ConfigParser` (all static), `PasswordGenerator` (all static)

**Impact**:
- Cannot be mocked for testing
- No polymorphism — testing error paths requires constructing real configurations
- Better implemented as free functions in a namespace

---

## 2. Infrastructure Debt (Severity: Critical)

### 2.1 No CI/CD Pipeline

| Gap | Impact |
|-----|--------|
| No automated build verification | Broken builds detected only on manual `xmake build` |
| No automated test runner | Skipped tests never flagged |
| No static analysis | Complexity regressions invisible |
| No security scanning | Dependency vulnerabilities unchecked |

**Annual cost estimate**: 
- 1 broken build/month × 30 min recovery = **6 hours/year**
- 2 regression bugs/year from unchecked changes = **16 hours/year**
- Total: **22 hours/year**

### 2.2 No Deployment Automation

| Gap | Impact |
|-----|--------|
| No Dockerfile | Each deployment requires manual environment setup |
| No systemd unit | No automatic restart on failure |
| No deployment scripts | Manual scp/rsync for every release |

### 2.3 Unpinned Dependencies

All 9 dependencies use `add_requires("...")` without version pins:

| Package | Risk |
|---------|------|
| `cpp-httplib` | API-breaking changes in minor versions (header-only) |
| `spdlog` | v2.x may change sink API |
| `nlohmann_json` | v4 breaking changes in JSON merge/diff |
| `libmaxminddb`, `sqlite3`, `openssl3` | ABI changes require recompilation |

**Fix**: Pin all versions to known-good releases in `xmake.lua`.

---

## 3. Testing Debt (Severity: Medium)

### 3.1 Pre-existing Failing Test

```
[  FAILED  ] PasswordGeneratorTest.GenerateWithExcludeSimilarFalse
```

**Root cause**: The test likely expects no similar-looking characters when `exclude_similar=true`, but the character set exclusion logic may be inconsistent. Affects 1/216 tests (0.46%).

### 3.2 Skipped Tests (Database-Dependent)

| Test Suite | Tests Skipped | Condition |
|-----------|--------------|-----------|
| `DatabaseTest` | 44 | MaxMind .mmdb files missing |
| `MACDatabaseTest` | 18 | OUI .db file missing |
| `MACLookupServiceTest` | 9 | OUI .db file missing |
| `HTTPServerTest` | 25 | MaxMind .mmdb files missing |
| **Total** | **96** | **44.4% of all tests** |

**Impact**: Nearly half the test suite is non-functional without downloading ~100MB of database files. New developers cannot run the full test suite on first checkout.

**Recommendation**: Provide a test-only mock/fake database implementation that returns canned results without real MMDB files.

### 3.3 Missing Test Coverage

| Component | What's Untested | Risk |
|-----------|----------------|------|
| Signal handler | Signal delivery path, graceful shutdown | Medium |
| `ScopedTimer` | Early return timing accuracy | Low |
| `CacheStats::operator+=` | Weighted averaging correctness | Low |
| Batch async path (>10 items) | Thread creation, result ordering, error handling | Medium |
| Trusted proxy logic | X-Forwarded-For parsing with multiple proxies | Medium |

---

## 4. Code Structure Debt (Severity: Low, Improving)

### 4.1 Duplicated Trusted Proxy Logic

**Files**: `src/auth.h` lines 35-46, `src/http_server.cpp` lines 252-257

Both `APIAuth` and `IPGeoHTTPServer` implement `is_trusted_proxy()` independently. This was partially reduced in the previous refactoring round but the `APIAuth` copy remains.

### 4.2 DIP Violation — Concrete Database Dependencies

```
IPGeoService → CityDatabase (concrete)
            → ASNDatabase   (concrete)
            → IPCache       (concrete)
```

No abstract interface for databases makes it impossible to swap implementations (e.g., for testing). Each service owns concrete database instances by value.

### 4.3 Bloom Filter Bitset Memory

`BloomFilter` uses `std::array<std::atomic_flag, Bits>` — for 65536 bits this is 8KB (fine), but `estimated_size()` does an O(Bits) scan every call, which is wasteful for a probabilistic data structure. Could use a running count or sampled estimate.

---

## 5. Security Debt (Severity: Low)

### 5.1 Manual IPv6 Parsing

`is_valid_ipv6()` in `http_server.cpp` (lines 40-71) is a hand-written parser that:
- Does not handle IPv4-mapped IPv6 (`::ffff:192.168.0.1`)
- Does not validate compressed zones (`fe80::1%eth0`)
- Uses `isxdigit()` which is locale-dependent

**Recommendation**: Use `getaddrinfo()` with `AF_INET6` to delegate validation to the OS.

### 5.2 Incomplete Confusable Character Sets

`CONFUSING_UPPER = "IO"` is missing 'S' (confusable with 5) and 'Z' (confusable with 2).  
`CONFUSING_LOWER = "ilo"` is missing 's' (confusable with 5).

---

## 6. Performance Debt (Severity: Low)

### 6.1 `std::async` Per Item for Batches >10

In `handle_lookup_post()` (line 496), each item >10 in a batch spawns a new OS thread via `std::async(std::launch::async, ...)`. For a batch of 100, this creates 100 threads. A thread pool (already available via httplib) or a fixed-size worker pool would be more efficient.

**Cost**: ~80μs thread creation overhead per item × 100 items = 8ms (vs ~2μs with thread pool).

### 6.2 Rate Limiter Single Mutex Bottleneck

`RateLimiter` uses a single `mutable std::mutex` for all IP records. Under high load with many distinct IPs, this becomes a contention point. A sharded approach (like `IPCache` uses for the cache) would scale to more cores.

### 6.3 Metrics Percentile Sort on Every Health Check

`calculate_percentile()` (metrics.cpp line 140) sorts the entire latency deque on every call. For the `/health` endpoint polled every 5 seconds, this creates O(1000 log 1000) work per request. Could use an incremental/streaming percentile approximation (e.g., TDigest).

---

## 7. Documentation Debt (Severity: Medium)

| Gap | Impact |
|-----|--------|
| No API reference docs | Users must read source code for all endpoint details |
| No architecture diagram | Team members need to reverse-engineer the component graph |
| No onboarding guide | Setup requires reading 4+ files (AGENTS.md, config.h, etc.) |
| No deployment guide | Operations team has no documented procedure |
| No configuration reference | All config keys and defaults must be read from source |

---

## 8. Remediation Roadmap

### Phase 1 — Quick Wins (Week 1, Effort: 12h)

| # | Item | Effort | Savings | ROI |
|---|------|--------|---------|-----|
| 1 | Pin dependency versions in xmake.lua | 0.5h | Prevents build breaks | Immediate |
| 2 | Use `getaddrinfo()` for IPv6 validation | 1h | Security correctness | Immediate |
| 3 | Fix confusable character sets | 0.5h | Password quality | Immediate |
| 4 | Add mock/fake database for tests | 8h | 96 tests become runnable | **200%** |
| 5 | Set up GitHub Actions CI | 2h | Auto-build + test | **300%** |

**Total Phase 1**: 12 hours → unlocks 96 tests, provides CI, fixes 2 bugs

### Phase 2 — Core Refactoring (Month 1, Effort: 40h)

| # | Item | Effort | Savings | ROI |
|---|------|--------|---------|-----|
| 6 | Split `IPGeoHTTPServer` into focused components | 20h | 8-12h/month coordination | **50% monthly** |
| 7 | Abstract database interfaces for testability | 8h | Enables mock-based testing | 3-month payback |
| 8 | Replace `std::async` with thread pool | 4h | Batch perf +2-4x | 1-month payback |
| 9 | Add Dockerfile + docker-compose | 8h | Zero-config deployment | Immediate |

**Total Phase 2**: 40 hours → reduces monthly maintenance by 15-20h

### Phase 3 — Structural Improvements (Quarter 2, Effort: 60h)

| # | Item | Effort | Savings | ROI |
|---|------|--------|---------|-----|
| 10 | Dependency injection for Logger/XDGPaths | 12h | Enables unit testing of all log paths | Medium |
| 11 | Sharded rate limiter | 16h | Scales to 100k+ concurrent IPs | Low (edge case) |
| 12 | Streaming percentile metrics | 8h | Reduces health check CPU by 80% | Low |
| 13 | API documentation (OpenAPI spec) | 16h | First-class API docs for clients | Medium |
| 14 | Comprehensive integration tests | 8h | Covers batch, auth, proxy paths | Medium |

**Total Phase 3**: 60 hours → comprehensive testability + observability

---

## 9. Prevention Strategy

### Quality Gates (Future)

| Gate | Tool | Threshold |
|------|------|-----------|
| Static analysis | `cppcheck` or `clang-tidy` | No new warnings with `-Wall -Wextra -Wpedantic` |
| Complexity check | `lizard` or custom script | Cyclomatic complexity < 10 per function |
| Dependency audit | `xmake require --info` | No unpinned versions |
| Test coverage | `gcov` + `lcov` | >80% line coverage for new code |

### Debt Budget

- **Monthly debt increase limit**: 2%
- **Mandatory quarterly reduction**: 5%
- **Exception process**: Any PR that increases total debt requires CTO approval

---

## 10. Risk Register

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Dependency update breaks build | Medium | High | Pin versions (Phase 1 item 1) |
| New contributor cannot run tests | High | Medium | Mock database (Phase 1 item 4) |
| Batch lookup thread exhaustion | Low | Medium | Thread pool (Phase 2 item 8) |
| IPv6 validation rejects valid addresses | Low | Medium | getaddrinfo (Phase 1 item 2) |
| Rate limiter becomes bottleneck | Low | Low | Sharded design (Phase 3 item 11) |

---

## 11. ROI Projections

### Investment Summary

| Phase | Hours | Cost ($150/h) | Benefit |
|-------|-------|---------------|---------|
| Phase 1 (Quick Wins) | 12 | $1,800 | 96 tests unlocked, CI running |
| Phase 2 (Core) | 40 | $6,000 | 15-20h/month maintenance saved |
| Phase 3 (Structural) | 60 | $9,000 | Comprehensive testability |
| **Total** | **112** | **$16,800** | |

### Annual Savings

- Maintenance overhead reduction: **180 hours/year** @ $150 = **$27,000/year**
- Bug prevention (estimated 3-5 bugs/year): **60 hours/year** @ $150 = **$9,000/year**
- Developer onboarding time reduction: **20 hours/year** @ $150 = **$3,000/year**
- **Total annual savings**: **$39,000/year**

### Payback Period

Investment: $16,800 → Monthly savings: $3,250 → **Payback in 5.2 months**  
5-year ROI: **1,060%** ($195,000 savings on $16,800 investment)
