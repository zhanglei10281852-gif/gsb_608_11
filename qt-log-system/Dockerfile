# ============================================================================
# Qt Log System - Multi-stage Docker Build
# 基础镜像选用跨平台版本，同时支持 ARM 和 X86
# ============================================================================

# Stage 1: Build + Test
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# 替换为阿里云镜像源
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list \
    && sed -i 's|http://security.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt .
COPY src/ src/
COPY tests/ tests/

# 编译主程序 + 测试
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel $(nproc)

# 运行测试（测试失败则构建中断）
RUN cd build && ctest --output-on-failure --verbose

# Stage 2: Runtime（仅包含主程序）
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list \
    && sed -i 's|http://security.ubuntu.com|http://mirrors.aliyun.com|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6core6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/QtLogSystem .

ENTRYPOINT ["./QtLogSystem"]
