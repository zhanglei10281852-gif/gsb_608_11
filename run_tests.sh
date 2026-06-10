#!/bin/bash
# ============================================================================
# Qt Log System - 一键 Docker 测试脚本 (Linux/macOS)
# 用法: chmod +x run_tests.sh && ./run_tests.sh
# ============================================================================

set -e

echo "============================================"
echo "  Qt Log System - Docker Test Runner"
echo "============================================"
echo ""

echo "[1/3] Building test image..."
docker build -t qt-log-test -f qt-log-system/Dockerfile.test qt-log-system

echo ""
echo "[2/3] Running tests..."
echo ""

TEST_EXIT=0
docker run --rm qt-log-test -v2 || TEST_EXIT=$?

echo ""
echo "[3/3] Cleaning up image..."
docker rmi qt-log-test > /dev/null 2>&1 || true

echo ""
if [ $TEST_EXIT -eq 0 ]; then
    echo "============================================"
    echo "  ALL TESTS PASSED"
    echo "============================================"
else
    echo "============================================"
    echo "  TESTS FAILED (exit code: $TEST_EXIT)"
    echo "============================================"
fi

exit $TEST_EXIT
