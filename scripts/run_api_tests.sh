#!/bin/bash

# API Test Runner for Jetson BMCweb
# This script runs the FastAPI/pytest tests against the C++ backend

set -e

echo "=========================================="
echo "Jetson BMCweb API Test Runner"
echo "=========================================="

# Check if backend is running
echo "Checking if backend is running on http://localhost:8080..."
if ! curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/api/system/info > /dev/null 2>&1; then
    echo "❌ Backend is not running. Please start it first:"
    echo "   ./build/jetson --config config.yml"
    echo ""
    echo "Starting backend in background..."
    cd /home/hao/app/jetson
    ./build/jetson --config config.yml &
    BACKEND_PID=$!
    echo "Backend started with PID: $BACKEND_PID"
    echo "Waiting for backend to be ready..."
    sleep 5
fi

# Check if Python dependencies are installed
echo "Checking Python dependencies..."
if ! python3 -c "import requests" 2>/dev/null; then
    echo "Installing Python dependencies..."
    pip3 install -r /home/hao/app/jetson/tests/requirements.txt
fi

# Run the tests
echo ""
echo "Running API tests..."
echo "=========================================="

cd /home/hao/app/jetson/tests

# Run with different verbosity levels based on arguments
if [ "$1" == "-v" ]; then
    python3 -m pytest api_tests.py -v --tb=short
elif [ "$1" == "-vv" ]; then
    python3 -m pytest api_tests.py -vv --tb=long
else
    python3 -m pytest api_tests.py -v --tb=short
fi

TEST_RESULT=$?

echo ""
echo "=========================================="
if [ $TEST_RESULT -eq 0 ]; then
    echo "✅ All API tests passed!"
else
    echo "❌ Some API tests failed"
fi
echo "=========================================="

# Cleanup if we started the backend
if [ ! -z "$BACKEND_PID" ]; then
    echo "Stopping backend (PID: $BACKEND_PID)..."
    kill $BACKEND_PID 2>/dev/null || true
fi

exit $TEST_RESULT
