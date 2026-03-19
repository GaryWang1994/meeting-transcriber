#include "utils.h"
#include "audio_processor.h"
#include "asr_engine.h"
#include "speaker_diarization.h"
#include "transcript_generator.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace meeting_transcriber;

// 程序版本
const char* VERSION = "1.0.0";

// 打印使用说明
void printUsage(const char* programName) {
    std::cout << "会议录音转文字工具 v" << VERSION << "\n\n";
    std::cout << "用法: " << programName << " <输入音频文件> [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  -o, --output <路径>       输出文件路径（默认：自动生成）\n";
    std::cout << "  -m, --model <路径>        ASR模型路径\n";
    std::cout << "  -l, --language <语言>     语言代码 (zh/en/auto, 默认：auto)\n";
    std::cout << "  -s, --speakers <数量>     说话人数（0=自动检测）\n";
    std::cout << "  -f, --format <格式>       输出格式 (md/txt/json/csv/html, 默认：md)\n";
    std::cout << "  --no-timestamps          不包含时间戳\n";
    std::cout << "  --no-speaker-labels      不包含说话人标签\n";
    std::cout << "  --gpu                    使用GPU加速\n";
    std::cout << "  --threads <数量>         设置线程数\n";
    std::cout << "  -v, --verbose            显示详细日志\n";
    std::cout << "  -h, --help               显示此帮助信息\n";
    std::cout << "\n示例:\n";
    std::cout << "  " << programName << " meeting.m4a\n";
    std::cout << "  " << programName << " meeting.m4a -o output.md --speakers 3\n";
    std::cout << "  " << programName << " meeting.m4a -l zh -f json --gpu\n";
}

// 命令行参数结构体
struct CommandLineArgs {
    std::string inputFile;
    std::string outputFile;
    std::string modelPath;
    std::string language = "auto";
    int expectedSpeakers = 0;
    std::string format = "md";
    bool includeTimestamps = true;
    bool includeSpeakerLabels = true;
    bool useGPU = false;
    int numThreads = 4;
    bool verbose = false;
};

// 解析命令行参数
bool parseArguments(int argc, char* argv[], CommandLineArgs& args) {
    if (argc < 2) {
        return false;
    }
    
    args.inputFile = argv[1];
    
    // 检查是否是帮助请求
    if (args.inputFile == "-h" || args.inputFile == "--help") {
        return false;
    }
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            args.outputFile = argv[++i];
        } else if ((arg == "-m" || arg == "--model") && i + 1 < argc) {
            args.modelPath = argv[++i];
        } else if ((arg == "-l" || arg == "--language") && i + 1 < argc) {
            args.language = argv[++i];
        } else if ((arg == "-s" || arg == "--speakers") && i + 1 < argc) {
            args.expectedSpeakers = std::stoi(argv[++i]);
        } else if ((arg == "-f" || arg == "--format") && i + 1 < argc) {
            args.format = argv[++i];
        } else if (arg == "--no-timestamps") {
            args.includeTimestamps = false;
        } else if (arg == "--no-speaker-labels") {
            args.includeSpeakerLabels = false;
        } else if (arg == "--gpu") {
            args.useGPU = true;
        } else if (arg == "--threads" && i + 1 < argc) {
            args.numThreads = std::stoi(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            return false;
        }
    }
    
    return true;
}

// 主处理函数
int processAudioFile(const CommandLineArgs& args) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 设置日志级别
    Logger::setLevel(args.verbose ? LogLevel::DEBUG : LogLevel::INFO);
    
    Logger::info("会议录音转文字工具 v" + std::string(VERSION));
    Logger::info("输入文件: " + args.inputFile);
    
    // 检查输入文件
    if (!FileUtils::fileExists(args.inputFile)) {
        Logger::error("输入文件不存在: " + args.inputFile);
        return 1;
    }
    
    // 确定输出文件路径
    std::string outputFile = args.outputFile;
    if (outputFile.empty()) {
        std::string baseName = FileUtils::getFilename(args.inputFile);
        size_t dotPos = baseName.find_last_of('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos);
        }
        outputFile = baseName + "." + args.format;
    }
    
    Logger::info("输出文件: " + outputFile);
    
    // 创建进度回调
    ConsoleProgressCallback progressCallback;
    
    // 初始化音频处理器
    Logger::info("正在初始化音频处理器...");
    AudioProcessor audioProcessor;
    audioProcessor.setTargetFormat(16000, 1); // 16kHz单声道
    
    // 处理音频文件
    Logger::info("正在处理音频文件...");
    AudioBuffer audioBuffer;
    
    progressCallback.onMessage("步骤 1/4: 解码音频...");
    if (!audioProcessor.processFile(args.inputFile, audioBuffer, &progressCallback)) {
        Logger::error("音频处理失败");
        return 1;
    }
    
    Logger::info("音频处理完成，样本数: " + std::to_string(audioBuffer.size()));
    
    // 初始化ASR引擎
    Logger::info("正在初始化ASR引擎...");
    ASRConfig asrConfig;
    asrConfig.modelPath = args.modelPath;
    asrConfig.language = args.language;
    asrConfig.useGPU = args.useGPU;
    asrConfig.intraOpNumThreads = args.numThreads;
    asrConfig.interOpNumThreads = args.numThreads;
    
    auto asrEngine = ASREngineFactory::createEngine();
    if (!asrEngine->initialize(asrConfig)) {
        Logger::error("ASR引擎初始化失败，尝试使用Python桥接...");
        // TODO: 实现Python桥接作为fallback
        return 1;
    }
    
    // 执行ASR
    Logger::info("正在执行语音识别...");
    progressCallback.onMessage("步骤 2/4: 语音识别...");
    
    auto asrResults = asrEngine->transcribe(audioBuffer, 16000);
    Logger::info("ASR完成，识别到 " + std::to_string(asrResults.size()) + " 个片段");
    
    progressCallback.onProgress("语音识别", 100, 100);
    
    // 说话人分离
    Logger::info("正在进行说话人分离...");
    progressCallback.onMessage("步骤 3/4: 说话人分离...");
    
    DiarizationConfig diarizationConfig;
    diarizationConfig.expectedSpeakers = args.expectedSpeakers;
    
    SpeakerDiarizer diarizer;
    if (!diarizer.initialize(diarizationConfig)) {
        Logger::warning("说话人分离初始化失败，将跳过此步骤");
    } else {
        // 结合ASR结果和说话人分离
        auto segmentsWithSpeakers = diarizer.assignSpeakers(
            audioBuffer, 16000, asrResults);
        
        Logger::info("说话人分离完成，检测到 " + 
                    std::to_string(diarizer.getSpeakerInfo().size()) + " 个说话人");
        
        // 生成转录文档
        progressCallback.onMessage("步骤 4/4: 生成会议记录...");
        
        TranscriptConfig transcriptConfig;
        transcriptConfig.includeTimestamps = args.includeTimestamps;
        transcriptConfig.includeSpeakerLabels = args.includeSpeakerLabels;
        
        if (args.format == "md") transcriptConfig.outputFormat = OutputFormat::MARKDOWN;
        else if (args.format == "txt") transcriptConfig.outputFormat = OutputFormat::TEXT;
        else if (args.format == "json") transcriptConfig.outputFormat = OutputFormat::JSON;
        else if (args.format == "csv") transcriptConfig.outputFormat = OutputFormat::CSV;
        else if (args.format == "html") transcriptConfig.outputFormat = OutputFormat::HTML;
        
        TranscriptGenerator generator;
        generator.setConfig(transcriptConfig);
        
        auto document = generator.generate(segmentsWithSpeakers,
                                          diarizer.getSpeakerInfo(),
                                          args.inputFile,
                                          audioBuffer.size() / 16000.0);
        
        // 导出文件
        std::string content;
        switch (transcriptConfig.outputFormat) {
            case OutputFormat::MARKDOWN:
                content = document.toMarkdown();
                break;
            case OutputFormat::TEXT:
                content = document.toText();
                break;
            case OutputFormat::JSON:
                content = document.toJson();
                break;
            case OutputFormat::CSV:
                // CSV需要特殊处理
                content = "timestamp,speaker,text,confidence\n";
                for (const auto& seg : document.segments) {
                    content += TimeUtils::formatTimestamp(seg.startTime) + ",";
                    content += (seg.speakerId >= 0 ? "Speaker " + std::string(1, 'A' + seg.speakerId) : "Unknown") + ",";
                    content += "\"" + seg.text + "\",";
                    content += std::to_string(seg.confidence) + "\n";
                }
                break;
            case OutputFormat::HTML:
                content = R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Meeting Transcript</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 40px auto; padding: 20px; }
        h1 { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }
        .meta { background: #f5f5f5; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .speaker { font-weight: bold; color: #007acc; }
        .timestamp { color: #666; font-size: 0.9em; }
        .segment { margin: 15px 0; padding: 10px; border-left: 3px solid #007acc; }
    </style>
</head>
<body>
    <h1>会议记录</h1>
)";
                content += "<div class='meta'>";
                content += "<strong>音频文件:</strong> " + document.sourceFile + "<br>";
                content += "<strong>时长:</strong> " + TimeUtils::formatDuration(document.duration) + "<br>";
                content += "<strong>说话人数:</strong> " + std::to_string(document.speakers.size());
                content += "</div>";
                
                for (const auto& seg : document.segments) {
                    content += "<div class='segment'>";
                    content += "<span class='timestamp'>[" + TimeUtils::formatTimestamp(seg.startTime) + "]</span> ";
                    if (seg.speakerId >= 0) {
                        content += "<span class='speaker'>Speaker " + std::string(1, 'A' + seg.speakerId) + ":</span> ";
                    }
                    content += seg.text;
                    content += "</div>";
                }
                
                content += "</body></html>";
                break;
            default:
                content = document.toText();
        }
        
        if (FileUtils::writeTextFile(outputFile, content)) {
            Logger::info("会议记录已保存到: " + outputFile);
        } else {
            Logger::error("保存文件失败: " + outputFile);
            return 1;
        }
        
        progressCallback.onProgress("完成", 100, 100);
        
        // 计算总耗时
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
        Logger::info("处理完成，总耗时: " + std::to_string(duration.count()) + " 秒");
        
        return 0;
    }
}

// 主函数
int main(int argc, char* argv[]) {
    // 设置控制台代码页（Windows）
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    // 打印横幅
    std::cout << R"(
    __  ___      ___       __  __      ______                __  
   /  |/  /___ _/ (_)___ _/ / / /___ _/ ____/________ ______/ /__
  / /|_/ / __ `/ / / __ `/ /_/ / __ `/ /   / ___/ _ `/ ___/ //_/
 / /  / / /_/ / / / /_/ / __  / /_/ / /___/ /  / /_/ / /__/ ,<   
/_/  /_/\__,_/_/_/\__, /_/ /_/\__,_/\____/_/   \__,_/\___/_/|_|  
                 /____/                                          
)" << std::endl;
    
    std::cout << "会议录音转文字工具 v" << VERSION << std::endl;
    std::cout << "基于 Qwen3-ASR-1.7B 模型\n" << std::endl;
    
    // 解析命令行参数
    CommandLineArgs args;
    if (!parseArguments(argc, argv, args)) {
        printUsage(argv[0]);
        return 1;
    }
    
    // 执行处理
    return processAudioFile(args);
}