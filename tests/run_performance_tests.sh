#!/bin/bash

# Performance Testing Script for IP Geolocation Service
# Usage: ./run_performance_tests.sh [OPTIONS]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BENCHMARK_MODE="all"
OUTPUT_DIR="${PROJECT_ROOT}/benchmark_results"
ITERATIONS=3
WARMUP_RUNS=1

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --benchmark)
            BENCHMARK_MODE="$2"
            shift 2
            ;;
        --iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --benchmark MODE   Run specific benchmark: all, database, cache, rate_limiter, batch, memory, auth, json, concurrent"
            echo "  --iterations N     Number of iterations for each benchmark (default: 3)"
            echo "  --output DIR       Output directory for results (default: ./benchmark_results)"
            echo "  --help             Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  IP Geolocation Service${NC}"
echo -e "${BLUE}  Performance Testing Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory not found. Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# Build benchmarks
echo -e "${GREEN}[1/4] Building benchmarks...${NC}"
cd "$BUILD_DIR"

# Enable benchmarks in CMake
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 || {
    echo -e "${RED}Failed to configure CMake${NC}"
    exit 1
}

make -j$(nproc) ip_server_benchmarks 2>&1 | grep -E "(error|warning:|Built target)" || true

if [ ! -f "$BUILD_DIR/bin/ip_server_benchmarks" ]; then
    echo -e "${RED}Failed to build benchmarks${NC}"
    exit 1
fi

echo -e "${GREEN}    Build complete${NC}"

# Run benchmarks
echo ""
echo -e "${GREEN}[2/4] Running performance benchmarks...${NC}"

BENCHMARK_FILTER=".*"
case $BENCHMARK_MODE in
    database)
        BENCHMARK_FILTER="DatabaseBenchmark.*"
        ;;
    cache)
        BENCHMARK_FILTER="CacheBenchmark.*"
        ;;
    rate_limiter)
        BENCHMARK_FILTER="RateLimiterBenchmark.*"
        ;;
    batch)
        BENCHMARK_FILTER="BatchLookupBenchmark.*"
        ;;
    memory)
        BENCHMARK_FILTER="MemoryBenchmark.*"
        ;;
    auth)
        BENCHMARK_FILTER="AuthBenchmark.*"
        ;;
    json)
        BENCHMARK_FILTER="JSONBenchmark.*"
        ;;
    concurrent)
        BENCHMARK_FILTER="ConcurrentBenchmark.*"
        ;;
    mac)
        BENCHMARK_FILTER="MACDatabaseBenchmark.*"
        ;;
    all)
        BENCHMARK_FILTER=".*"
        ;;
esac

# Run benchmark
OUTPUT_FILE="${OUTPUT_DIR}/benchmark_results_$(date +%Y%m%d_%H%M%S).json"
"$BUILD_DIR/bin/ip_server_benchmarks" \
    --benchmark_filter="$BENCHMARK_FILTER" \
    --benchmark_repetitions=$ITERATIONS \
    --benchmark_report_aggregates_only=true \
    --benchmark_out="$OUTPUT_FILE" \
    --v=0

echo -e "${GREEN}    Results saved to: $OUTPUT_FILE${NC}"

# Generate HTML report
echo ""
echo -e "${GREEN}[3/4] Generating HTML report...${NC}"

HTML_REPORT="${OUTPUT_DIR}/report_$(date +%Y%m%d_%H%M%S).html"
cat > "$HTML_REPORT" << 'HTML_END'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>IP Geolocation Service - Performance Report</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        h1 {
            color: #333;
            border-bottom: 2px solid #007bff;
            padding-bottom: 10px;
        }
        h2 {
            color: #555;
            margin-top: 30px;
        }
        .summary {
            background: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            background: white;
            border-radius: 8px;
            overflow: hidden;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        th, td {
            padding: 12px 15px;
            text-align: left;
            border-bottom: 1px solid #eee;
        }
        th {
            background-color: #007bff;
            color: white;
            font-weight: 600;
        }
        tr:hover {
            background-color: #f8f9fa;
        }
        .metric {
            font-family: monospace;
            background: #f8f9fa;
            padding: 2px 6px;
            border-radius: 4px;
        }
        .badge {
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
        }
        .badge-good { background-color: #28a745; color: white; }
        .badge-warning { background-color: #ffc107; color: #333; }
        .badge-bad { background-color: #dc3545; color: white; }
        .footer {
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
            color: #666;
            font-size: 14px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 IP Geolocation Service - Performance Report</h1>
HTML_END

# Extract and format results from JSON
echo "<div class='summary'>" >> "$HTML_REPORT"
echo "<h2>📊 Benchmark Summary</h2>" >> "$HTML_REPORT"
echo "<p><strong>Generated:</strong> $(date)</p>" >> "$HTML_REPORT"
echo "<p><strong>Benchmark Mode:</strong> $BENCHMARK_MODE</p>" >> "$HTML_REPORT"
echo "<p><strong>Iterations:</strong> $ITERATIONS</p>" >> "$HTML_REPORT"
echo "</div>" >> "$HTML_REPORT"

# Parse JSON results and create tables
echo "<h2>📈 Detailed Results</h2>" >> "$HTML_REPORT"

# Group benchmarks by category
echo "<h3>Database Operations</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'DatabaseBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000  # Convert to ms
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>Cache Performance</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'CacheBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>Rate Limiter Performance</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'RateLimiterBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>Batch Lookup Performance</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'BatchLookupBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>Memory Usage</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'MemoryBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>JSON Operations</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'JSONBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

echo "<h3>Concurrent Access</h3>" >> "$HTML_REPORT"
python3 -c "
import json
import sys

try:
    with open('$OUTPUT_FILE', 'r') as f:
        data = json.load(f)

    print('<table>')
    print('<tr><th>Benchmark</th><th>Mean</th><th>Std Dev</th><th>Min</th><th>Max</th><th>Iterations</th></tr>')

    for run in data.get('benchmarks', []):
        name = run.get('run_name', '')
        if 'ConcurrentBenchmark' in name and 'real_time' in run:
            mean = run.get('aggregate_name', {}).get('mean', 0) * 1000
            stddev = run.get('aggregate_name', {}).get('stddev', 0) * 1000
            min_val = run.get('aggregate_name', {}).get('min', 0) * 1000
            max_val = run.get('aggregate_name', {}).get('max', 0) * 1000
            iterations = run.get('run_iterations', 0)

            print(f'<tr><td>{name}</td><td class=\"metric\">{mean:.4f} ms</td><td>{stddev:.4f} ms</td><td>{min_val:.4f} ms</td><td>{max_val:.4f} ms</td><td>{iterations}</td></tr>')

    print('</table>')
except Exception as e:
    print(f'<p>Error parsing results: {e}</p>')
" >> "$HTML_REPORT"

cat >> "$HTML_REPORT" << 'HTML_END'

        <div class="footer">
            <p>Generated by IP Geolocation Service Performance Testing Suite</p>
            <p>For more details, check the JSON output files in the benchmark_results directory.</p>
        </div>
    </div>
</body>
</html>
HTML_END

echo -e "${GREEN}    HTML report saved to: $HTML_REPORT${NC}"

# Print summary
echo ""
echo -e "${GREEN}[4/4] Summary${NC}"
echo ""
echo -e "${BLUE}Benchmark Categories:${NC}"
echo "  • Database Operations (City/ASN lookup)"
echo "  • Cache Performance (Get/Put/Eviction)"
echo "  • Rate Limiter Performance"
echo "  • Batch Lookup Performance"
echo "  • Memory Usage"
echo "  • JSON Serialization"
echo "  • Concurrent Access"
echo ""
echo -e "${BLUE}Output Files:${NC}"
echo "  • JSON Results: $OUTPUT_FILE"
echo "  • HTML Report: $HTML_REPORT"
echo ""
echo -e "${GREEN}✅ Performance testing complete!${NC}"
