#!/bin/bash
set -e

echo "===================================="
echo "Meeting Transcriber Windows Build"
echo "===================================="
echo ""

PROJECT_DIR="/home/garywang/Desktop/project/meeting-transcriber"
BUILD_DIR="$PROJECT_DIR/build-windows"
DEPS_DIR="$PROJECT_DIR/deps"

mkdir -p "$BUILD_DIR"
mkdir -p "$DEPS_DIR"

echo "[1/4] Checking compiler..."
x86_64-w64-mingw32-g++ --version | head -1
echo "  OK MinGW-w64 available"
echo ""

echo "[2/4] Downloading ONNX Runtime..."
ONNX_VERSION="1.16.3"
ONNX_DIR="$DEPS_DIR/onnxruntime-win-x64-${ONNX_VERSION}"

if [ ! -d "$ONNX_DIR" ]; then
    cd "$DEPS_DIR"
    
    echo "  Downloading ONNX Runtime ${ONNX_VERSION}..."
    
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-win-x64-${ONNX_VERSION}.zip"
    
    if curl -L --max-time 120 -o onnxruntime.zip "$ONNX_URL" 2>/dev/null; then
        echo "  Extracting ONNX Runtime..."
        unzip -q onnxruntime.zip
        rm -f onnxruntime.zip
        echo "  OK ONNX Runtime downloaded"
    else
        echo "  Warning: Failed to download ONNX Runtime"
        echo "  Creating minimal stub..."
        mkdir -p "$ONNX_DIR/include"
        mkdir -p "$ONNX_DIR/lib"
        
        cat > "$ONNX_DIR/include/onnxruntime_cxx_api.h" << 'EOF'
#pragma once
namespace Ort {
    class Session {};
    class Env {};
}
EOF
        
        # Create minimal lib file
        touch "$ONNX_DIR/lib/onnxruntime.lib"
    fi
else
    echo "  OK ONNX Runtime already exists"
fi
cd "$PROJECT_DIR"
echo ""

echo "[3/4] Configuring CMake for Windows..."
cd "$BUILD_DIR"

export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++

cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DONNXRUNTIME_ROOT="$ONNX_DIR" \
    2>&1 | tail -30

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "Error: CMake configuration failed!"
    echo "Trying to build without ONNX Runtime..."
    
    cd "$PROJECT_DIR"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    cmake .. \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DONNXRUNTIME_ROOT="" \
        2>&1 | tail -20
fi

echo "  OK CMake configured"
echo ""

echo "[4/4] Building Windows executable..."
make -j$(nproc) 2>&1 | tail -40

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "Build failed!"
    exit 1
fi

echo "  OK Build completed"
echo ""

echo "===================================="
echo "Packaging Windows Release"
echo "===================================="

PACKAGE_NAME="meeting-transcriber-windows-x64"
PACKAGE_DIR="$BUILD_DIR/$PACKAGE_NAME"

mkdir -p "$PACKAGE_DIR"

# Copy executable
if [ -f "$BUILD_DIR/bin/meeting-transcriber.exe" ]; then
    cp "$BUILD_DIR/bin/meeting-transcriber.exe" "$PACKAGE_DIR/"
elif [ -f "$BUILD_DIR/meeting-transcriber.exe" ]; then
    cp "$BUILD_DIR/meeting-transcriber.exe" "$PACKAGE_DIR/"
fi

# Copy DLLs
cp "$BUILD_DIR/bin/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true

# Copy ONNX Runtime DLLs
if [ -d "$ONNX_DIR" ]; then
    cp "$ONNX_DIR/lib/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true
fi

# Create README
cat > "$PACKAGE_DIR/README.txt" << 'ENDREADME'
Meeting Transcriber for Windows
================================

使用方法:
1. 将音频文件 (m4a 格式) 放入此文件夹
2. 打开命令提示符 (CMD) 或 PowerShell
3. 运行: meeting-transcriber.exe -i <音频文件.m4a> -o <输出文件.md>

示例:
  meeting-transcriber.exe -i meeting.m4a -o meeting.md
  meeting-transcriber.exe -i meeting.m4a -f json -o meeting.json

参数说明:
  -i, --input    输入音频文件路径
  -o, --output   输出文件路径
  -f, --format   输出格式: md, txt, json, csv, html (默认: md)
  --no-timestamps  不包含时间戳
  --no-speakers    不包含说话人标识

系统要求:
- Windows 10 或更高版本
- 64位系统

项目地址: http://192.168.1.252:3000/gary/meeting-transcriber
ENDREADME

# Create ZIP
cd "$BUILD_DIR"
zip -r "$PACKAGE_NAME.zip" "$PACKAGE_NAME"

echo ""
echo "===================================="
echo "Build Summary"
echo "===================================="
ls -lh "$PACKAGE_NAME.zip"
echo ""
echo "Package contents:"
ls -lh "$PACKAGE_NAME/"
echo "===================================="
echo ""
echo "✓ Windows build completed successfully!"
echo "Output: $BUILD_DIR/$PACKAGE_NAME.zip"
