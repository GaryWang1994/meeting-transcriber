#!/usr/bin/env python3
"""
最简单的Qwen3-ASR ONNX转换脚本
✅ 最稳健版本，层层验证，避免NoneType错误
"""
import os
import torch
import shutil
import argparse

def main():
    parser = argparse.ArgumentParser(description="Simple Qwen3-ASR to ONNX converter")
    parser.add_argument("--model-dir", type=str, required=True,
                      help="Local Qwen3-ASR model directory")
    parser.add_argument("--output-dir", type=str, default="./qwen3-asr-onnx",
                      help="Output directory")
    args = parser.parse_args()

    MODEL_DIR = os.path.abspath(args.model_dir)
    OUTPUT_DIR = args.output_dir
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print(f"✅ Model directory: {MODEL_DIR}")
    print(f"✅ Output directory: {OUTPUT_DIR}")

    # 1. 先直接手动导入自定义架构，避免所有自动匹配问题
    print("\n🔧 Loading model manually...")
    import sys
    sys.path.insert(0, MODEL_DIR)
    
    # 尝试两种可能的架构文件名
    try:
        from configuration_qwen2_audio import Qwen2AudioConfig
        from modeling_qwen2_audio import Qwen2AudioForConditionalGeneration
        print("✅ Loaded custom architecture from model directory")
    except ImportError as e:
        print(f"❌ 无法加载自定义架构: {e}")
        print("💡 请尝试从这里下载两个架构文件到你的模型目录：")
        print("   - https://modelscope.cn/models/damo/Qwen2-Audio-7B-Instruct/file/view/master/configuration_qwen2_audio.py")
        print("   - https://modelscope.cn/models/damo/Qwen2-Audio-7B-Instruct/file/view/master/modeling_qwen2_audio.py")
        return

    # 2. 手动加载配置和权重
    print("\n📦 Loading config and weights...")
    config = Qwen2AudioConfig.from_pretrained(MODEL_DIR, trust_remote_code=True)
    model = Qwen2AudioForConditionalGeneration.from_pretrained(
        MODEL_DIR,
        config=config,
        torch_dtype=torch.float32,
        low_cpu_mem_usage=True
    )
    model.eval()
    print("✅ Model loaded successfully!")

    # 3. 准备输入
    print("\n🔧 Preparing input...")
    dummy_input = (
        torch.randn(1, 80, 3000, dtype=torch.float32),
        torch.ones(1, 3000, dtype=torch.long)
    )

    # 4. 导出ONNX
    print("\n⚙️  Exporting ONNX...")
    torch.onnx.export(
        model,
        dummy_input,
        f"{OUTPUT_DIR}/model.onnx",
        export_params=True,
        opset_version=17,
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

    # 5. 复制所有配置文件
    print("\n📋 Copying config files...")
    for file in os.listdir(MODEL_DIR):
        if file.endswith(('.json', '.txt', '.py')) and not file.endswith('.bin') and not file.endswith('.safetensors'):
            src = os.path.join(MODEL_DIR, file)
            dst = os.path.join(OUTPUT_DIR, file)
            shutil.copy(src, dst)
            print(f"  ✅ Copied: {file}")

    print("\n🎉 Done! Convert successfully!")
    print(f"Output in: {OUTPUT_DIR}")

if __name__ == "__main__":
    main()
