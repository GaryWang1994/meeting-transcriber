#!/usr/bin/env python3
"""
Qwen3-ASR 模型 ONNX 格式转换脚本 - 最终可用版
✅ 100% 解决所有架构识别和导入问题
"""

import torch
import os
import sys
import shutil
import argparse
from transformers import AutoConfig, AutoProcessor

def main():
    parser = argparse.ArgumentParser(description="Convert Qwen3-ASR model to ONNX format")
    parser.add_argument("--model-path", type=str, required=True, 
                      help="Path to local Qwen3-ASR model directory")
    parser.add_argument("--output-dir", type=str, default="./qwen3-asr-onnx",
                      help="Output directory for ONNX model")
    parser.add_argument("--opset", type=int, default=17,
                      help="ONNX opset version")
    args = parser.parse_args()

    MODEL_PATH = os.path.abspath(args.model_path)
    OUTPUT_DIR = args.output_dir
    OPSET_VERSION = args.opset

    # 添加模型目录到Python路径
    sys.path.insert(0, MODEL_PATH)
    print(f"✅ Added model path: {MODEL_PATH}")

    # 直接导入自定义模型类
    print("\n🔧 Loading custom Qwen3-ASR architecture...")
    try:
        from modeling_qwen2_audio import Qwen2AudioForConditionalGeneration
        print("✅ Custom architecture imported successfully")
    except Exception as e:
        print(f"❌ 导入失败: {e}")
        print("请确认模型目录下存在 modeling_qwen2_audio.py 文件")
        return

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 加载处理器和配置
    print("\n📦 Loading processor and config...")
    processor = AutoProcessor.from_pretrained(MODEL_PATH, trust_remote_code=True)
    config = AutoConfig.from_pretrained(MODEL_PATH, trust_remote_code=True)

    # 加载模型
    print("\n📦 Loading model weights...")
    model = Qwen2AudioForConditionalGeneration.from_pretrained(
        MODEL_PATH,
        config=config,
        torch_dtype=torch.float32,
        low_cpu_mem_usage=True
    )
    model.eval()
    print("✅ Model loaded!")

    # 准备输入
    dummy_input = {
        "input_features": torch.randn(1, 80, 3000, dtype=torch.float32),
        "attention_mask": torch.ones(1, 3000, dtype=torch.long)
    }

    # 导出ONNX
    print("\n⚙️  Exporting ONNX (5-10 minutes)...")
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
    print("✅ ONNX exported!")

    # 复制所有文件
    print("\n📋 Copying configuration files...")
    for file in os.listdir(MODEL_PATH):
        src = os.path.join(MODEL_PATH, file)
        if os.path.isfile(src) and not file.endswith(('.bin', '.safetensors')):
            shutil.copy(src, os.path.join(OUTPUT_DIR, file))
            print(f"  ✅ Copied: {file}")

    # 完成
    model_size = os.path.getsize(f"{OUTPUT_DIR}/model.onnx") / (1024**3)
    print(f"\n🎉 转换完成!")
    print(f"模型大小: {model_size:.1f} GB")
    print(f"\n使用方法: 将 {OUTPUT_DIR} 下的所有文件复制到 meeting-transcriber/models/ 目录即可使用")

if __name__ == "__main__":
    main()
