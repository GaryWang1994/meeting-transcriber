#pragma once

#include "utils.h"
#include "audio_processor.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

// ONNX Runtime forward declarations
namespace Ort {
class Env;
class Session;
class SessionOptions;
class Value;
struct AllocatedStringPtr;
}

namespace meeting_transcriber {

// ASR引擎配置
struct ASRConfig {
    std::string modelPath;              // ONNX模型路径
    std::string tokenizerPath;          // Tokenizer路径（可选）
    std::string language;               // 语言代码 (zh, en, auto)
    bool useGPU;                        // 是否使用GPU
    int gpuDeviceId;                    // GPU设备ID
    int intraOpNumThreads;              // 内部操作线程数
    int interOpNumThreads;              // 跨操作线程数
    int batchSize;                      // 批处理大小
    
    ASRConfig() : language("auto"), useGPU(false), gpuDeviceId(0),
                  intraOpNumThreads(4), interOpNumThreads(4), batchSize(1) {}
};

// ASR结果
struct ASRResult {
    std::string text;
    double startTime;
    double endTime;
    float confidence;
    std::vector<float> tokenLogProbs;
    std::string language;
    int speakerId;

    bool canMergeWith(const ASRResult& other, double maxGap = 1.0) const;
    ASRResult mergeWith(const ASRResult& other) const;
};

// 语言检测结果
struct LanguageDetection {
    std::string language;       // 语言代码
    float confidence;           // 置信度
};

// ONNX Runtime ASR引擎
class ONNXASREngine {
public:
    ONNXASREngine();
    ~ONNXASREngine();
    
    // 禁止拷贝
    ONNXASREngine(const ONNXASREngine&) = delete;
    ONNXASREngine& operator=(const ONNXASREngine&) = delete;
    
    // 允许移动
    ONNXASREngine(ONNXASREngine&&) noexcept;
    ONNXASREngine& operator=(ONNXASREngine&&) noexcept;
    
    // 初始化引擎
    bool initialize(const ASRConfig& config);
    bool isInitialized() const;
    
    // 转录音频
    std::vector<ASRResult> transcribe(const AudioBuffer& audio, int sampleRate);
    
    // 批量转录（更高效率）
    std::vector<std::vector<ASRResult>> transcribeBatch(
        const std::vector<AudioBuffer>& audioBuffers, int sampleRate);
    
    // 语言检测
    LanguageDetection detectLanguage(const AudioBuffer& audio, int sampleRate);
    
    // 获取模型信息
    struct ModelInfo {
        std::string name;
        std::string version;
        int sampleRate;
        int nMels;              // Mel滤波器组数
        int nFFT;               // FFT点数
        int hopLength;          // 帧移
        std::vector<std::string> supportedLanguages;
    };
    ModelInfo getModelInfo() const;
    
    // 获取最后错误信息
    std::string getLastError() const;
    
    // 释放资源
    void shutdown();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // 预处理音频（特征提取）
    std::vector<float> preprocessAudio(const AudioBuffer& audio, int sampleRate);
    
    // 计算Mel频谱图
    std::vector<std::vector<float>> computeMelSpectrogram(
        const std::vector<float>& samples, int sampleRate);
    
    // 解码模型输出
    std::vector<ASRResult> decodeOutputs(
        const Ort::Value& output, double startTime, double endTime);
    
    // 后处理（标点、大小写等）
    std::string postProcessText(const std::string& text);
};

// ASR引擎工厂
class ASREngineFactory {
public:
    // 支持的引擎类型
    enum class EngineType {
        ONNX_RUNTIME,       // ONNX Runtime
        PYTHON_BRIDGE,      // Python bridge (fallback)
    };
    
    // 创建引擎实例
    static std::unique_ptr<ONNXASREngine> createEngine(
        EngineType type = EngineType::ONNX_RUNTIME);
    
    // 检查引擎是否可用
    static bool isEngineAvailable(EngineType type);
    
    // 获取推荐引擎类型
    static EngineType getRecommendedEngine();
};

// Python桥接引擎（当ONNX不可用时使用）
class PythonASRBridge {
public:
    PythonASRBridge();
    ~PythonASRBridge();
    
    bool initialize(const std::string& modelPath, const std::string& pythonPath = "");
    bool isInitialized() const;
    
    std::vector<ASRResult> transcribe(const AudioBuffer& audio, int sampleRate);
    
    void shutdown();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 性能分析器
class ASRProfiler {
public:
    struct Timing {
        double featureExtraction;
        double modelInference;
        double decoding;
        double postProcessing;
        double total;
    };
    
    void startTiming();
    void recordFeatureExtraction(double ms);
    void recordInference(double ms);
    void recordDecoding(double ms);
    void recordPostProcessing(double ms);
    Timing finishTiming();
    
    void reset();
    
private:
    std::chrono::high_resolution_clock::time_point m_start;
    Timing m_timing;
    bool m_running = false;
};

} // namespace meeting_transcriber