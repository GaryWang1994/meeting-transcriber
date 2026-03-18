#pragma once

#include "utils.h"
#include <memory>
#include <vector>
#include <string>

// FFmpeg forward declarations
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct SwrContext;

namespace meeting_transcriber {

// 音频格式信息
struct AudioFormat {
    int sampleRate;      // 采样率 (Hz)
    int channels;        // 声道数
    int bitsPerSample;   // 位深
    uint64_t totalSamples; // 总样本数
    double duration;     // 时长（秒）
    
    AudioFormat() : sampleRate(16000), channels(1), bitsPerSample(16), 
                    totalSamples(0), duration(0.0) {}
};

// 音频缓冲区
class AudioBuffer {
public:
    AudioBuffer();
    AudioBuffer(size_t initialCapacity);
    
    // 添加样本数据
    void append(const float* data, size_t count);
    void append(const std::vector<float>& data);
    
    // 获取数据指针
    const float* data() const { return m_data.data(); }
    float* data() { return m_data.data(); }
    
    // 获取大小
    size_t size() const { return m_data.size(); }
    bool empty() const { return m_data.empty(); }
    
    // 清空
    void clear();
    void reserve(size_t capacity);
    
    // 切片操作
    std::vector<float> slice(size_t start, size_t length) const;
    AudioBuffer subBuffer(size_t start, size_t length) const;
    
    // 归一化
    void normalize();
    void applyGain(float gain);
    
    // 计算RMS能量
    float calculateRMS() const;
    float calculatePeak() const;
    
private:
    std::vector<float> m_data;
};

// FFmpeg音频解码器
class FFmpegAudioDecoder {
public:
    FFmpegAudioDecoder();
    ~FFmpegAudioDecoder();
    
    // 禁止拷贝
    FFmpegAudioDecoder(const FFmpegAudioDecoder&) = delete;
    FFmpegAudioDecoder& operator=(const FFmpegAudioDecoder&) = delete;
    
    // 打开音频文件
    bool open(const std::string& filePath);
    
    // 获取音频格式信息
    AudioFormat getFormat() const;
    
    // 解码整个文件到缓冲区
    bool decodeToBuffer(AudioBuffer& buffer, ProgressCallback* callback = nullptr);
    
    // 流式解码（用于大文件）
    bool startStreaming();
    bool readNextChunk(AudioBuffer& chunk, size_t targetSamples);
    void stopStreaming();
    
    // 跳转到指定时间（秒）
    bool seekToTime(double seconds);
    
    // 关闭文件
    void close();
    
    // 获取错误信息
    std::string getLastError() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 音频重采样器
class AudioResampler {
public:
    AudioResampler();
    ~AudioResampler();
    
    // 初始化重采样器
    bool initialize(int srcSampleRate, int srcChannels,
                   int dstSampleRate, int dstChannels);
    
    // 处理音频数据
    bool process(const float* input, size_t inputSamples,
                float* output, size_t& outputSamples);
    
    // 刷新缓冲区
    bool flush(float* output, size_t& outputSamples);
    
    // 计算输出样本数
    size_t calculateOutputSamples(size_t inputSamples) const;
    
    void reset();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// 音频处理器（高层封装）
class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();
    
    // 设置目标格式（默认为16kHz单声道）
    void setTargetFormat(int sampleRate = 16000, int channels = 1);
    
    // 处理音频文件
    bool processFile(const std::string& inputPath, AudioBuffer& output,
                    ProgressCallback* callback = nullptr);
    
    // 批量处理
    bool processFiles(const std::vector<std::string>& inputPaths,
                     std::vector<AudioBuffer>& outputs,
                     ProgressCallback* callback = nullptr);
    
    // 获取处理统计信息
    struct ProcessingStats {
        double inputDuration;
        double outputDuration;
        double processingTime;
        int resampleOperations;
    };
    ProcessingStats getStats() const;
    
private:
    std::unique_ptr<FFmpegAudioDecoder> m_decoder;
    std::unique_ptr<AudioResampler> m_resampler;
    int m_targetSampleRate;
    int m_targetChannels;
    ProcessingStats m_stats;
};

// 音频分块器（用于ASR分段）
class AudioChunker {
public:
    struct Chunk {
        double startTime;
        double endTime;
        std::vector<float> samples;
    };
    
    AudioChunker(double chunkDuration = 30.0, double overlap = 1.0);
    
    void setChunkParams(double duration, double overlap);
    
    std::vector<Chunk> chunkAudio(const AudioBuffer& audio, int sampleRate);
    
    // 基于语音活动的智能分块
    std::vector<Chunk> chunkByVAD(const AudioBuffer& audio, int sampleRate,
                                   double silenceThreshold = 0.5);
    
private:
    double m_chunkDuration;
    double m_overlap;
};

} // namespace meeting_transcriber