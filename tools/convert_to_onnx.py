#!/usr/bin/env python3
"""
Qwen3-ASR 模型 ONNX 格式转换脚本
适用于 Qwen/Qwen3-ASR-1.7B 等 Qwen 系列 ASR 模型

⚠️ 注意：必须使用 ModelScope 的 AutoModel 类，不能用 Transformers 的，否则会识别不到 qwen3_asr 架构
"""

import torch
import os
import shutil
import argparse
# 必须使用 ModelScope 提供的加载类，支持自定义架构
from modelscope import AutoModelForSpeechSeq2Seq, AutoProcessor

def parse_args():
    parser = argparse.ArgumentParser(description="Convert Qwen3-ASR model to ONNX format")
    parser.add_argument("--model-path", type=str, required=True, 
                      help="Path to local Qwen3-ASR model directory (downloaded from ModelScope)")
    parser.add_argument("--output-dir", type=str, default="./qwen3-asr-onnx",
                      help="Output directory for ONNX model (default: ./qwen3-asr-onnx)")
    parser.add_argument("--opset", type=int, default=17,
                      help="ONNX opset version (default: 17)")
    return parser.parse_args()

def main():
    args = parse_args()
    MODEL_PATH = args.model_path
    OUTPUT_DIR = args.output_dir
    OPSET_VERSION = args.opset

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"🚀 Starting ONNX conversion for Qwen3-ASR model")
    print(f"📥 Input model path: {MODEL_PATH}")
    print(f"📤 Output directory: {OUTPUT_DIR}")
    print(f"⚙️  ONNX opset version: {OPSET_VERSION}")

    # ======================
    # Step 1: Load model and processor
    # ======================
    print("\n📦 Loading model and processor from ModelScope...")
    processor = AutoProcessor.from_pretrained(MODEL_PATH, trust_remote_code=True)
    model = AutoModelForSpeechSeq2Seq.from_pretrained(
        MODEL_PATH, 
        torch_dtype=torch.float32,
        trust_remote_code=True,
        low_cpu_mem_usage=True
    )
    model.eval()
    print("✅ Model loaded successfully (ModelScope format)")

    # ======================
    # Step 2: Prepare dummy input
    # ======================
    print("\n🔧 Preparing test input...")
    # Qwen3-ASR input shape: (batch_size, 80 mel features, sequence_length)
    # 3000 frames = ~15 seconds of audio
    dummy_input = {
        "input_features": torch.randn(1, 80, 3000, dtype=torch.float32),
        "attention_mask": torch.ones(1, 3000, dtype=torch.long)
    }

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
    # Step 4: Copy configuration files (including custom model code)
    # ======================
    print("\n📋 Copying configuration and custom code files...")
    files_to_copy = [
        "config.json",
        "vocab.txt",
        "preprocessor_config.json",
        "generation_config.json",
        "modeling_qwen2_audio.py",
        "configuration_qwen2_audio.py",
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
    # Step 5: Create conversion info
    # ======================
    info_content = f"""# 模型转换信息
- 源模型: Qwen3-ASR-1.7B
- 转换工具版本: v1.1 (ModelScope版)
- ONNX opset版本: {OPSET_VERSION}
- 精度: FP32
- 转换时间: {torch.__version__}

## 使用方法
将本目录下所有文件复制到 meeting-transcriber 的 `models` 目录即可使用。
无需额外安装FFmpeg，Windows版本已内置所有依赖。
"""
    with open(f"{OUTPUT_DIR}/CONVERSION_INFO.txt", "w", encoding="utf-8") as f:
        f.write(info_content)

    # ======================
    # Summary
    # ======================
    print("\n🎉 Conversion completed successfully!")
    print(f"📦 Output files are in: {OUTPUT_DIR}")
    print(f"\n✅ 验证方法:")
    print(f"  请确认 {OUTPUT_DIR} 目录下包含:")
    print(f"   - model.onnx (约1.7GB左右)")
    print(f"   - config.json, vocab.txt, preprocessor_config.json")
    print(f"\n🚀 使用方法:")
    print(f"  1. 将所有文件复制到 meeting-transcriber 的 'models' 目录")
    print(f"  2. 直接运行: meeting-transcriber.exe 你的音频文件.m4a")

if __name__ == "__main__":
    main()
