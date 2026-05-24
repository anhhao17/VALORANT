#!/bin/bash
set -e

# Build script for Jetson BMCweb

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "=========================================="
echo "Building Jetson BMCweb"
echo "=========================================="
echo "Build type: ${BUILD_TYPE}"
echo ""

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ..
cmake --build . -- -j$(nproc)

echo ""
echo "✅ Build complete! Binary: ${BUILD_DIR}/jetson"
echo ""
echo "To run the application:"
echo "  ./build/jetson"
