#!/bin/bash
set -e

echo "===================================="
echo "Meeting Transcriber Windows Build (Lightweight)"
echo "===================================="
echo ""

PROJECT_DIR="/home/garywang/Desktop/project/meeting-transcriber"
BUILD_DIR="$PROJECT_DIR/build-windows"

mkdir -p "$BUILD_DIR"

echo "[1/3] 验证编译工具..."
x86_64-w64-mingw32-g++ --version | head -1
echo "  ✓ MinGW-w64 可用"
echo ""

echo "[2/3] 配置 CMake (无 ONNX Runtime 模式)..."
cd "$BUILD_DIR"

export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++

cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DONNXRUNTIME_ROOT="" \
    2>&1 | tail -20

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo "错误: CMake 配置失败！"
    exit 1
fi

echo "  ✓ CMake 配置成功"
echo ""

echo "[3/3] 编译 Windows 可执行文件..."
make -j$(nproc) 2>&1 | tail -30

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "错误: 编译失败！"
    echo "正在尝试不使用 ONNX 重新配置..."
    
    cd "$PROJECT_DIR"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    echo "创建最小化 CMake 配置..."
    
    cat > "$BUILD_DIR/CMakeLists.txt" << 'MINIMAL_CMAKE'
cmake_minimum_required(VERSION 3.15)
project(MeetingTranscriber VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

# Windows cross-compile settings
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
endif()

# Source files (minimal version without ONNX)
set(SOURCES
    ../src/main.cpp
    ../src/audio_processor.cpp
    ../src/speaker_diarization.cpp
    ../src/transcript_generator.cpp
    ../src/utils.cpp
)

add_executable(meeting-transcriber ${SOURCES})

target_include_directories(meeting-transcriber PRIVATE ../include)

target_link_libraries(meeting-transcriber PRIVATE)
MINIMAL_CMAKE
    
    export CC=x86_64-w64-mingw32-gcc
    export CXX=x86_64-w64-mingw32-g++
    
    cmake . -DCMAKE_SYSTEM_NAME=Windows
    make -j$(nproc)
fi

echo ""
echo "===================================="
echo "构建结果:"
echo "===================================="

if [ -f "$BUILD_DIR/meeting-transcriber.exe" ]; then
    ls -lh "$BUILD_DIR/meeting-transcriber.exe"
    echo ""
    echo "✓ Windows 可执行文件创建成功!"
    echo "文件位置: $BUILD_DIR/meeting-transcriber.exe"
    echo ""
    
    # 创建简单 ZIP 包
    cd "$BUILD_DIR"
    if command -v zip &> /dev/null; then
        zip meeting-transcriber-windows-x64.zip meeting-transcriber.exe
        echo "ZIP 包已创建: $BUILD_DIR/meeting-transcriber-windows-x64.zip"
    fi
    
elif [ -f "$BUILD_DIR/bin/meeting-transcriber.exe" ]; then
    ls -lh "$BUILD_DIR/bin/meeting-transcriber.exe"
    echo ""
    echo "✓ Windows 可执行文件创建成功!"
    echo "文件位置: $BUILD_DIR/bin/meeting-transcriber.exe"
else
    echo "✗ 未找到生成的可执行文件"
    echo "可能编译过程中出现错误"
    exit 1
fi

echo "===================================="
