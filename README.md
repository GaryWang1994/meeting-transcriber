# 会议录音转文字工具 (Meeting Transcriber)

基于 Qwen3-ASR-1.7B 模型的智能会议录音转文字工具，支持自动识别多个说话人。

## 功能特点

- **高质量语音识别**: 基于阿里巴巴 Qwen3-ASR-1.7B 模型
- **自动说话人分离**: 智能识别不同说话人
- **多格式支持**: 支持 m4a、mp3、wav、flac 等音频格式
- **多种输出格式**: 支持 Markdown、纯文本、JSON、CSV、HTML
- **时间戳标记**: 每个片段都有精确的时间戳
- **GPU 加速**: 支持 NVIDIA GPU 加速推理

## 系统要求

### 最低配置
- Windows 10/11 或 Linux
- 8GB 内存
- 4GB 可用磁盘空间

### 推荐配置
- Windows 10/11 64位
- 16GB+ 内存
- NVIDIA GPU 4GB+ 显存（用于GPU加速）
- SSD 存储

## 安装说明

### Windows 安装

1. **下载预编译版本**
   - 从 Releases 页面下载 `MeetingTranscriber-v1.0.0-win64.zip`
   - 解压到任意目录
   - ✅ **FFmpeg 已内置**，无需单独安装任何依赖

2. **下载模型文件**
   - 从 [ModelScope](https://modelscope.cn/models/Qwen/Qwen3-ASR-1.7B) 下载ONNX格式模型，或使用 `tools/convert_to_onnx.py` 脚本自行转换
   - 模型放置方式：在程序目录创建 `models` 文件夹，将所有ONNX模型和配置文件放入其中

3. **添加到环境变量**（可选）
   - 将解压目录添加到 PATH 环境变量

### Linux 从源码编译

```bash
# 克隆仓库
git clone https://github.com/yourusername/meeting-transcriber.git
cd meeting-transcriber

# 安装依赖
sudo apt-get update
sudo apt-get install -y cmake build-essential pkg-config
sudo apt-get install -y libavcodec-dev libavformat-dev libavutil-dev libswresample-dev

# 下载ONNX Runtime
wget https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-linux-x64-1.16.3.tgz
tar -xzf onnxruntime-linux-x64-1.16.3.tgz
sudo cp -r onnxruntime-linux-x64-1.16.3/include/* /usr/local/include/
sudo cp -r onnxruntime-linux-x64-1.16.3/lib/* /usr/local/lib/

# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

## 使用方法

### 基本使用

```bash
# 基本转换
MeetingTranscriber meeting.m4a

# 指定输出文件
MeetingTranscriber meeting.m4a -o output.md

# 指定说话人数
MeetingTranscriber meeting.m4a --speakers 3

# 使用GPU加速
MeetingTranscriber meeting.m4a --gpu
```

### 高级选项

```bash
# 指定语言和模型
MeetingTranscriber meeting.m4a -l zh -m /path/to/model.onnx

# 输出不同格式
MeetingTranscriber meeting.m4a -f json
MeetingTranscriber meeting.m4a -f csv
MeetingTranscriber meeting.m4a -f html

# 自定义处理
MeetingTranscriber meeting.m4a \
    --no-timestamps \
    --no-speaker-labels \
    --threads 8
```

### 输出格式示例

**Markdown (默认):**
```markdown
# 会议记录

## 会议信息

- **音频文件**: meeting.m4a
- **时长**: 32分15秒
- **处理时间**: 2024-01-15 10:30:00

## 参会人员

- **Speaker A** (发言时长: 12分30秒, 15 段)
- **Speaker B** (发言时长: 10分45秒, 12 段)
- **Speaker C** (发言时长: 8分20秒, 10 段)

## 会议内容

[00:00:00] **Speaker A**: 大家好，我们开始今天的会议。

[00:00:05] **Speaker B**: 好的，我先汇报一下上个月的进度...
```

**JSON:**
```json
{
  "sourceFile": "meeting.m4a",
  "duration": 1935.0,
  "processedAt": "2024-01-15T10:30:00",
  "speakers": [...],
  "segments": [...]
}
```

## 模型下载

### 自动下载
```bash
python python/download_model.py --model Qwen/Qwen3-ASR-1.7B
```

### 手动下载
1. 访问 [ModelScope](https://modelscope.cn/models/Qwen/Qwen3-ASR-1.7B)
2. 下载模型文件
3. 解压到 `models/` 目录

### 模型格式转换
如果需要将模型转换为ONNX格式：
```bash
python python/convert_to_onnx.py \
    --model_path /path/to/Qwen3-ASR-1.7B \
    --output_path models/qwen3-asr-1.7b.onnx
```

## 性能优化

### 1. 使用GPU加速
```bash
MeetingTranscriber meeting.m4a --gpu
```

### 2. 调整线程数
```bash
MeetingTranscriber meeting.m4a --threads 8
```

### 3. 批量处理
```bash
# 处理多个文件
for file in *.m4a; do
    MeetingTranscriber "$file" &
done
wait
```

## 故障排除

### 常见问题

**1. 无法加载模型**
```
错误: Failed to load model
解决: 检查模型路径是否正确，模型文件是否完整
```

**2. 音频解码失败**
```
错误: Failed to decode audio
解决: 安装FFmpeg，或转换音频格式为WAV
```

**3. GPU内存不足**
```
错误: CUDA out of memory
解决: 使用--gpu选项时降低batch size，或改用CPU
```

### 调试模式
```bash
MeetingTranscriber meeting.m4a -v
```

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 致谢

- [Qwen3-ASR](https://modelscope.cn/models/Qwen/Qwen3-ASR-1.7B) - 语音识别模型
- [ONNX Runtime](https://onnxruntime.ai/) - 推理引擎
- [FFmpeg](https://ffmpeg.org/) - 音频处理

## 联系方式

- 项目主页: https://github.com/yourusername/meeting-transcriber
- 问题反馈: https://github.com/yourusername/meeting-transcriber/issues
- 邮箱: your.email@example.com