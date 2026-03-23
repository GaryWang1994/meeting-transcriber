#!/usr/bin/env python3
"""
Qwen3-ASR 模型 ONNX 转换脚本 - 官方FunASR版本
✅ 无需自定义文件，完美支持Qwen3-ASR-1.7B
"""

import torch
import os
import shutil
import argparse
from funasr import AutoModel

def parse_args():
    parser = argparse.ArgumentParser(description="Qwen3-ASR to ONNX converter (FunASR official version)")
    parser.add_argument("--model-name", type=str, default="Qwen/Qwen3-ASR-1.7B",
                      help="Model name on ModelScope (default: Qwen/Qwen3-ASR-1.7B)")
    parser.add_argument("--output-dir", type=str, default="./qwen3-asr-onnx",
                      help="Output directory for ONNX model")
    parser.add_argument("--opset", type=int, default=17,
                      help="ONNX opset version (default: 17)")
    return parser.parse_args()

def main():
    args = parse_args()
    MODEL_NAME = args.model_name
    OUTPUT_DIR = args.output_dir
    OPSET_VERSION = args.opset

    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"🚀 Qwen3-ASR ONNX 转换开始（FunASR官方版本）")
    print(f"📥 模型名称: {MODEL_NAME}")
    print(f"📤 输出目录: {OUTPUT_DIR}")

    # ======================
    # Step 1: 用FunASR加载模型（官方支持，无架构问题）
    # ======================
    print("\n📦 正在加载模型（自动从ModelScope下载，无需手动下载）...")
    model = AutoModel(
        model=MODEL_NAME,
        trust_remote_code=True,
        device="cpu"
    )
    print("✅ 模型加载成功！")

    # ======================
    # Step 2: 准备输入
    # ======================
    print("\n🔧 准备测试输入...")
    # Qwen3-ASR输入形状: (batch, 80 mel, 3000 frames)
    dummy_input = {
        "input_features": torch.randn(1, 80, 3000, dtype=torch.float32),
        "attention_mask": torch.ones(1, 3000, dtype=torch.long)
    }

    # ======================
    # Step 3: 导出ONNX
    # ======================
    print("\n⚙️  正在导出ONNX模型（需要5-10分钟）...")
    torch.onnx.export(
        model.model,
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
    print("✅ ONNX模型导出成功！")

    # ======================
    # Step 4: 导出配置和词汇表
    # ======================
    print("\n📋 正在导出配置文件...")
    # 保存配置
    if hasattr(model, 'config'):
        model.config.save_pretrained(OUTPUT_DIR)
    # 保存处理器
    if hasattr(model, 'processor'):
        model.processor.save_pretrained(OUTPUT_DIR)
    
    # 自动复制所有需要的文件
    for file in os.listdir(model.model_dir):
        if file.endswith(('.json', '.txt', '.py')) and not file.startswith('pytorch_model'):
            src = os.path.join(model.model_dir, file)
            dst = os.path.join(OUTPUT_DIR, file)
            if os.path.isfile(src):
                shutil.copy(src, dst)
                print(f"  ✅ 复制: {file}")

    # ======================
    # 完成
    # ======================
    model_size = os.path.getsize(f"{OUTPUT_DIR}/model.onnx") / (1024**3)
    print(f"\n🎉 转换完成！")
    print(f"📦 模型大小: {model_size:.1f} GB")
    print(f"\n🚀 使用方法：")
    print(f"1. 将 {OUTPUT_DIR} 目录下的所有文件复制到 meeting-transcriber 的 'models' 目录")
    print(f"2. 直接运行程序：meeting-transcriber.exe 你的音频文件.m4a")

if __name__ == "__main__":
    main()
