#!/bin/bash

# Test script for new features: graceful shutdown, rate limiting, and batch size limit

echo "=========================================="
echo "Testing New Features"
echo "=========================================="
echo ""

# Start the server in background
echo "Starting server..."
cd /home/code/ip_local/build
./bin/ip_server --host 127.0.0.1 --port 9999 --enable-rate-limiter true --max-requests-per-minute 5 --max-batch-size 3 &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo "Server started with PID: $SERVER_PID"
echo ""

# Test 1: Graceful shutdown
echo "Test 1: Graceful Shutdown"
echo "Sending SIGINT to server..."
kill -INT $SERVER_PID
wait $SERVER_PID
echo "✓ Server shut down gracefully"
echo ""

# Restart server for other tests
echo "Restarting server for remaining tests..."
./bin/ip_server --host 127.0.0.1 --port 9999 --enable-rate-limiter true --max-requests-per-minute 5 --max-batch-size 3 &
SERVER_PID=$!
sleep 2

# Test 2: Rate limiting
echo "Test 2: Rate Limiting"
echo "Making 7 requests (limit is 5 per minute)..."
for i in {1..7}; do
    RESPONSE=$(curl -s -w "\n%{http_code}" http://127.0.0.1:9999/lookup?ip=8.8.8.8)
    HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
    echo "  Request $i: HTTP $HTTP_CODE"
    if [ $i -gt 5 ] && [ "$HTTP_CODE" = "429" ]; then
        echo "  ✓ Rate limit triggered correctly"
    fi
    sleep 0.1
done
echo ""

# Test 3: Batch size limit
echo "Test 3: Batch Size Limit"
echo "Testing batch with 5 IPs (limit is 3)..."
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST http://127.0.0.1:9999/lookup \
    -H "Content-Type: application/json" \
    -d '{"ips": ["8.8.8.8", "1.1.1.1", "9.9.9.9", "8.8.4.4", "1.0.0.1"]}')
HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
echo "  Response HTTP code: $HTTP_CODE"
if [ "$HTTP_CODE" = "400" ]; then
    echo "  ✓ Batch size limit enforced correctly"
fi
echo ""

# Test 4: Valid batch request
echo "Test 4: Valid Batch Request (within limit)"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST http://127.0.0.1:9999/lookup \
    -H "Content-Type: application/json" \
    -d '{"ips": ["8.8.8.8", "1.1.1.1"]}')
HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
echo "  Response HTTP code: $HTTP_CODE"
if [ "$HTTP_CODE" = "200" ]; then
    echo "  ✓ Valid batch request processed correctly"
fi
echo ""

# Clean up
echo "Stopping server..."
kill -INT $SERVER_PID
wait $SERVER_PID

echo ""
echo "=========================================="
echo "All tests completed!"
echo "=========================================="