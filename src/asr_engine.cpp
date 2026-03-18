#include "asr_engine.h"
#include "utils.h"
#include <algorithm>
#include <cstring>

#ifdef HAVE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace meeting_transcriber {

// ============== ASRResult ==============

bool ASRResult::canMergeWith(const ASRResult& other, double maxGap) const {
    double gap = other.startTime - endTime;
    return gap <= maxGap && speakerId == other.speakerId && language == other.language;
}

ASRResult ASRResult::mergeWith(const ASRResult& other) const {
    ASRResult result;
    result.startTime = startTime;
    result.endTime = other.endTime;
    result.text = text + " " + other.text;
    result.confidence = (confidence + other.confidence) / 2.0f;
    result.speakerId = speakerId;
    result.language = language;
    return result;
}

// ============== ONNXASREngine ==============

#ifdef HAVE_ONNXRUNTIME

struct ONNXASREngine::Impl {
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    Ort::AllocatorWithDefaultOptions allocator;
    
    ASRConfig config;
    bool initialized = false;
    std::string lastError;
    
    // 输入输出信息
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    std::vector<int64_t> inputShape;
    std::vector<int64_t> outputShape;
};

#else

struct ONNXASREngine::Impl {
    ASRConfig config;
    bool initialized = false;
    std::string lastError = "ONNX Runtime not compiled in";
};

#endif

ONNXASREngine::ONNXASREngine() : m_impl(std::make_unique<Impl>()) {}
ONNXASREngine::~ONNXASREngine() = default;
ONNXASREngine::ONNXASREngine(ONNXASREngine&&) noexcept = default;
ONNXASREngine& ONNXASREngine::operator=(ONNXASREngine&&) noexcept = default;

bool ONNXASREngine::initialize(const ASRConfig& config) {
#ifdef HAVE_ONNXRUNTIME
    m_impl->config = config;
    m_impl->initialized = false;
    
    try {
        // 创建ONNX Runtime环境
        OrtLoggingLevel loggingLevel = config.useGPU ? ORT_LOGGING_LEVEL_WARNING : ORT_LOGGING_LEVEL_ERROR;
        m_impl->env = std::make_unique<Ort::Env>(loggingLevel, "MeetingTranscriber");
        
        // 配置会话选项
        m_impl->sessionOptions = std::make_unique<Ort::SessionOptions>();
        
        // 设置线程数
        m_impl->sessionOptions->SetIntraOpNumThreads(config.intraOpNumThreads);
        m_impl->sessionOptions->SetInterOpNumThreads(config.interOpNumThreads);
        
        // 设置图优化
        m_impl->sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // 配置GPU（如果需要）
        if (config.useGPU) {
            OrtCUDAProviderOptions cudaOptions;
            cudaOptions.device_id = config.gpuDeviceId;
            m_impl->sessionOptions->AppendExecutionProvider_CUDA(cudaOptions);
            Logger::info("Using GPU for inference");
        }
        
        // 加载模型
        if (config.modelPath.empty() || !FileUtils::fileExists(config.modelPath)) {
            m_impl->lastError = "Model file not found: " + config.modelPath;
            Logger::error(m_impl->lastError);
            return false;
        }
        
        Logger::info("Loading ONNX model: " + config.modelPath);
        m_impl->session = std::make_unique<Ort::Session>(
            *m_impl->env, config.modelPath.c_str(), *m_impl->sessionOptions);
        
        // 获取输入输出信息
        Ort::AllocatorWithDefaultOptions allocator;
        
        // 输入
        size_t numInputs = m_impl->session->GetInputCount();
        for (size_t i = 0; i < numInputs; i++) {
            auto name = m_impl->session->GetInputNameAllocated(i, allocator);
            m_impl->inputNames.push_back(name.get());
            
            auto typeInfo = m_impl->session->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            auto shape = tensorInfo.GetShape();
            m_impl->inputShape = std::vector<int64_t>(shape.begin(), shape.end());
        }
        
        // 输出
        size_t numOutputs = m_impl->session->GetOutputCount();
        for (size_t i = 0; i < numOutputs; i++) {
            auto name = m_impl->session->GetOutputNameAllocated(i, allocator);
            m_impl->outputNames.push_back(name.get());
            
            auto typeInfo = m_impl->session->GetOutputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            auto shape = tensorInfo.GetShape();
            m_impl->outputShape = std::vector<int64_t>(shape.begin(), shape.end());
        }
        
        m_impl->initialized = true;
        Logger::info("ONNX model loaded successfully");
        Logger::info("  Inputs: " + std::to_string(numInputs));
        Logger::info("  Outputs: " + std::to_string(numOutputs));
        
        return true;
        
    } catch (const Ort::Exception& e) {
        m_impl->lastError = "ONNX Runtime error: " + std::string(e.what());
        Logger::error(m_impl->lastError);
        return false;
    } catch (const std::exception& e) {
        m_impl->lastError = "Error: " + std::string(e.what());
        Logger::error(m_impl->lastError);
        return false;
    }
#else
    Logger::error("ONNX Runtime support not compiled in");
    return false;
#endif
}

bool ONNXASREngine::isInitialized() const {
    return m_impl->initialized;
}

std::vector<ASRResult> ONNXASREngine::transcribe(const AudioBuffer& audio, int sampleRate) {
#ifdef HAVE_ONNXRUNTIME
    if (!m_impl->initialized) {
        Logger::error("ASR engine not initialized");
        return {};
    }
    
    try {
        // 预处理音频
        auto features = preprocessAudio(audio, sampleRate);
        
        // 准备输入tensor
        std::vector<int64_t> inputShape = {1, static_cast<int64_t>(features.size())};
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            m_impl->allocator, features.data(), features.size(), inputShape.data(), inputShape.size());
        
        // 运行推理
        auto outputTensors = m_impl->session->Run(
            Ort::RunOptions{nullptr},
            m_impl->inputNames.data(), &inputTensor, 1,
            m_impl->outputNames.data(), m_impl->outputNames.size());
        
        // 解码输出
        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        
        return decodeOutputs(outputTensors[0], 0.0, audio.size() / static_cast<double>(sampleRate));
        
    } catch (const std::exception& e) {
        Logger::error("Transcription error: " + std::string(e.what()));
        return {};
    }
#else
    return {};
#endif
}

std::vector<std::vector<ASRResult>> ONNXASREngine::transcribeBatch(
    const std::vector<AudioBuffer>& audioBuffers, int sampleRate) {
    std::vector<std::vector<ASRResult>> results;
    results.reserve(audioBuffers.size());
    
    for (const auto& audio : audioBuffers) {
        results.push_back(transcribe(audio, sampleRate));
    }
    
    return results;
}

LanguageDetection ONNXASREngine::detectLanguage(const AudioBuffer& audio, int sampleRate) {
    LanguageDetection detection;
    detection.language = m_impl->config.language;
    detection.confidence = 1.0f;
    
    // 如果模型支持语言检测，这里实现检测逻辑
    // 否则返回配置的语言
    
    return detection;
}

ONNXASREngine::ModelInfo ONNXASREngine::getModelInfo() const {
    ModelInfo info;
    info.name = "Qwen3-ASR-1.7B";
    info.version = "1.0";
    info.sampleRate = 16000;
    info.nMels = 80;
    info.nFFT = 400;
    info.hopLength = 160;
    info.supportedLanguages = {"zh", "en", "auto"};
    return info;
}

std::string ONNXASREngine::getLastError() const {
    return m_impl->lastError;
}

void ONNXASREngine::shutdown() {
#ifdef HAVE_ONNXRUNTIME
    m_impl->session.reset();
    m_impl->sessionOptions.reset();
    m_impl->env.reset();
    m_impl->initialized = false;
#endif
}

std::vector<float> ONNXASREngine::preprocessAudio(const AudioBuffer& audio, int sampleRate) {
    // 简单的预处理：归一化并转换为模型输入格式
    // 实际实现中应该进行特征提取（如Mel频谱图）
    
    std::vector<float> features;
    features.reserve(audio.size());
    
    // 复制并归一化
    float peak = audio.calculatePeak();
    if (peak > 0.0f) {
        for (size_t i = 0; i < audio.size(); i++) {
            features.push_back(audio.data()[i] / peak);
        }
    } else {
        for (size_t i = 0; i < audio.size(); i++) {
            features.push_back(audio.data()[i]);
        }
    }
    
    return features;
}

std::vector<std::vector<float>> ONNXASREngine::computeMelSpectrogram(
    const std::vector<float>& samples, int sampleRate) {
    // 简化实现：实际应该使用FFT和Mel滤波器组
    // 这里返回空数据，实际实现需要补充
    return {};
}

std::vector<ASRResult> ONNXASREngine::decodeOutputs(
    const Ort::Value& output, double startTime, double endTime) {
    std::vector<ASRResult> results;
    
    // 简化实现：实际应该解析模型的token输出
    // 这里返回空数据，实际实现需要根据模型输出格式补充
    
    ASRResult result;
    result.startTime = startTime;
    result.endTime = endTime;
    result.text = "ASR not fully implemented - need to complete decodeOutputs";
    result.confidence = 0.0f;
    result.speakerId = -1;
    result.language = m_impl->config.language;
    
    results.push_back(result);
    
    return results;
}

std::string ONNXASREngine::postProcessText(const std::string& text) {
    // 简单的后处理：去除多余空格，规范化标点
    std::string result = text;
    
    // 去除首尾空格
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    result = result.substr(start, end - start + 1);
    
    // 替换多个空格为单个空格
    std::string cleaned;
    bool lastWasSpace = false;
    for (char c : result) {
        if (std::isspace(c)) {
            if (!lastWasSpace) {
                cleaned += ' ';
                lastWasSpace = true;
            }
        } else {
            cleaned += c;
            lastWasSpace = false;
        }
    }
    
    return cleaned;
}

// ============== ASREngineFactory ==============

std::unique_ptr<ONNXASREngine> ASREngineFactory::createEngine(EngineType type) {
    switch (type) {
        case EngineType::ONNX_RUNTIME:
            return std::make_unique<ONNXASREngine>();
        case EngineType::PYTHON_BRIDGE:
            // TODO: 实现Python桥接
            return std::make_unique<ONNXASREngine>();
        default:
            return std::make_unique<ONNXASREngine>();
    }
}

bool ASREngineFactory::isEngineAvailable(EngineType type) {
    switch (type) {
        case EngineType::ONNX_RUNTIME:
#ifdef HAVE_ONNXRUNTIME
            return true;
#else
            return false;
#endif
        case EngineType::PYTHON_BRIDGE:
            // 检查Python是否可用
            return true; // 简化处理
        default:
            return false;
    }
}

ASREngineFactory::EngineType ASREngineFactory::getRecommendedEngine() {
    if (isEngineAvailable(EngineType::ONNX_RUNTIME)) {
        return EngineType::ONNX_RUNTIME;
    }
    return EngineType::PYTHON_BRIDGE;
}

// ============== PythonASRBridge ==============

struct PythonASRBridge::Impl {
    std::string pythonPath;
    std::string modelPath;
    bool initialized = false;
};

PythonASRBridge::PythonASRBridge() : m_impl(std::make_unique<Impl>()) {}
PythonASRBridge::~PythonASRBridge() = default;

bool PythonASRBridge::initialize(const std::string& modelPath, const std::string& pythonPath) {
    m_impl->modelPath = modelPath;
    m_impl->pythonPath = pythonPath.empty() ? "python" : pythonPath;
    
    // TODO: 实现Python进程初始化和通信
    
    m_impl->initialized = true;
    return true;
}

bool PythonASRBridge::isInitialized() const {
    return m_impl->initialized;
}

std::vector<ASRResult> PythonASRBridge::transcribe(const AudioBuffer& audio, int sampleRate) {
    // TODO: 实现通过Python进程调用转录
    return {};
}

void PythonASRBridge::shutdown() {
    m_impl->initialized = false;
}

// ============== ASRProfiler ==============

void ASRProfiler::startTiming() {
    m_start = std::chrono::high_resolution_clock::now();
    m_timing = {};
    m_running = true;
}

void ASRProfiler::recordFeatureExtraction(double ms) {
    m_timing.featureExtraction += ms;
}

void ASRProfiler::recordInference(double ms) {
    m_timing.modelInference += ms;
}

void ASRProfiler::recordDecoding(double ms) {
    m_timing.decoding += ms;
}

void ASRProfiler::recordPostProcessing(double ms) {
    m_timing.postProcessing += ms;
}

ASRProfiler::Timing ASRProfiler::finishTiming() {
    if (m_running) {
        auto end = std::chrono::high_resolution_clock::now();
        m_timing.total = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start).count();
        m_running = false;
    }
    return m_timing;
}

void ASRProfiler::reset() {
    m_timing = {};
    m_running = false;
}

} // namespace meeting_transcriber