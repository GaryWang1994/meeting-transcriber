#!/bin/bash
# Windows 交叉编译脚本 - 包含 ONNX Runtime 下载

set -e

echo "===================================="
echo "Meeting Transcriber Windows Build"
echo "===================================="
echo ""

# 项目根目录
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build-windows"
DEPS_DIR="$PROJECT_DIR/deps"

# 创建目录
mkdir -p "$BUILD_DIR"
mkdir -p "$DEPS_DIR"

echo "[1/6] 检查编译工具..."
# 检查 MinGW
if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
    echo "错误: 未找到 MinGW-w64。正在尝试安装..."
    sudo apt-get update -qq
    sudo apt-get install -y -qq mingw-w64 cmake zip curl
    
    if ! command -v x86_64-w64-mingw32-g++ &> /dev/null; then
        echo "错误: MinGW-w64 安装失败！"
        exit 1
    fi
fi

echo "  ✓ MinGW-w64: $(x86_64-w64-mingw32-g++ --version | head -1)"
echo "  ✓ CMake: $(cmake --version | head -1)"
echo ""

echo "[2/6] 下载 ONNX Runtime for Windows..."
ONNX_VERSION="1.16.3"
ONNX_DIR="$DEPS_DIR/onnxruntime-win-x64-${ONNX_VERSION}"

if [ ! -d "$ONNX_DIR" ]; then
    echo "  下载 ONNX Runtime ${ONNX_VERSION}..."
    cd "$DEPS_DIR"
    
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-win-x64-${ONNX_VERSION}.zip"
    
    if ! curl -L -o "onnxruntime.zip" "$ONNX_URL" 2>/dev/null; then
        echo "  警告: 从 GitHub 下载失败，尝试使用 wget..."
        if ! wget -q -O "onnxruntime.zip" "$ONNX_URL" 2>/dev/null; then
            echo "  错误: 无法下载 ONNX Runtime。将创建最小化构建..."
            # 创建最小化的 ONNX Runtime stub
            mkdir -p "$ONNX_DIR/include"
            mkdir -p "$ONNX_DIR/lib"
            touch "$ONNX_DIR/include/onnxruntime_cxx_api.h"
            echo "已创建最小化 ONNX Runtime 占位符"
        fi
    else
        echo "  解压 ONNX Runtime..."
        unzip -q "onnxruntime.zip"
        rm -f "onnxruntime.zip"
    fi
    
    cd "$PROJECT_DIR"
else
    echo "  ✓ ONNX Runtime 已存在"
fi

if [ -d "$ONNX_DIR" ]; then
    echo "  ✓ ONNX Runtime 目录: $ONNX_DIR"
fi
echo ""

echo "[3/6] 配置 CMake (Windows 交叉编译)..."
cd "$BUILD_DIR"

# 设置编译器
export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++

# 运行 CMake
cmake .. \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DONNXRUNTIME_ROOT="$ONNX_DIR" \
    2>&1 | tee cmake.log

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "错误: CMake 配置失败！"
    echo "查看 cmake.log 了解详情"
    exit 1
fi

echo "  ✓ CMake 配置成功"
echo ""

echo "[4/6] 编译项目..."
make -j$(nproc) 2>&1 | tee build.log

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo ""
    echo "错误: 编译失败！"
    echo "查看 build.log 了解详情"
    exit 1
fi

echo "  ✓ 编译成功"
echo ""

echo "[5/6] 打包 Windows 版本..."
PACKAGE_NAME="meeting-transcriber-windows-x64"
PACKAGE_DIR="$BUILD_DIR/$PACKAGE_NAME"

# 创建目录
mkdir -p "$PACKAGE_DIR"

# 复制可执行文件
if [ -f "$BUILD_DIR/bin/meeting-transcriber.exe" ]; then
    cp "$BUILD_DIR/bin/meeting-transcriber.exe" "$PACKAGE_DIR/"
elif [ -f "$BUILD_DIR/meeting-transcriber.exe" ]; then
    cp "$BUILD_DIR/meeting-transcriber.exe" "$PACKAGE_DIR/"
else
    echo "错误: 未找到可执行文件！"
    exit 1
fi

# 复制 DLL 依赖
cp "$BUILD_DIR/bin/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true
cp "$BUILD_DIR/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true

# 复制 ONNX Runtime DLL（如果有）
if [ -d "$ONNX_DIR" ]; then
    cp "$ONNX_DIR/lib/"*.dll "$PACKAGE_DIR/" 2>/dev/null || true
fi

# 创建 README
cat > "$PACKAGE_DIR/README.txt" << 'EOF'
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
EOF

# 创建 ZIP 包
cd "$BUILD_DIR"
zip -r "$PACKAGE_NAME.zip" "$PACKAGE_NAME"

echo "  ✓ 打包完成: $BUILD_DIR/$PACKAGE_NAME.zip"
echo ""

# 显示文件信息
echo "[6/6] 构建结果:"
echo "===================================="
ls -lh "$PACKAGE_NAME.zip"
echo ""
echo "文件列表:"
ls -lh "$PACKAGE_NAME/"
echo "===================================="
echo ""
echo "✓ Windows 版本构建成功！"
echo "输出文件: $BUILD_DIR/$PACKAGE_NAME.zip"
