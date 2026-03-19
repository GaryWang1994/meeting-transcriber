#!/bin/bash
# Windows 交叉编译脚本
# 使用方法: 在安装了 MinGW-w64 的 Linux 系统上运行此脚本

set -e

echo "=== Meeting Transcriber Windows Build Script ==="
echo ""

# 检查依赖
command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 || { echo "错误: 未找到 MinGW-w64。请安装 mingw-w64 包。"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "错误: 未找到 CMake。请安装 cmake 包。"; exit 1; }

echo "✓ 依赖检查通过"
echo ""

# 创建构建目录
BUILD_DIR="build-windows"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "=== 配置 CMake (Windows 交叉编译) ==="
# 配置为 Windows 交叉编译
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=./install

# 如果没有工具链文件，使用环境变量方式
if [ $? -ne 0 ]; then
    echo "使用环境变量方式进行交叉编译..."
    rm -rf CMakeCache.txt CMakeFiles
    
    CC=x86_64-w64-mingw32-gcc \
    CXX=x86_64-w64-mingw32-g++ \
    cmake .. \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
fi

echo ""
echo "=== 编译项目 ==="
make -j$(nproc)

echo ""
echo "=== 打包 Windows 版本 ==="
PACKAGE_NAME="meeting-transcriber-windows-x64"
PACKAGE_DIR="$PACKAGE_NAME"
mkdir -p "$PACKAGE_DIR"

# 复制可执行文件
cp bin/meeting-transcriber.exe "$PACKAGE_DIR/" 2>/dev/null || cp meeting-transcriber.exe "$PACKAGE_DIR/" 2>/dev/null || echo "警告: 未找到 .exe 文件"

# 创建 README
cat > "$PACKAGE_DIR/README.txt" << 'PACKAGEEOF'
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

更多信息请访问: https://github.com/gary/meeting-transcriber
PACKAGEEOF

# 创建 ZIP 包
zip -r "${PACKAGE_NAME}.zip" "$PACKAGE_DIR"

echo ""
echo "=== 构建完成! ==="
echo "输出文件: $(pwd)/${PACKAGE_NAME}.zip"
echo ""
echo "文件列表:"
ls -lh "${PACKAGE_NAME}.zip"
