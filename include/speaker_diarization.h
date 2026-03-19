#pragma once

#include "utils.h"
#include "audio_processor.h"
#include "asr_engine.h"
#include <memory>
#include <vector>
#include <map>

namespace meeting_transcriber {

// 说话人分离配置
struct DiarizationConfig {
    int expectedSpeakers;           // 预期说话人数（0=自动检测）
    int minSpeakers;                // 最少说话人数
    int maxSpeakers;                // 最多说话人数
    double minSegmentDuration;      // 最小片段时长（秒）
    double maxSegmentDuration;      // 最大片段时长（秒）
    double segmentationWindow;      // 分割窗口大小（秒）
    double segmentationHop;           // 分割步长（秒）
    double clusteringThreshold;     // 聚类阈值
    bool refineSegments;            // 是否优化片段边界
    bool mergeShortPauses;          // 是否合并短停顿
    double shortPauseThreshold;     // 短停顿阈值（秒）
    
    DiarizationConfig() 
        : expectedSpeakers(0), minSpeakers(2), maxSpeakers(10),
          minSegmentDuration(0.5), maxSegmentDuration(30.0),
          segmentationWindow(2.0), segmentationHop(1.0),
          clusteringThreshold(0.75), refineSegments(true),
          mergeShortPauses(true), shortPauseThreshold(0.3) {}
};

// 声纹特征提取器
class SpeakerEmbeddingExtractor {
public:
    SpeakerEmbeddingExtractor();
    ~SpeakerEmbeddingExtractor();
    
    // 初始化
    bool initialize(const std::string& modelPath = "");
    bool isInitialized() const;
    
    // 提取声纹特征
    std::vector<float> extract(const AudioBuffer& audio, int sampleRate);
    
    // 批量提取
    std::vector<std::vector<float>> extractBatch(
        const std::vector<AudioBuffer>& audioBuffers, int sampleRate);
    
    // 计算相似度
    static float computeSimilarity(
        const std::vector<float>& emb1, 
        const std::vector<float>& emb2);
    static float computeDistance(
        const std::vector<float>& emb1, 
        const std::vector<float>& emb2);
    
    // 获取特征维度
    int getEmbeddingDimension() const;
    
    void shutdown();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 语音活动检测（VAD）
class VoiceActivityDetector {
public:
    struct Config {
        float threshold;            // 能量阈值
        float minSpeechDuration;      // 最小语音时长（秒）
        float minSilenceDuration;     // 最小静音时长（秒）
        float preSpeechPadding;       // 前置填充（秒）
        float postSpeechPadding;      // 后置填充（秒）
        
        Config() : threshold(0.02f), minSpeechDuration(0.25f),
                   minSilenceDuration(0.3f), preSpeechPadding(0.1f),
                   postSpeechPadding(0.1f) {}
    };
    
    struct SpeechSegment {
        double startTime;
        double endTime;
        float averageEnergy;
        int sampleRate;
        
        double duration() const { return endTime - startTime; }
    };
    
    VoiceActivityDetector(const Config& config = Config());
    ~VoiceActivityDetector();
    
    // 检测语音片段
    std::vector<SpeechSegment> detect(
        const AudioBuffer& audio, int sampleRate);
    
    // 流式检测（支持实时处理）
    void resetStream();
    bool processChunk(const AudioBuffer& chunk, int sampleRate,
                     std::vector<SpeechSegment>& completedSegments);
    
    // 更新配置
    void setConfig(const Config& config);
    Config getConfig() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 谱聚类
class SpectralClustering {
public:
    struct Config {
        int nClusters;
        int nNeighbors;             // KNN邻居数
        double sigma;               // 高斯核宽度
        int maxIter;                // 最大迭代次数
        double tolerance;           // 收敛容差
        int randomSeed;
        
        Config() : nClusters(2), nNeighbors(10), sigma(1.0),
                   maxIter(100), tolerance(1e-6), randomSeed(42) {}
    };
    
    SpectralClustering(const Config& config = Config());
    ~SpectralClustering();
    
    // 执行聚类
    std::vector<int> fit(const std::vector<std::vector<float>>& features);
    
    // 预测新样本
    std::vector<int> predict(const std::vector<std::vector<float>>& features);
    
    // 获取聚类中心
    std::vector<std::vector<float>> getClusterCenters() const;
    
    // 获取每个样本的聚类分配
    std::vector<int> getLabels() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 说话人分离主类
class SpeakerDiarizer {
public:
    SpeakerDiarizer();
    ~SpeakerDiarizer();
    
    // 禁止拷贝
    SpeakerDiarizer(const SpeakerDiarizer&) = delete;
    SpeakerDiarizer& operator=(const SpeakerDiarizer&) = delete;
    
    // 初始化
    bool initialize(const DiarizationConfig& config = DiarizationConfig());
    bool isInitialized() const;
    
    // 执行说话人分离
    // 返回按时间排序的片段列表，每个片段包含说话人ID
    std::vector<TranscriptionSegment> diarize(
        const AudioBuffer& audio, int sampleRate);
    
    // 与ASR结果结合
    // 输入：音频和ASR转录结果
    // 输出：带有说话人标签的转录结果
    std::vector<TranscriptionSegment> assignSpeakers(
        const AudioBuffer& audio, int sampleRate,
        const std::vector<ASRResult>& asrResults);
    
    // 获取说话人信息
    std::vector<SpeakerInfo> getSpeakerInfo() const;
    
    // 设置说话人数量（用于已知说话人数的场景）
    void setExpectedSpeakerCount(int count);
    
    // 获取配置
    DiarizationConfig getConfig() const;
    void setConfig(const DiarizationConfig& config);
    
    // 获取处理统计信息
    struct DiarizationStats {
        double vadTime;
        double embeddingExtractionTime;
        double clusteringTime;
        double totalTime;
        int detectedSpeakers;
        int totalSegments;
    };
    DiarizationStats getStats() const;
    
    // 保存/加载说话人声纹
    bool saveSpeakerEmbeddings(const std::string& filePath);
    bool loadSpeakerEmbeddings(const std::string& filePath);
    
    void shutdown();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    // 内部处理步骤
    std::vector<VoiceActivityDetector::SpeechSegment> performVAD(
        const AudioBuffer& audio, int sampleRate);
    
    std::vector<std::vector<float>> extractEmbeddings(
        const AudioBuffer& audio, int sampleRate,
        const std::vector<VoiceActivityDetector::SpeechSegment>& segments);
    
    std::vector<int> performClustering(
        const std::vector<std::vector<float>>& embeddings);
    
    std::vector<TranscriptionSegment> mergeSegmentResults(
        const std::vector<VoiceActivityDetector::SpeechSegment>& segments,
        const std::vector<int>& labels);
};

} // namespace meeting_transcriber