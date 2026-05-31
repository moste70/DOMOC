#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "📦 Installing dependencies..."
if command -v apt-get &> /dev/null; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq cmake g++ libgtest-dev 2>&1 | grep -v "^Get:\|^Hit:\|^Reading\|^Building" || true
fi

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
