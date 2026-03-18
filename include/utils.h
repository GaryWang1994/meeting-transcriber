#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace meeting_transcriber {

// 时间格式化工具
class TimeUtils {
public:
    static std::string formatTimestamp(double seconds);
    static std::string formatDuration(double seconds);
    static double parseTimestamp(const std::string& ts);
};

// 日志系统
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static void setLevel(LogLevel level);
    static void log(LogLevel level, const std::string& message);
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

private:
    static LogLevel s_level;
    static std::string getTimestamp();
    static std::string levelToString(LogLevel level);
};

// 文件操作工具
class FileUtils {
public:
    static bool fileExists(const std::string& path);
    static bool directoryExists(const std::string& path);
    static bool createDirectory(const std::string& path);
    static std::string getExtension(const std::string& path);
    static std::string getFilename(const std::string& path);
    static std::string getDirectory(const std::string& path);
    static std::string joinPath(const std::string& a, const std::string& b);
    static std::string readTextFile(const std::string& path);
    static bool writeTextFile(const std::string& path, const std::string& content);
};

// 进度回调接口
class ProgressCallback {
public:
    virtual ~ProgressCallback() = default;
    virtual void onProgress(const std::string& stage, int current, int total) = 0;
    virtual void onMessage(const std::string& message) = 0;
};

// 简单的控制台进度回调实现
class ConsoleProgressCallback : public ProgressCallback {
public:
    void onProgress(const std::string& stage, int current, int total) override;
    void onMessage(const std::string& message) override;
};

// 数据结构定义

// 音频片段信息
struct AudioSegment {
    double startTime;           // 开始时间（秒）
    double endTime;             // 结束时间（秒）
    std::vector<float> samples; // PCM音频样本
    int sampleRate;             // 采样率
    int channels;               // 声道数
};

// 转录结果（单个片段）
struct TranscriptionSegment {
    double startTime;           // 开始时间
    double endTime;             // 结束时间
    std::string text;           // 转录文本
    float confidence;           // 置信度
    int speakerId;              // 说话人ID（-1表示未分配）
};

// 说话人信息
struct SpeakerInfo {
    int id;                     // 说话人ID
    std::string label;          // 显示标签（如"Speaker A"）
    int segmentCount;           // 片段数量
    double totalSpeakingTime;   // 总说话时间
    std::vector<float> embedding; // 声纹特征向量
};

// 完整会议记录
struct MeetingTranscript {
    std::string sourceFile;                     // 源音频文件
    double duration;                            // 总时长
    std::chrono::system_clock::time_point processedAt; // 处理时间
    std::vector<SpeakerInfo> speakers;          // 说话人列表
    std::vector<TranscriptionSegment> segments; // 转录片段
    
    // 导出为Markdown格式
    std::string toMarkdown() const;
    
    // 导出为纯文本格式
    std::string toText() const;
    
    // 导出为JSON格式
    std::string toJson() const;
};

} // namespace meeting_transcriber