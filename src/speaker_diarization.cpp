#include "speaker_diarization.h"
#include "utils.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <numeric>

namespace meeting_transcriber {

// ============== SpeakerEmbeddingExtractor ==============

struct SpeakerEmbeddingExtractor::Impl {
    bool initialized = false;
    int embeddingDim = 256;  // 默认嵌入维度
    std::string lastError;
};

SpeakerEmbeddingExtractor::SpeakerEmbeddingExtractor() 
    : m_impl(std::make_unique<Impl>()) {}

SpeakerEmbeddingExtractor::~SpeakerEmbeddingExtractor() = default;

bool SpeakerEmbeddingExtractor::initialize(const std::string& modelPath) {
    // 简化实现：实际应该加载预训练的说话人识别模型
    // 这里使用简单的基于能量的特征作为占位
    
    m_impl->initialized = true;
    Logger::info("Speaker embedding extractor initialized (placeholder implementation)");
    return true;
}

bool SpeakerEmbeddingExtractor::isInitialized() const {
    return m_impl->initialized;
}

std::vector<float> SpeakerEmbeddingExtractor::extract(const AudioBuffer& audio, int sampleRate) {
    if (!m_impl->initialized) {
        return {};
    }
    
    // 简化实现：提取基于MFCC的简化特征
    // 实际应该使用预训练的神经网络提取声纹嵌入
    
    std::vector<float> embedding;
    embedding.reserve(m_impl->embeddingDim);
    
    // 计算基本统计特征作为简化的嵌入
    float mean = 0.0f;
    float variance = 0.0f;
    float rms = audio.calculateRMS();
    float peak = audio.calculatePeak();
    
    const float* data = audio.data();
    size_t size = audio.size();
    
    // 计算均值
    for (size_t i = 0; i < size; ++i) {
        mean += data[i];
    }
    mean /= size;
    
    // 计算方差
    for (size_t i = 0; i < size; ++i) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= size;
    
    // 生成简化的嵌入向量
    std::mt19937 gen(static_cast<unsigned>(size));  // 使用音频大小作为种子
    std::normal_distribution<float> dist(mean, std::sqrt(variance));
    
    for (int i = 0; i < m_impl->embeddingDim; ++i) {
        // 结合统计特征和随机性生成嵌入
        float baseValue = dist(gen);
        float featureValue = baseValue * (1.0f + rms) * (1.0f + peak * 0.5f);
        embedding.push_back(featureValue);
    }
    
    // 归一化嵌入向量
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    if (norm > 0.0f) {
        for (float& val : embedding) {
            val /= norm;
        }
    }
    
    return embedding;
}

std::vector<std::vector<float>> SpeakerEmbeddingExtractor::extractBatch(
    const std::vector<AudioBuffer>& audioBuffers, int sampleRate) {
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(audioBuffers.size());
    
    for (const auto& audio : audioBuffers) {
        embeddings.push_back(extract(audio, sampleRate));
    }
    
    return embeddings;
}

float SpeakerEmbeddingExtractor::computeSimilarity(
    const std::vector<float>& emb1, const std::vector<float>& emb2) {
    if (emb1.size() != emb2.size() || emb1.empty()) {
        return 0.0f;
    }
    
    float dot = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;
    
    for (size_t i = 0; i < emb1.size(); ++i) {
        dot += emb1[i] * emb2[i];
        norm1 += emb1[i] * emb1[i];
        norm2 += emb2[i] * emb2[i];
    }
    
    float denom = std::sqrt(norm1 * norm2);
    if (denom > 0.0f) {
        return dot / denom;
    }
    
    return 0.0f;
}

float SpeakerEmbeddingExtractor::computeDistance(
    const std::vector<float>& emb1, const std::vector<float>& emb2) {
    if (emb1.size() != emb2.size() || emb1.empty()) {
        return std::numeric_limits<float>::max();
    }
    
    float sum = 0.0f;
    for (size_t i = 0; i < emb1.size(); ++i) {
        float diff = emb1[i] - emb2[i];
        sum += diff * diff;
    }
    
    return std::sqrt(sum);
}

int SpeakerEmbeddingExtractor::getEmbeddingDimension() const {
    return m_impl->embeddingDim;
}

void SpeakerEmbeddingExtractor::shutdown() {
    m_impl->initialized = false;
}

// ============== 其他类的简化实现 ==============

// VoiceActivityDetector 简化实现
struct VoiceActivityDetector::Impl {
    Config config;
    std::vector<float> buffer;
    double currentTime = 0.0;
    bool inSpeech = false;
    double speechStartTime = 0.0;
    float currentEnergy = 0.0f;
};

VoiceActivityDetector::VoiceActivityDetector(const Config& config) 
    : m_impl(std::make_unique<Impl>()) {
    m_impl->config = config;
}

VoiceActivityDetector::~VoiceActivityDetector() = default;

std::vector<VoiceActivityDetector::SpeechSegment> VoiceActivityDetector::detect(
    const AudioBuffer& audio, int sampleRate) {
    std::vector<SpeechSegment> segments;
    
    resetStream();
    
    // 分块处理
    size_t chunkSize = sampleRate / 10;  // 100ms chunks
    size_t numChunks = (audio.size() + chunkSize - 1) / chunkSize;
    
    for (size_t i = 0; i < numChunks; ++i) {
        size_t start = i * chunkSize;
        size_t end = std::min(start + chunkSize, audio.size());
        
        AudioBuffer chunk;
        chunk.append(audio.data() + start, end - start);
        
        std::vector<SpeechSegment> completed;
        processChunk(chunk, sampleRate, completed);
        
        segments.insert(segments.end(), completed.begin(), completed.end());
    }
    
    // 处理最后一个可能未完成的片段
    // ...
    
    return segments;
}

void VoiceActivityDetector::resetStream() {
    m_impl->buffer.clear();
    m_impl->currentTime = 0.0;
    m_impl->inSpeech = false;
    m_impl->speechStartTime = 0.0;
    m_impl->currentEnergy = 0.0f;
}

bool VoiceActivityDetector::processChunk(const AudioBuffer& chunk, int sampleRate,
                                        std::vector<SpeechSegment>& completedSegments) {
    // 计算能量
    float rms = chunk.calculateRMS();
    m_impl->currentEnergy = rms;
    
    double chunkDuration = chunk.size() / static_cast<double>(sampleRate);
    
    // 语音活动检测
    bool isSpeech = rms > m_impl->config.threshold;
    
    if (isSpeech && !m_impl->inSpeech) {
        // 语音开始
        m_impl->inSpeech = true;
        m_impl->speechStartTime = m_impl->currentTime;
    } else if (!isSpeech && m_impl->inSpeech) {
        // 语音结束
        double speechDuration = m_impl->currentTime - m_impl->speechStartTime;
        if (speechDuration >= m_impl->config.minSpeechDuration) {
            SpeechSegment segment;
            segment.startTime = m_impl->speechStartTime;
            segment.endTime = m_impl->currentTime;
            segment.averageEnergy = m_impl->currentEnergy;
            segment.sampleRate = sampleRate;
            completedSegments.push_back(segment);
        }
        m_impl->inSpeech = false;
    }
    
    m_impl->currentTime += chunkDuration;
    return true;
}

void VoiceActivityDetector::setConfig(const Config& config) {
    m_impl->config = config;
}

VoiceActivityDetector::Config VoiceActivityDetector::getConfig() const {
    return m_impl->config;
}

// ============== SpeakerDiarizer ==============

struct SpeakerDiarizer::Impl {
    DiarizationConfig config;
    std::unique_ptr<SpeakerEmbeddingExtractor> embeddingExtractor;
    std::unique_ptr<VoiceActivityDetector> vad;
    std::vector<SpeakerInfo> speakers;
    DiarizationStats stats;
    bool initialized = false;
    std::string lastError;
};

SpeakerDiarizer::SpeakerDiarizer() : m_impl(std::make_unique<Impl>()) {}
SpeakerDiarizer::~SpeakerDiarizer() = default;

bool SpeakerDiarizer::initialize(const DiarizationConfig& config) {
    m_impl->config = config;
    m_impl->initialized = false;
    
    // 初始化嵌入提取器
    m_impl->embeddingExtractor = std::make_unique<SpeakerEmbeddingExtractor>();
    if (!m_impl->embeddingExtractor->initialize()) {
        Logger::warning("Failed to initialize embedding extractor, using fallback");
    }
    
    // 初始化VAD
    VoiceActivityDetector::Config vadConfig;
    vadConfig.minSpeechDuration = config.minSegmentDuration;
    m_impl->vad = std::make_unique<VoiceActivityDetector>(vadConfig);
    
    m_impl->initialized = true;
    Logger::info("Speaker diarizer initialized");
    return true;
}

bool SpeakerDiarizer::isInitialized() const {
    return m_impl->initialized;
}

std::vector<TranscriptionSegment> SpeakerDiarizer::diarize(
    const AudioBuffer& audio, int sampleRate) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. VAD
    auto vadStart = std::chrono::high_resolution_clock::now();
    auto speechSegments = performVAD(audio, sampleRate);
    auto vadEnd = std::chrono::high_resolution_clock::now();
    m_impl->stats.vadTime = std::chrono::duration_cast<std::chrono::milliseconds>(vadEnd - vadStart).count();
    
    Logger::info("VAD found " + std::to_string(speechSegments.size()) + " speech segments");
    
    // 2. 提取嵌入
    auto embedStart = std::chrono::high_resolution_clock::now();
    auto embeddings = extractEmbeddings(audio, sampleRate, speechSegments);
    auto embedEnd = std::chrono::high_resolution_clock::now();
    m_impl->stats.embeddingExtractionTime = std::chrono::duration_cast<std::chrono::milliseconds>(embedEnd - embedStart).count();
    
    // 3. 聚类
    auto clusterStart = std::chrono::high_resolution_clock::now();
    auto labels = performClustering(embeddings);
    auto clusterEnd = std::chrono::high_resolution_clock::now();
    m_impl->stats.clusteringTime = std::chrono::duration_cast<std::chrono::milliseconds>(clusterEnd - clusterStart).count();
    
    // 4. 合并结果
    auto segments = mergeSegmentResults(speechSegments, labels);
    
    // 计算统计信息
    auto endTime = std::chrono::high_resolution_clock::now();
    m_impl->stats.totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    m_impl->stats.detectedSpeakers = *std::max_element(labels.begin(), labels.end()) + 1;
    m_impl->stats.totalSegments = segments.size();
    
    return segments;
}

std::vector<TranscriptionSegment> SpeakerDiarizer::assignSpeakers(
    const AudioBuffer& audio, int sampleRate,
    const std::vector<ASRResult>& asrResults) {
    
    // 首先进行说话人分离
    auto diarizationSegments = diarize(audio, sampleRate);
    
    // 将ASR结果与说话人分离结果对齐
    std::vector<TranscriptionSegment> resultSegments;
    
    for (const auto& asrResult : asrResults) {
        TranscriptionSegment segment;
        segment.startTime = asrResult.startTime;
        segment.endTime = asrResult.endTime;
        segment.text = asrResult.text;
        segment.confidence = asrResult.confidence;
        
        // 找到匹配的说话人
        // 使用最大重叠时间的方法
        int bestSpeakerId = -1;
        double maxOverlap = 0.0;
        
        for (const auto& diarSeg : diarizationSegments) {
            double overlapStart = std::max(asrResult.startTime, diarSeg.startTime);
            double overlapEnd = std::min(asrResult.endTime, diarSeg.endTime);
            double overlap = std::max(0.0, overlapEnd - overlapStart);
            
            if (overlap > maxOverlap) {
                maxOverlap = overlap;
                bestSpeakerId = diarSeg.speakerId;
            }
        }
        
        segment.speakerId = bestSpeakerId;
        resultSegments.push_back(segment);
    }
    
    return resultSegments;
}

std::vector<SpeakerInfo> SpeakerDiarizer::getSpeakerInfo() const {
    return m_impl->speakers;
}

void SpeakerDiarizer::setExpectedSpeakerCount(int count) {
    m_impl->config.expectedSpeakers = count;
}

DiarizationConfig SpeakerDiarizer::getConfig() const {
    return m_impl->config;
}

void SpeakerDiarizer::setConfig(const DiarizationConfig& config) {
    m_impl->config = config;
}

SpeakerDiarizer::DiarizationStats SpeakerDiarizer::getStats() const {
    return m_impl->stats;
}

bool SpeakerDiarizer::saveSpeakerEmbeddings(const std::string& filePath) {
    // TODO: 实现声纹保存
    return true;
}

bool SpeakerDiarizer::loadSpeakerEmbeddings(const std::string& filePath) {
    // TODO: 实现声纹加载
    return true;
}

void SpeakerDiarizer::shutdown() {
    m_impl->embeddingExtractor.reset();
    m_impl->vad.reset();
    m_impl->initialized = false;
}

std::vector<VoiceActivityDetector::SpeechSegment> SpeakerDiarizer::performVAD(
    const AudioBuffer& audio, int sampleRate) {
    return m_impl->vad->detect(audio, sampleRate);
}

std::vector<std::vector<float>> SpeakerDiarizer::extractEmbeddings(
    const AudioBuffer& audio, int sampleRate,
    const std::vector<VoiceActivityDetector::SpeechSegment>& segments) {
    
    std::vector<std::vector<float>> embeddings;
    embeddings.reserve(segments.size());
    
    for (const auto& segment : segments) {
        // 提取片段音频
        size_t startSample = static_cast<size_t>(segment.startTime * sampleRate);
        size_t endSample = static_cast<size_t>(segment.endTime * sampleRate);
        
        AudioBuffer segmentAudio = audio.subBuffer(startSample, endSample - startSample);
        
        // 提取嵌入
        auto embedding = m_impl->embeddingExtractor->extract(segmentAudio, sampleRate);
        embeddings.push_back(embedding);
    }
    
    return embeddings;
}

std::vector<int> SpeakerDiarizer::performClustering(
    const std::vector<std::vector<float>>& embeddings) {
    
    if (embeddings.empty()) {
        return {};
    }
    
    int nSamples = embeddings.size();
    int nClusters = m_impl->config.expectedSpeakers;
    
    // 如果未指定说话人数，使用启发式方法估计
    if (nClusters <= 0) {
        nClusters = std::min(static_cast<int>(std::sqrt(nSamples / 2)), 
                            m_impl->config.maxSpeakers);
        nClusters = std::max(nClusters, m_impl->config.minSpeakers);
    }
    
    // 简单的K-means聚类
    std::vector<int> labels(nSamples, 0);
    std::vector<std::vector<float>> centroids(nClusters, 
        std::vector<float>(embeddings[0].size(), 0.0f));
    
    // 随机初始化中心点
    std::mt19937 gen(42);
    std::uniform_int_distribution<int> dist(0, nSamples - 1);
    for (int i = 0; i < nClusters; ++i) {
        centroids[i] = embeddings[dist(gen)];
    }
    
    // K-means迭代
    const int maxIter = 100;
    for (int iter = 0; iter < maxIter; ++iter) {
        bool changed = false;
        
        // 分配步骤
        for (int i = 0; i < nSamples; ++i) {
            float minDist = std::numeric_limits<float>::max();
            int bestLabel = 0;
            
            for (int j = 0; j < nClusters; ++j) {
                float dist = SpeakerEmbeddingExtractor::computeDistance(
                    embeddings[i], centroids[j]);
                if (dist < minDist) {
                    minDist = dist;
                    bestLabel = j;
                }
            }
            
            if (labels[i] != bestLabel) {
                labels[i] = bestLabel;
                changed = true;
            }
        }
        
        // 更新步骤
        std::vector<int> counts(nClusters, 0);
        for (int j = 0; j < nClusters; ++j) {
            std::fill(centroids[j].begin(), centroids[j].end(), 0.0f);
        }
        
        for (int i = 0; i < nSamples; ++i) {
            int label = labels[i];
            counts[label]++;
            for (size_t k = 0; k < centroids[label].size(); ++k) {
                centroids[label][k] += embeddings[i][k];
            }
        }
        
        for (int j = 0; j < nClusters; ++j) {
            if (counts[j] > 0) {
                for (float& val : centroids[j]) {
                    val /= counts[j];
                }
            }
        }
        
        if (!changed) break;
    }
    
    return labels;
}

std::vector<TranscriptionSegment> SpeakerDiarizer::mergeSegmentResults(
    const std::vector<VoiceActivityDetector::SpeechSegment>& segments,
    const std::vector<int>& labels) {
    
    std::vector<TranscriptionSegment> results;
    
    if (segments.size() != labels.size()) {
        return results;
    }
    
    for (size_t i = 0; i < segments.size(); ++i) {
        TranscriptionSegment segment;
        segment.startTime = segments[i].startTime;
        segment.endTime = segments[i].endTime;
        segment.text = "";  // 将在后续ASR处理中填充
        segment.confidence = 1.0f;
        segment.speakerId = labels[i];
        results.push_back(segment);
    }
    
    return results;
}

} // namespace meeting_transcriber