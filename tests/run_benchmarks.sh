#!/bin/bash
# Benchmark test runner script for IP Geolocation Service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if benchmark executable exists
BENCHMARK_BIN="./build/bin/ip_server_benchmarks"
if [ ! -f "$BENCHMARK_BIN" ]; then
    echo -e "${RED}Error: Benchmark executable not found at $BENCHMARK_BIN${NC}"
    echo "Please build the project first with: cmake .. -DBUILD_BENCHMARKS=ON && cmake --build ."
    exit 1
fi

# Check if database files exist
if [ ! -f "db/GeoLite2-City.mmdb" ] || [ ! -f "db/GeoLite2-ASN.mmdb" ]; then
    echo -e "${RED}Error: Database files not found in db/ directory${NC}"
    echo "Please download GeoLite2 database files from https://dev.maxmind.com/geoip/geolite2-free-geolocation-data"
    exit 1
fi

echo -e "${GREEN}=== IP Geolocation Service Benchmark Tests ===${NC}"
echo ""

# Function to run benchmark with description
run_benchmark() {
    local filter=$1
    local description=$2

    echo -e "${YELLOW}Running: $description${NC}"
    echo "Filter: $filter"
    echo ""

    $BENCHMARK_BIN --benchmark_filter="$filter" 2>&1 | grep -E "^(Benchmark|DatabaseBenchmark)" || true

    echo ""
    echo "---"
    echo ""
}

# Run individual benchmark groups
run_benchmark "CityDatabase_SingleLookup" "City Database Single Lookup"
run_benchmark "ASNDatabase_SingleLookup" "ASN Database Single Lookup"
run_benchmark "CityDatabase_MultipleLookup" "City Database Multiple Lookup"
run_benchmark "ASNDatabase_MultipleLookup" "ASN Database Multiple Lookup"
run_benchmark "IPGeoService_WithCache" "IPGeoService With Cache"
run_benchmark "IPGeoService_WithoutCache" "IPGeoService Without Cache"
run_benchmark "IPGeoService_CacheHit" "IPGeoService Cache Hit"
run_benchmark "IPGeoService_CacheOperations" "IPGeoService Cache Operations"
run_benchmark "CityDatabase_OpenClose" "City Database Open/Close"
run_benchmark "ASNDatabase_OpenClose" "ASN Database Open/Close"
run_benchmark "IPGeoService_Initialization" "IPGeoService Initialization"
run_benchmark "CityDatabase_IPv6Lookup" "City Database IPv6 Lookup"
run_benchmark "IPGeoService_ConcurrentLookups" "IPGeoService Concurrent Lookups"

echo -e "${GREEN}=== All benchmark tests completed ===${NC}"
echo ""
echo "To run all benchmarks at once:"
echo "  $BENCHMARK_BIN"
echo ""
echo "To run specific benchmarks:"
echo "  $BENCHMARK_BIN --benchmark_filter=<pattern>"
echo ""
echo "For more options:"
echo "  $BENCHMARK_BIN --help"