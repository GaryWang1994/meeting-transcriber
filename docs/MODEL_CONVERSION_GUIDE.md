# Qwen3-ASR 模型 ONNX 转换指南

本指南教你如何将 Qwen3-ASR 模型从 safetensors/PyTorch 格式转换为 ONNX 格式，供 meeting-transcriber 程序使用。

---

## 📋 环境要求
- Python 3.8 ~ 3.10 (推荐 3.10，兼容性最好)
- 至少 8GB 内存（16GB 以上推荐）
- Windows/Linux/macOS 均可

---

## 🚀 快速开始

### 方法一：使用现成转换脚本（推荐）
仓库中已经提供了现成的转换脚本 `tools/convert_to_onnx.py`，可以直接使用。

#### 1. 安装依赖
```bash
# 创建虚拟环境（可选但推荐）
python -m venv venv
# Windows
venv\Scripts\activate
# Linux/macOS
source venv/bin/activate

# 安装依赖
pip install torch==2.1.2 transformers==4.39.3 modelscope==1.15.0
pip install onnx==1.15.0 onnxruntime==1.16.3 optimum==1.17.1

# GPU版本（可选，有NVIDIA显卡时安装，转换更快）
pip3 install torch==2.1.2 torchvision==0.16.2 torchaudio==2.1.2 --index-url https://download.pytorch.org/whl/cu118
```

#### 2. 下载原始模型
方法A：从 ModelScope 自动下载
```python
from modelscope import snapshot_download
model_dir = snapshot_download('Qwen/Qwen3-ASR-1.7B', cache_dir='./')
print(f"模型下载完成: {model_dir}")
```

方法B：手动下载
- 访问 https://modelscope.cn/models/Qwen/Qwen3-ASR-1.7B/files
- 下载所有文件到本地目录

#### 3. 运行转换脚本
```bash
# 基本用法
python tools/convert_to_onnx.py --model-path ./Qwen/Qwen3-ASR-1.7B

# 高级选项
python tools/convert_to_onnx.py \
  --model-path ./Qwen/Qwen3-ASR-1.7B \
  --output-dir ./my-onnx-model \
  --opset 17 \
  --fp16  # GPU用户可以开启FP16精度，模型更小速度更快
```

#### 4. 转换完成
转换后的文件会输出到指定目录（默认 `./qwen3-asr-onnx`），包含：
- `model.onnx` - 主模型文件
- 各种 `.json` 和 `.txt` 配置文件
- `README_CONVERSION.txt` - 转换说明

---

### 方法二：手动转换（适合高级用户）
如果脚本转换遇到问题，可以尝试手动转换：
```python
import torch
from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor

# 加载模型
model = AutoModelForSpeechSeq2Seq.from_pretrained(
    "Qwen/Qwen3-ASR-1.7B",
    trust_remote_code=True
)
model.eval()

# 导出ONNX
dummy_input = {
    "input_features": torch.randn(1, 80, 3000),
    "attention_mask": torch.ones(1, 3000, dtype=torch.long)
}

torch.onnx.export(
    model,
    (dummy_input["input_features"], dummy_input["attention_mask"]),
    "model.onnx",
    opset_version=17,
    input_names=["input_features", "attention_mask"],
    output_names=["logits"],
    dynamic_axes={
        "input_features": {0: "batch_size", 2: "sequence_length"},
        "attention_mask": {0: "batch_size", 1: "sequence_length"},
        "logits": {0: "batch_size", 1: "sequence_length"}
    }
)
```

---

## ✅ 验证模型
转换完成后可以简单验证模型是否正常：
```python
import onnxruntime as ort
import numpy as np

session = ort.InferenceSession("./qwen3-asr-onnx/model.onnx")
input_features = np.random.randn(1, 80, 3000).astype(np.float32)
attention_mask = np.ones((1, 3000), dtype=np.int64)
outputs = session.run(None, {
    "input_features": input_features,
    "attention_mask": attention_mask
})
print(f"模型输出形状: {outputs[0].shape}")
```
如果没有报错说明转换成功。

---

## 📦 使用模型
将转换后的**所有文件**复制到 `meeting-transcriber` 程序的 `models` 目录下，结构如下：
```
meeting-transcriber.exe/meeting-transcriber
models/
  ├── model.onnx
  ├── config.json
  ├── vocab.txt
  ├── preprocessor_config.json
  └── 其他配置文件
```

运行程序测试：
```bash
# Windows
meeting-transcriber.exe "会议录音.m4a"

# Linux
./meeting-transcriber "会议录音.m4a"
```

---

## 🐛 常见问题

### ❌ 转换时提示 `trust_remote_code` 错误
- 解决方案：升级 transformers 到 4.37 以上版本：`pip install --upgrade transformers`

### ❌ 转换后程序无法加载模型
- 检查是否所有配置文件都复制到了 `models` 目录，不要遗漏任何 `.json` 或 `.txt` 文件
- 确认 ONNX opset 版本 >= 17
- 尝试使用 opset 16 重新转换：`--opset 16`

### ❌ 识别结果全是乱码
- 检查 `vocab.txt` 和 `preprocessor_config.json` 文件是否存在且正确
- 确认模型版本和转换脚本版本匹配

### ❌ 转换速度非常慢
- 这是正常现象，1.7B 模型转换需要消耗大量内存，建议关闭其他程序
- 使用 GPU 版本的 PyTorch 可以大幅加快转换速度

### ❌ 提示缺少 FFmpeg
- Windows：下载 FFmpeg 并将 bin 目录添加到 PATH，或者将 dll 文件复制到程序目录
- Linux：`sudo apt install ffmpeg`
- macOS：`brew install ffmpeg`

---

## 📌 版本说明
- 本脚本测试通过的模型版本：Qwen/Qwen3-ASR-1.7B
- 支持的 ONNX Runtime 版本：1.15.x ~ 1.17.x
- 本程序不支持 PyTorch 格式和 safetensors 格式的模型，必须转换为 ONNX 格式才能使用
