#!/bin/bash
set -e

# Build script for Jetson BMCweb

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"
WEBUI_DIR="webui"

echo "=========================================="
echo "Building Jetson BMCweb"
echo "=========================================="
echo "Build type: ${BUILD_TYPE}"
echo ""

# Build Backend
echo "=========================================="
echo "Building Backend (C++)"
echo "=========================================="
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ..
cmake --build . -- -j$(nproc)
echo "✅ Backend build complete! Binary: ${BUILD_DIR}/jetson"
echo ""

# Build Frontend
echo "=========================================="
echo "Building Frontend (Vue 3)"
echo "=========================================="
cd "../${WEBUI_DIR}"

# Check if node_modules exists, if not install dependencies
if [ ! -d "node_modules" ]; then
    echo "Installing frontend dependencies..."
    npm install
fi

echo "Building frontend..."
npm run build
echo "✅ Frontend build complete! Output: ${WEBUI_DIR}/dist"
echo ""

cd ".."

echo "=========================================="
echo "✅ Build Complete!"
echo "=========================================="
echo "Backend binary: ${BUILD_DIR}/jetson"
echo "Frontend dist: ${WEBUI_DIR}/dist"
echo ""
echo "To run the application:"
echo "  ./build/jetson"
echo ""
echo "The backend will serve the frontend UI on http://localhost:8080"
