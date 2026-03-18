# 构建指南

## Windows 构建说明

### 前置要求

1. **Visual Studio 2019/2022** (社区版即可)
   - 安装 "使用C++的桌面开发" 工作负载
   - 包含 CMake 工具

2. **CMake 3.18+**
   - 可从 https://cmake.org/download/ 下载

3. **依赖库**
   - FFmpeg (必需，用于音频解码)
   - ONNX Runtime (可选，用于ASR推理)

### 安装依赖

#### 1. FFmpeg

**方法1: 使用预编译包**
```powershell
# 下载 FFmpeg Windows builds
# 从 https://www.gyan.dev/ffmpeg/builds/ 下载 release-full 版本
# 解压到 C:\ffmpeg 或其他位置

# 设置环境变量
setx PATH "%PATH%;C:\ffmpeg\bin"
```

**方法2: 使用 vcpkg**
```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install ffmpeg[avcodec,avformat,avutil,swresample]:x64-windows
```

#### 2. ONNX Runtime

```powershell
# 下载 ONNX Runtime
# 从 https://github.com/microsoft/onnxruntime/releases 下载 Windows x64 版本
# 例如: onnxruntime-win-x64-1.16.3.zip

# 解压到项目目录
mkdir .\third_party\onnxruntime
# 将解压后的文件复制到上述目录
```

### 构建项目

#### 使用 CMake GUI

1. 打开 CMake GUI
2. 设置源代码目录: `C:/path/to/meeting-transcriber`
3. 设置构建目录: `C:/path/to/meeting-transcriber/build`
4. 点击 "Configure"
   - 选择 Visual Studio 17 2022 (或 2019)
   - 选择 x64 平台
5. 等待配置完成
6. 设置依赖路径（如果需要）:
   - `FFMPEG_INCLUDE_DIR`: FFmpeg 头文件路径
   - `FFMPEG_LIBRARIES`: FFmpeg 库路径
   - `ONNXRUNTIME_INCLUDE_DIR`: ONNX Runtime 头文件路径
   - `ONNXRUNTIME_LIBRARY`: ONNX Runtime 库文件
7. 点击 "Generate"
8. 点击 "Open Project" 或打开生成的 .sln 文件
9. 在 Visual Studio 中，选择 Release 配置
10. 构建 -> 生成解决方案 (Ctrl+Shift+B)

#### 使用命令行

```powershell
# 进入项目目录
cd meeting-transcriber

# 创建构建目录
mkdir build
cd build

# 配置
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DFFMPEG_INCLUDE_DIR="C:/ffmpeg/include" `
    -DFFMPEG_LIBRARIES="C:/ffmpeg/lib" `
    -DONNXRUNTIME_INCLUDE_DIR="../third_party/onnxruntime/include" `
    -DONNXRUNTIME_LIBRARY="../third_party/onnxruntime/lib/onnxruntime.lib"

# 构建
cmake --build . --config Release --parallel

# 安装（可选）
cmake --install . --config Release
```

### 创建可执行文件包

```powershell
# 创建发布目录
mkdir MeetingTranscriber-v1.0.0-win64
cd MeetingTranscriber-v1.0.0-win64

# 复制可执行文件
copy ..\build\Release\MeetingTranscriber.exe .

# 复制依赖DLL
# FFmpeg DLLs
copy C:\ffmpeg\bin\avcodec-60.dll .
copy C:\ffmpeg\bin\avformat-60.dll .
copy C:\ffmpeg\bin\avutil-58.dll .
copy C:\ffmpeg\bin\swresample-4.dll .

# ONNX Runtime DLL (如果使用)
copy ..\third_party\onnxruntime\lib\onnxruntime.dll .

# Visual C++ Redistributable
# 可以从 https://aka.ms/vs/17/release/vc_redist.x64.exe 下载

# 创建模型目录
mkdir models

# 复制Python桥接脚本（可选）
mkdir python
copy ..\python\asr_bridge.py python\

# 创建README
copy ..\README.md .

# 打包
cd ..
Compress-Archive -Path MeetingTranscriber-v1.0.0-win64 -DestinationPath MeetingTranscriber-v1.0.0-win64.zip
```

## Linux 构建

### Ubuntu/Debian

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

# 下载ONNX Runtime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-linux-x64-1.16.3.tgz
tar -xzf onnxruntime-linux-x64-1.16.3.tgz

# 构建
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DONNXRUNTIME_INCLUDE_DIR=../onnxruntime-linux-x64-1.16.3/include \
    -DONNXRUNTIME_LIBRARY=../onnxruntime-linux-x64-1.16.3/lib/libonnxruntime.so

make -j$(nproc)
sudo make install
```

## macOS 构建

```bash
# 安装依赖
brew install cmake ffmpeg onnxruntime

# 构建
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
make install
```

## 故障排除

### 常见问题

1. **找不到 FFmpeg**
   - 确保设置了正确的 FFMPEG_INCLUDE_DIR 和 FFMPEG_LIBRARIES
   - 或者使用 vcpkg 管理依赖

2. **ONNX Runtime 链接错误**
   - 确保使用与编译器匹配的 ONNX Runtime 版本
   - 检查是否正确设置了运行时库路径

3. **运行时缺少 DLL**
   - 确保所有依赖的 DLL 都在 PATH 中或与可执行文件在同一目录
   - 使用工具如 Dependency Walker 检查依赖

4. **中文显示乱码**
   - 确保源代码文件使用 UTF-8 编码
   - Windows 控制台使用 `chcp 65001` 切换到 UTF-8
   - 代码中使用 `SetConsoleOutputCP(CP_UTF8)`

### 获取帮助

如果遇到问题，请：
1. 查看 README.md 中的故障排除部分
2. 检查日志输出（使用 `-v` 选项）
3. 提交 Issue 到项目仓库