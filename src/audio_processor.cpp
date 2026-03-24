#include "audio_processor.h"
#include "utils.h"
#include <algorithm>
#include <cmath>

#ifdef HAVE_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}
#endif

namespace meeting_transcriber {

// ============== AudioBuffer ==============

AudioBuffer::AudioBuffer() = default;

AudioBuffer::AudioBuffer(size_t initialCapacity) {
    m_data.reserve(initialCapacity);
}

void AudioBuffer::append(const float* data, size_t count) {
    m_data.insert(m_data.end(), data, data + count);
}

void AudioBuffer::append(const std::vector<float>& data) {
    m_data.insert(m_data.end(), data.begin(), data.end());
}

void AudioBuffer::clear() {
    m_data.clear();
    m_data.shrink_to_fit();
}

void AudioBuffer::reserve(size_t capacity) {
    m_data.reserve(capacity);
}

std::vector<float> AudioBuffer::slice(size_t start, size_t length) const {
    if (start >= m_data.size()) return {};
    size_t end = std::min(start + length, m_data.size());
    return std::vector<float>(m_data.begin() + start, m_data.begin() + end);
}

AudioBuffer AudioBuffer::subBuffer(size_t start, size_t length) const {
    AudioBuffer result;
    result.m_data = slice(start, length);
    return result;
}

void AudioBuffer::normalize() {
    float peak = calculatePeak();
    if (peak > 0.0f) {
        float gain = 1.0f / peak;
        applyGain(gain);
    }
}

void AudioBuffer::applyGain(float gain) {
    for (auto& sample : m_data) {
        sample *= gain;
    }
}

float AudioBuffer::calculateRMS() const {
    if (m_data.empty()) return 0.0f;
    double sum = 0.0;
    for (float sample : m_data) {
        sum += static_cast<double>(sample) * sample;
    }
    return static_cast<float>(std::sqrt(sum / m_data.size()));
}

float AudioBuffer::calculatePeak() const {
    if (m_data.empty()) return 0.0f;
    float peak = 0.0f;
    for (float sample : m_data) {
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

// ============== FFmpegAudioDecoder ==============

#ifdef HAVE_FFMPEG

struct FFmpegAudioDecoder::Impl {
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVFrame* frame = nullptr;
    SwrContext* swrCtx = nullptr;
    int audioStreamIndex = -1;
    std::string lastError;
    bool streaming = false;
    AVPacket* packet = nullptr;
    
    ~Impl() {
        cleanup();
    }
    
    void cleanup() {
        if (swrCtx) {
            swr_free(&swrCtx);
            swrCtx = nullptr;
        }
        if (frame) {
            av_frame_free(&frame);
            frame = nullptr;
        }
        if (packet) {
            av_packet_free(&packet);
            packet = nullptr;
        }
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
            codecCtx = nullptr;
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
            formatCtx = nullptr;
        }
        streaming = false;
    }
    
    void setError(const std::string& error) {
        lastError = error;
        Logger::error("FFmpeg: " + error);
    }
};

FFmpegAudioDecoder::FFmpegAudioDecoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegAudioDecoder::~FFmpegAudioDecoder() = default;

bool FFmpegAudioDecoder::open(const std::string& filePath) {
    m_impl->cleanup();
    
    // 打开输入文件
    int ret = avformat_open_input(&m_impl->formatCtx, filePath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        m_impl->setError("Failed to open file: " + std::string(errbuf));
        return false;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(m_impl->formatCtx, nullptr);
    if (ret < 0) {
        m_impl->setError("Failed to find stream info");
        return false;
    }
    
    // 查找音频流
    for (unsigned int i = 0; i < m_impl->formatCtx->nb_streams; i++) {
        if (m_impl->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_impl->audioStreamIndex = i;
            break;
        }
    }
    
    if (m_impl->audioStreamIndex == -1) {
        m_impl->setError("No audio stream found");
        return false;
    }
    
    // 获取编解码器参数
    AVCodecParameters* codecpar = m_impl->formatCtx->streams[m_impl->audioStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        m_impl->setError("Codec not found");
        return false;
    }
    
    // 分配编解码器上下文
    m_impl->codecCtx = avcodec_alloc_context3(codec);
    if (!m_impl->codecCtx) {
        m_impl->setError("Failed to allocate codec context");
        return false;
    }
    
    // 复制编解码器参数
    ret = avcodec_parameters_to_context(m_impl->codecCtx, codecpar);
    if (ret < 0) {
        m_impl->setError("Failed to copy codec parameters");
        return false;
    }
    
    // 打开编解码器
    ret = avcodec_open2(m_impl->codecCtx, codec, nullptr);
    if (ret < 0) {
        m_impl->setError("Failed to open codec");
        return false;
    }
    
    // 分配帧和包
    m_impl->frame = av_frame_alloc();
    m_impl->packet = av_packet_alloc();
    
    if (!m_impl->frame || !m_impl->packet) {
        m_impl->setError("Failed to allocate frame or packet");
        return false;
    }
    
    Logger::info("FFmpeg decoder opened successfully");
    return true;
}

AudioFormat FFmpegAudioDecoder::getFormat() const {
    AudioFormat format;
    if (m_impl->codecCtx) {
        format.sampleRate = m_impl->codecCtx->sample_rate;
        format.channels = m_impl->codecCtx->ch_layout.nb_channels;
        format.bitsPerSample = av_get_bytes_per_sample(m_impl->codecCtx->sample_fmt) * 8;
    }
    if (m_impl->formatCtx && m_impl->audioStreamIndex >= 0) {
        format.duration = m_impl->formatCtx->streams[m_impl->audioStreamIndex]->duration *
                         av_q2d(m_impl->formatCtx->streams[m_impl->audioStreamIndex]->time_base);
        format.totalSamples = static_cast<uint64_t>(format.duration * format.sampleRate);
    }
    return format;
}

bool FFmpegAudioDecoder::decodeToBuffer(AudioBuffer& buffer, ProgressCallback* callback) {
    if (!m_impl->formatCtx || !m_impl->codecCtx) {
        m_impl->setError("Decoder not initialized");
        return false;
    }
    
    // 获取音频格式
    AudioFormat format = getFormat();
    
    // 初始化重采样器
    m_impl->swrCtx = swr_alloc_set_opts(
        nullptr,
        AV_CHANNEL_LAYOUT_MONO,                     // 输出：单声道
        AV_SAMPLE_FMT_FLT,                          // 输出格式：float
        16000,                                      // 输出采样率：16kHz
        m_impl->codecCtx->ch_layout.u.mask,         // 输入声道布局
        m_impl->codecCtx->sample_fmt,              // 输入格式
        m_impl->codecCtx->sample_rate,             // 输入采样率
        0, nullptr
    );
    
    if (!m_impl->swrCtx) {
        m_impl->setError("Failed to allocate resampler");
        return false;
    }
    
    if (swr_init(m_impl->swrCtx) < 0) {
        m_impl->setError("Failed to initialize resampler");
        return false;
    }
    
    // 获取总时长用于进度计算
    int64_t totalDuration = m_impl->formatCtx->duration;
    int64_t processedDuration = 0;
    int lastProgress = -1;
    
    // 读取并解码
    AVPacket* packet = m_impl->packet;
    AVFrame* frame = m_impl->frame;
    
    while (av_read_frame(m_impl->formatCtx, packet) >= 0) {
        if (packet->stream_index == m_impl->audioStreamIndex) {
            // 发送包到解码器
            int ret = avcodec_send_packet(m_impl->codecCtx, packet);
            if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                continue;
            }
            
            // 接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(m_impl->codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    break;
                }
                
                // 重采样
                uint8_t* outputData = nullptr;
                int outputSamples = swr_convert(
                    m_impl->swrCtx,
                    &outputData, frame->nb_samples * 2,  // 最大输出样本数
                    (const uint8_t**)frame->data, frame->nb_samples
                );
                
                if (outputSamples > 0 && outputData) {
                    // 添加到缓冲区
                    float* floatData = reinterpret_cast<float*>(outputData);
                    buffer.append(floatData, outputSamples);
                }
            }
            
            // 更新进度
            processedDuration += packet->duration;
            if (callback && totalDuration > 0) {
                int progress = static_cast<int>((processedDuration * 100) / totalDuration);
                if (progress != lastProgress) {
                    callback->onProgress("解码音频", progress, 100);
                    lastProgress = progress;
                }
            }
        }
        
        av_packet_unref(packet);
    }
    
    // 刷新解码器
    avcodec_send_packet(m_impl->codecCtx, nullptr);
    while (avcodec_receive_frame(m_impl->codecCtx, frame) == 0) {
        // 处理剩余帧...
    }
    
    if (callback) {
        callback->onProgress("解码音频", 100, 100);
    }
    
    Logger::info("音频解码完成，总样本数: " + std::to_string(buffer.size()));
    return true;
}

void FFmpegAudioDecoder::close() {
    m_impl->cleanup();
}

std::string FFmpegAudioDecoder::getLastError() const {
    return m_impl->lastError;
}

#else  // !HAVE_FFMPEG

// 当FFmpeg不可用的存根实现
struct FFmpegAudioDecoder::Impl {};
FFmpegAudioDecoder::FFmpegAudioDecoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegAudioDecoder::~FFmpegAudioDecoder() = default;
bool FFmpegAudioDecoder::open(const std::string&) { return false; }
AudioFormat FFmpegAudioDecoder::getFormat() const { return {}; }
bool FFmpegAudioDecoder::decodeToBuffer(AudioBuffer&, ProgressCallback*) { return false; }
void FFmpegAudioDecoder::close() {}
std::string FFmpegAudioDecoder::getLastError() const { 
    return "FFmpeg support not compiled in"; 
}

#endif

AudioProcessor::AudioProcessor()
    : m_decoder(std::make_unique<FFmpegAudioDecoder>())
    , m_resampler(std::make_unique<AudioResampler>())
    , m_targetSampleRate(16000)
    , m_targetChannels(1) {
    m_stats = {};
}

AudioProcessor::~AudioProcessor() = default;

void AudioProcessor::setTargetFormat(int sampleRate, int channels) {
    m_targetSampleRate = sampleRate;
    m_targetChannels = channels;
}

bool AudioProcessor::processFile(const std::string& inputPath, AudioBuffer& output, ProgressCallback* callback) {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (callback) {
        callback->onMessage("正在打开音频文件...");
    }

    if (!m_decoder->open(inputPath)) {
        Logger::error("无法打开音频文件: " + inputPath);
        return false;
    }

    AudioFormat inputFormat = m_decoder->getFormat();

    if (callback) {
        callback->onMessage("解码音频...");
    }

    if (!m_decoder->decodeToBuffer(output, callback)) {
        Logger::error("音频解码失败");
        m_decoder->close();
        return false;
    }

    m_decoder->close();

    if (inputFormat.sampleRate != m_targetSampleRate || inputFormat.channels != m_targetChannels) {
        if (callback) {
            callback->onMessage("Resampling...");
        }

        if (!m_resampler->initialize(inputFormat.sampleRate, inputFormat.channels,
                                     m_targetSampleRate, m_targetChannels)) {
            Logger::error("Resampler initialization failed");
            return false;
        }

        size_t inputSamples = output.size();
        size_t outputSamples = m_resampler->calculateOutputSamples(inputSamples);

        std::vector<float> resampledData(outputSamples);
        size_t actualOutputSamples = outputSamples;

        if (!m_resampler->process(output.data(), inputSamples, resampledData.data(), actualOutputSamples)) {
            Logger::error("Resampling failed");
            return false;
        }

        output.clear();
        output.append(resampledData.data(), actualOutputSamples);
        m_stats.resampleOperations++;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.processingTime = std::chrono::duration<double>(endTime - startTime).count();
    m_stats.inputDuration = static_cast<double>(output.size()) / m_targetSampleRate;
    m_stats.outputDuration = m_stats.inputDuration;

    return true;
}

bool AudioProcessor::processFiles(const std::vector<std::string>& inputPaths,
                                   std::vector<AudioBuffer>& outputs,
                                   ProgressCallback* callback) {
    outputs.clear();
    outputs.reserve(inputPaths.size());

    for (size_t i = 0; i < inputPaths.size(); ++i) {
        if (callback) {
            callback->onMessage("处理文件 " + std::to_string(i + 1) + "/" + std::to_string(inputPaths.size()));
        }

        AudioBuffer buffer;
        if (!processFile(inputPaths[i], buffer, callback)) {
            return false;
        }

        outputs.push_back(std::move(buffer));
    }

    return true;
}

AudioProcessor::ProcessingStats AudioProcessor::getStats() const {
    return m_stats;
}

struct AudioResampler::Impl {
    int srcSampleRate = 0;
    int srcChannels = 0;
    int dstSampleRate = 0;
    int dstChannels = 0;
};

AudioResampler::AudioResampler() : m_impl(std::make_unique<Impl>()) {}
AudioResampler::~AudioResampler() = default;

bool AudioResampler::initialize(int srcSampleRate, int srcChannels, int dstSampleRate, int dstChannels) {
    m_impl->srcSampleRate = srcSampleRate;
    m_impl->srcChannels = srcChannels;
    m_impl->dstSampleRate = dstSampleRate;
    m_impl->dstChannels = dstChannels;
    return true;
}

bool AudioResampler::process(const float* input, size_t inputSamples, float* output, size_t& outputSamples) {
    if (!input || !output) return false;
    
    size_t maxOutput = outputSamples;
    outputSamples = 0;
    
    for (size_t i = 0; i < inputSamples && outputSamples < maxOutput; ++i) {
        output[outputSamples++] = input[i];
    }
    
    return true;
}

bool AudioResampler::flush(float* output, size_t& outputSamples) {
    outputSamples = 0;
    return true;
}

size_t AudioResampler::calculateOutputSamples(size_t inputSamples) const {
    return inputSamples;
}

void AudioResampler::reset() {
}

} // namespace meeting_transcriber