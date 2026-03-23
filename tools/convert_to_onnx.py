#!/usr/bin/env python3
"""
Qwen3-ASR 模型 ONNX 格式转换脚本
适用于 Qwen/Qwen3-ASR-1.7B 等 Qwen 系列 ASR 模型
"""

import torch
import os
import shutil
import argparse
from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor

def parse_args():
    parser = argparse.ArgumentParser(description="Convert Qwen3-ASR model to ONNX format")
    parser.add_argument("--model-path", type=str, required=True, 
                      help="Path to local Qwen3-ASR model directory")
    parser.add_argument("--output-dir", type=str, default="./qwen3-asr-onnx",
                      help="Output directory for ONNX model (default: ./qwen3-asr-onnx)")
    parser.add_argument("--opset", type=int, default=17,
                      help="ONNX opset version (default: 17)")
    parser.add_argument("--fp16", action="store_true",
                      help="Export as FP16 precision (requires GPU)")
    return parser.parse_args()

def main():
    args = parse_args()
    MODEL_PATH = args.model_path
    OUTPUT_DIR = args.output_dir
    OPSET_VERSION = args.opset
    USE_FP16 = args.fp16

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"🚀 Starting ONNX conversion for Qwen3-ASR model")
    print(f"📥 Input model path: {MODEL_PATH}")
    print(f"📤 Output directory: {OUTPUT_DIR}")
    print(f"⚙️  ONNX opset version: {OPSET_VERSION}")
    print(f"⚡ FP16 precision: {'Enabled' if USE_FP16 else 'Disabled (FP32)'}")

    # ======================
    # Step 1: Load model and processor
    # ======================
    print("\n📦 Loading model and processor...")
    processor = AutoProcessor.from_pretrained(MODEL_PATH, trust_remote_code=True)
    model = AutoModelForSpeechSeq2Seq.from_pretrained(
        MODEL_PATH, 
        torch_dtype=torch.float16 if USE_FP16 else torch.float32,
        trust_remote_code=True,
        low_cpu_mem_usage=True
    )
    model.eval()
    
    if USE_FP16 and torch.cuda.is_available():
        model = model.cuda()
        print("✅ Using GPU for conversion")
    else:
        print("✅ Using CPU for conversion")

    # ======================
    # Step 2: Prepare dummy input
    # ======================
    print("\n🔧 Preparing test input...")
    # Input shape: (batch_size, feature_dim, sequence_length)
    # Qwen3-ASR uses 80 mel features, default sequence length 3000 (15 seconds)
    dummy_input = {
        "input_features": torch.randn(1, 80, 3000, dtype=torch.float16 if USE_FP16 else torch.float32),
        "attention_mask": torch.ones(1, 3000, dtype=torch.long)
    }
    
    if USE_FP16 and torch.cuda.is_available():
        dummy_input["input_features"] = dummy_input["input_features"].cuda()
        dummy_input["attention_mask"] = dummy_input["attention_mask"].cuda()

    # ======================
    # Step 3: Export ONNX model
    # ======================
    print("\n⚙️  Exporting ONNX model... This may take 5-10 minutes...")
    torch.onnx.export(
        model,
        (dummy_input["input_features"], dummy_input["attention_mask"]),
        f"{OUTPUT_DIR}/model.onnx",
        export_params=True,
        opset_version=OPSET_VERSION,
        do_constant_folding=True,
        input_names=["input_features", "attention_mask"],
        output_names=["logits"],
        dynamic_axes={
            "input_features": {0: "batch_size", 2: "sequence_length"},
            "attention_mask": {0: "batch_size", 1: "sequence_length"},
            "logits": {0: "batch_size", 1: "sequence_length"}
        }
    )
    print("✅ Model exported successfully")

    # ======================
    # Step 4: Copy configuration files
    # ======================
    print("\n📋 Copying configuration files...")
    files_to_copy = [
        "config.json",
        "vocab.json",
        "vocab.txt",
        "preprocessor_config.json",
        "generation_config.json",
        "tokenizer.json",
        "tokenizer_config.json"
    ]

    copied_files = []
    for file in files_to_copy:
        src = os.path.join(MODEL_PATH, file)
        dst = os.path.join(OUTPUT_DIR, file)
        if os.path.exists(src):
            shutil.copy(src, dst)
            copied_files.append(file)
            print(f"  ✅ Copied: {file}")
        else:
            print(f"  ⚠️  Not found: {file} (skipping, may not be required)")

    # ======================
    # Step 5: Create usage guide
    # ======================
    guide_content = f"""# 模型使用说明

## 转换信息
- 源模型路径: {MODEL_PATH}
- 转换时间: {torch.__version__}
- ONNX opset版本: {OPSET_VERSION}
- 精度: {'FP16' if USE_FP16 else 'FP32'}

## 使用方法
将本目录下所有文件复制到 meeting-transcriber 的 `models` 目录下，结构如下：
```
meeting-transcriber.exe
models/
  ├── model.onnx
  ├── config.json
  ├── vocab.txt
  ├── preprocessor_config.json
  └── [其他配置文件]
```

运行程序：
```bash
meeting-transcriber.exe "你的音频文件.m4a"
```

或者手动指定模型路径：
```bash
meeting-transcriber.exe "你的音频文件.m4a" --model "/path/to/model.onnx"
```
"""
    with open(f"{OUTPUT_DIR}/README_CONVERSION.txt", "w", encoding="utf-8") as f:
        f.write(guide_content)
    print("\n📝 Generated conversion guide: README_CONVERSION.txt")

    # ======================
    # Summary
    # ======================
    print("\n🎉 Conversion completed!")
    print(f"📦 Output files are in: {OUTPUT_DIR}")
    print(f"\n📋 Next steps:")
    print(f"  1. Copy all files from {OUTPUT_DIR} to the 'models' directory of meeting-transcriber")
    print(f"  2. Ensure FFmpeg is installed (required for audio decoding)")
    print(f"  3. Run meeting-transcriber.exe with your audio file")

if __name__ == "__main__":
    main()
