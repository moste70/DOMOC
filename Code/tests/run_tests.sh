#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "📦 Compiling simple test (no dependencies needed)..."
mkdir -p "$BUILD_DIR"

g++ -std=c++14 -I"${SCRIPT_DIR}/../Base/include" \
    "${SCRIPT_DIR}/test_simple.cpp" \
    -o "${BUILD_DIR}/test_simple"

echo "✅ Running tests..."
"${BUILD_DIR}/test_simple"

echo ""
echo "✨ Tests completed!"
