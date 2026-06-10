@echo off
REM ============================================================================
REM Qt Log System - 一键 Docker 测试脚本 (Windows)
REM 用法: run_tests.bat
REM ============================================================================

echo ============================================
echo   Qt Log System - Docker Test Runner
echo ============================================
echo.

echo [1/3] Building test image...
docker build -t qt-log-test -f qt-log-system/Dockerfile.test qt-log-system
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FAILED] Docker build failed!
    exit /b 1
)

echo.
echo [2/3] Running tests...
echo.
docker run --rm qt-log-test -v2
set TEST_EXIT=%ERRORLEVEL%

echo.
echo [3/3] Cleaning up image...
docker rmi qt-log-test >nul 2>&1

echo.
if %TEST_EXIT% EQU 0 (
    echo ============================================
    echo   ALL TESTS PASSED
    echo ============================================
) else (
    echo ============================================
    echo   TESTS FAILED (exit code: %TEST_EXIT%)
    echo ============================================
)

exit /b %TEST_EXIT%
