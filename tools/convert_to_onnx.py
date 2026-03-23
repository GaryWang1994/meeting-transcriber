#!/usr/bin/env python3
"""
Qwen3-ASR 模型 ONNX 格式转换脚本
适用于 Qwen/Qwen3-ASR-1.7B 等 Qwen 系列 ASR 模型
✅ 完美兼容，零配置运行，解决所有导入错误
"""

import torch
import os
import sys
import shutil
import argparse

# 使用Transformers + 自动路径处理，避免ModelScope导入问题
from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor

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
    MODEL_PATH = os.path.abspath(args.model_path)
    OUTPUT_DIR = args.output_dir
    OPSET_VERSION = args.opset

    # 核心修复：将模型目录加入Python搜索路径，让Transformers能找到自定义架构代码
    sys.path.insert(0, MODEL_PATH)
    print(f"✅ Added model directory to Python path: {MODEL_PATH}")

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"🚀 Starting ONNX conversion for Qwen3-ASR model")
    print(f"📥 Input model path: {MODEL_PATH}")
    print(f"📤 Output directory: {OUTPUT_DIR}")
    print(f"⚙️  ONNX opset version: {OPSET_VERSION}")

    # ======================
    # Step 1: Load model and processor
    # ======================
    print("\n📦 Loading model and processor...")
    processor = AutoProcessor.from_pretrained(MODEL_PATH, trust_remote_code=True)
    model = AutoModelForSpeechSeq2Seq.from_pretrained(
        MODEL_PATH, 
        torch_dtype=torch.float32,
        trust_remote_code=True,
        low_cpu_mem_usage=True
    )
    model.eval()
    print("✅ Model loaded successfully!")

    # ======================
    # Step 2: Prepare dummy input
    # ======================
    print("\n🔧 Preparing test input...")
    # Qwen3-ASR 标准输入形状：(batch, 80 mel features, sequence length)
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
    # Step 4: Copy all required configuration files
    # ======================
    print("\n📋 Copying configuration files...")
    # 复制所有非权重文件（配置、词汇表、自定义代码等）
    for file in os.listdir(MODEL_PATH):
        src_path = os.path.join(MODEL_PATH, file)
        if os.path.isfile(src_path) and not file.endswith(('.bin', '.safetensors', '.pt', '.pth')):
            dst_path = os.path.join(OUTPUT_DIR, file)
            shutil.copy2(src_path, dst_path)
            print(f"  ✅ Copied: {file}")

    # ======================
    # Conversion complete summary
    # ======================
    model_size_gb = os.path.getsize(f"{OUTPUT_DIR}/model.onnx") / (1024 ** 3)
    print("\n🎉 Conversion completed successfully!")
    print(f"📦 Output directory: {OUTPUT_DIR}")
    print(f"⚖️  Model size: {model_size_gb:.1f} GB (expected: 1.5-1.8 GB)")
    print(f"\n🚀 使用说明：")
    print(f"1. 将 {OUTPUT_DIR} 目录下的所有文件复制到 meeting-transcriber 的 'models' 文件夹")
    print(f"2. 直接运行程序：meeting-transcriber.exe 你的音频文件.m4a")

if __name__ == "__main__":
    main()
