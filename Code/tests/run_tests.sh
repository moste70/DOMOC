#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "🔨 Building host-based unit tests..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake ..
cmake --build . --config Debug

echo ""
echo "✅ Running tests..."
ctest --output-on-failure

echo ""
echo "✨ All tests passed!"
