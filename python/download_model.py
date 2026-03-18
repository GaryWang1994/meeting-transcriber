#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Model Downloader for Qwen3-ASR-1.7B

Downloads the Qwen3-ASR-1.7B model from ModelScope or HuggingFace.
"""

import os
import sys
import argparse
from pathlib import Path

def download_from_modelscope(model_id: str, output_dir: str) -> bool:
    """Download model from ModelScope"""
    try:
        from modelscope import snapshot_download
        
        print(f"Downloading {model_id} from ModelScope...")
        model_dir = snapshot_download(model_id, cache_dir=output_dir)
        print(f"Model downloaded to: {model_dir}")
        return True
        
    except ImportError:
        print("Error: modelscope not installed. Install with: pip install modelscope")
        return False
    except Exception as e:
        print(f"Error downloading from ModelScope: {e}")
        return False

def download_from_huggingface(model_id: str, output_dir: str) -> bool:
    """Download model from HuggingFace"""
    try:
        from huggingface_hub import snapshot_download
        
        print(f"Downloading {model_id} from HuggingFace...")
        model_dir = snapshot_download(model_id, cache_dir=output_dir)
        print(f"Model downloaded to: {model_dir}")
        return True
        
    except ImportError:
        print("Error: huggingface_hub not installed. Install with: pip install huggingface_hub")
        return False
    except Exception as e:
        print(f"Error downloading from HuggingFace: {e}")
        return False

def convert_to_onnx(model_path: str, output_path: str) -> bool:
    """Convert PyTorch model to ONNX format"""
    try:
        import torch
        from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor
        
        print(f"Loading model from {model_path}...")
        model = AutoModelForSpeechSeq2Seq.from_pretrained(model_path)
        processor = AutoProcessor.from_pretrained(model_path)
        
        model.eval()
        
        # Create dummy input
        dummy_input = torch.randn(1, 80, 3000)  # (batch, mel_bins, time)
        
        print(f"Exporting to ONNX: {output_path}...")
        torch.onnx.export(
            model,
            dummy_input,
            output_path,
            input_names=["input_features"],
            output_names=["logits"],
            dynamic_axes={
                "input_features": {0: "batch_size", 2: "sequence_length"},
                "logits": {0: "batch_size", 1: "sequence_length"}
            },
            opset_version=14
        )
        
        print("ONNX conversion complete!")
        return True
        
    except ImportError as e:
        print(f"Error: Required package not installed: {e}")
        return False
    except Exception as e:
        print(f"Error converting to ONNX: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Download Qwen3-ASR-1.7B model')
    parser.add_argument('--model', '-m', default='Qwen/Qwen3-ASR-1.7B',
                       help='Model ID (default: Qwen/Qwen3-ASR-1.7B)')
    parser.add_argument('--output', '-o', default='../models',
                       help='Output directory (default: ../models)')
    parser.add_argument('--source', '-s', choices=['modelscope', 'huggingface', 'auto'],
                       default='auto', help='Download source')
    parser.add_argument('--convert-onnx', '-c', action='store_true',
                       help='Convert to ONNX format after download')
    parser.add_argument('--onnx-output', default=None,
                       help='ONNX output path (default: auto)')
    
    args = parser.parse_args()
    
    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Determine download source
    source = args.source
    if source == 'auto':
        # Try ModelScope first (for Chinese users), then HuggingFace
        source = 'modelscope'
    
    # Download model
    success = False
    model_path = None
    
    if source == 'modelscope':
        success = download_from_modelscope(args.model, str(output_dir))
        if success:
            # Find the downloaded model directory
            model_dirs = list(output_dir.glob('*/'))
            if model_dirs:
                model_path = str(model_dirs[0])
    
    if not success and source == 'huggingface':
        success = download_from_huggingface(args.model, str(output_dir))
        if success:
            model_dirs = list(output_dir.glob('*/'))
            if model_dirs:
                model_path = str(model_dirs[0])
    
    if not success:
        print("Failed to download model from any source")
        return 1
    
    print(f"\nModel downloaded successfully to: {model_path}")
    
    # Convert to ONNX if requested
    if args.convert_onnx:
        if model_path is None:
            print("Error: Cannot convert to ONNX - model path not found")
            return 1
        
        onnx_output = args.onnx_output
        if onnx_output is None:
            onnx_output = str(output_dir / "qwen3-asr-1.7b.onnx")
        
        if not convert_to_onnx(model_path, onnx_output):
            print("Warning: ONNX conversion failed, but model download was successful")
            return 0  # Still return success since download worked
    
    return 0

if __name__ == '__main__':
    sys.exit(main())