#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qwen3-ASR-1.7B Python Bridge for C++ Application

This script provides a Python-based ASR interface that can be called from
the C++ application when ONNX Runtime is not available.
"""

import sys
import os
import json
import argparse
import numpy as np
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple
import tempfile
import wave

# 尝试导入transformers和torch
try:
    import torch
    import torch.nn as nn
    from transformers import AutoModelForSpeechSeq2Seq, AutoProcessor, pipeline
    TRANSFORMERS_AVAILABLE = True
except ImportError:
    TRANSFORMERS_AVAILABLE = False
    print("Warning: transformers or torch not available. ASR functionality will be limited.")

# 尝试导入模型scope
try:
    from modelscope import snapshot_download, AutoModel, AutoTokenizer
    MODELSCOPE_AVAILABLE = True
except ImportError:
    MODELSCOPE_AVAILABLE = False


class Qwen3ASRBridge:
    """Qwen3-ASR-1.7B model bridge"""
    
    def __init__(self, model_path: Optional[str] = None, device: str = "auto"):
        self.model = None
        self.processor = None
        self.model_path = model_path
        self.device = self._get_device(device)
        self.torch_dtype = torch.float16 if self.device != "cpu" else torch.float32
        
    def _get_device(self, device: str) -> str:
        """Determine the best device to use"""
        if device != "auto":
            return device
            
        if torch.cuda.is_available():
            return "cuda:0"
        elif hasattr(torch.backends, 'mps') and torch.backends.mps.is_available():
            return "mps"
        else:
            return "cpu"
    
    def initialize(self) -> bool:
        """Initialize the model"""
        if not TRANSFORMERS_AVAILABLE:
            print("Error: transformers library is required")
            return False
            
        try:
            print(f"Loading Qwen3-ASR-1.7B model on {self.device}...")
            
            # 如果未指定模型路径，尝试从modelscope下载
            if self.model_path is None or not os.path.exists(self.model_path):
                if MODELSCOPE_AVAILABLE:
                    print("Downloading model from ModelScope...")
                    self.model_path = snapshot_download("Qwen/Qwen3-ASR-1.7B")
                else:
                    # 使用HuggingFace
                    self.model_path = "Qwen/Qwen3-ASR-1.7B"
            
            # 加载模型和处理器
            self.processor = AutoProcessor.from_pretrained(self.model_path)
            
            self.model = AutoModelForSpeechSeq2Seq.from_pretrained(
                self.model_path,
                torch_dtype=self.torch_dtype,
                device_map=self.device,
            )
            
            self.model.eval()
            
            print("Model loaded successfully!")
            return True
            
        except Exception as e:
            print(f"Error loading model: {e}")
            return False
    
    def transcribe_file(self, audio_path: str, language: str = "auto") -> List[Dict]:
        """Transcribe an audio file"""
        if self.model is None:
            print("Model not initialized")
            return []
            
        try:
            # 读取音频文件
            import librosa
            
            print(f"Loading audio: {audio_path}")
            audio, sample_rate = librosa.load(audio_path, sr=16000, mono=True)
            
            return self.transcribe_audio(audio, sample_rate, language)
            
        except Exception as e:
            print(f"Error transcribing file: {e}")
            return []
    
    def transcribe_audio(self, audio: np.ndarray, sample_rate: int, 
                        language: str = "auto") -> List[Dict]:
        """Transcribe audio data"""
        if self.model is None:
            print("Model not initialized")
            return []
            
        try:
            # 处理语言参数
            forced_language = None if language == "auto" else language
            
            # 处理音频（分块处理长音频）
            chunk_length = 30  # 30秒一块
            stride_length = 5   # 5秒重叠
            
            results = []
            
            # 将音频分割成块
            total_samples = len(audio)
            chunk_samples = chunk_length * sample_rate
            stride_samples = stride_length * sample_rate
            
            num_chunks = max(1, (total_samples - chunk_samples) // (chunk_samples - stride_samples) + 1)
            
            print(f"Processing audio in {num_chunks} chunks...")
            
            for i in range(num_chunks):
                start_sample = i * (chunk_samples - stride_samples)
                end_sample = min(start_sample + chunk_samples, total_samples)
                
                chunk = audio[start_sample:end_sample]
                
                # 准备输入
                inputs = self.processor(
                    chunk,
                    sampling_rate=sample_rate,
                    return_tensors="pt"
                )
                
                # 将输入移到正确的设备
                input_features = inputs.input_features.to(self.device).to(self.torch_dtype)
                
                # 生成
                with torch.no_grad():
                    generate_kwargs = {}
                    if forced_language:
                        forced_lang_ids = self.processor.tokenizer.get_lang_id(forced_language)
                        generate_kwargs["forced_decoder_ids"] = [(1, forced_lang_ids)]
                    
                    predicted_ids = self.model.generate(
                        input_features,
                        **generate_kwargs
                    )
                
                # 解码
                transcription = self.processor.batch_decode(predicted_ids, skip_special_tokens=True)
                
                start_time = start_sample / sample_rate
                end_time = end_sample / sample_rate
                
                results.append({
                    "start_time": start_time,
                    "end_time": end_time,
                    "text": transcription[0].strip() if transcription else "",
                    "confidence": 0.95  # Qwen3-ASR不直接提供置信度
                })
                
                print(f"  Chunk {i+1}/{num_chunks} processed")
            
            return results
            
        except Exception as e:
            print(f"Error in transcription: {e}")
            import traceback
            traceback.print_exc()
            return []
    
    def shutdown(self):
        """Clean up resources"""
        if self.model is not None:
            del self.model
            self.model = None
        if self.processor is not None:
            del self.processor
            self.processor = None
        torch.cuda.empty_cache() if torch.cuda.is_available() else None


def main():
    parser = argparse.ArgumentParser(description='Qwen3-ASR Python Bridge')
    parser.add_argument('--mode', choices=['transcribe', 'server'], required=True)
    parser.add_argument('--input', '-i', help='Input audio file')
    parser.add_argument('--output', '-o', help='Output JSON file')
    parser.add_argument('--model', '-m', help='Model path or name')
    parser.add_argument('--language', '-l', default='auto', help='Language code')
    parser.add_argument('--device', '-d', default='auto', help='Device (cpu/cuda/auto)')
    parser.add_argument('--port', '-p', type=int, default=8080, help='Server port')
    
    args = parser.parse_args()
    
    if args.mode == 'transcribe':
        if not args.input:
            print("Error: --input is required for transcribe mode")
            return 1
        
        # 初始化ASR
        asr = Qwen3ASRBridge(model_path=args.model, device=args.device)
        if not asr.initialize():
            print("Failed to initialize ASR")
            return 1
        
        try:
            # 执行转录
            results = asr.transcribe_file(args.input, language=args.language)
            
            # 准备输出
            output_data = {
                "success": True,
                "input_file": args.input,
                "language": args.language,
                "segments": results
            }
            
            # 输出结果
            output_json = json.dumps(output_data, ensure_ascii=False, indent=2)
            
            if args.output:
                with open(args.output, 'w', encoding='utf-8') as f:
                    f.write(output_json)
                print(f"Output written to: {args.output}")
            else:
                print(output_json)
                
        finally:
            asr.shutdown()
            
    elif args.mode == 'server':
        print(f"Starting server on port {args.port}")
        # TODO: 实现HTTP服务器模式
        print("Server mode not yet implemented")
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())