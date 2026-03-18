#include "transcript_generator.h"
#include "utils.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace meeting_transcriber {

// ============== MeetingTranscriptDocument ==============

MeetingTranscriptDocument::MeetingTranscriptDocument() 
    : duration(0.0), processedAt(std::chrono::system_clock::now()) {}

void MeetingTranscriptDocument::setSourceFile(const std::string& path) {
    sourceFile = path;
}

void MeetingTranscriptDocument::setTitle(const std::string& title) {
    m_title = title;
}

void MeetingTranscriptDocument::setDate(const std::string& date) {
    m_date = date;
}

void MeetingTranscriptDocument::setDuration(double dur) {
    duration = dur;
}

void MeetingTranscriptDocument::addParticipant(const std::string& name) {
    m_participants.push_back(name);
}

void MeetingTranscriptDocument::setNotes(const std::string& notes) {
    m_notes = notes;
}

void MeetingTranscriptDocument::addSpeaker(const SpeakerInfo& speaker) {
    speakers.push_back(speaker);
}

void MeetingTranscriptDocument::addSegment(const TranscriptionSegment& segment) {
    segments.push_back(segment);
}

void MeetingTranscriptDocument::addParagraph(const TranscriptParagraph& paragraph) {
    m_paragraphs.push_back(paragraph);
}

void MeetingTranscriptDocument::organizeParagraphs(const TranscriptConfig& config) {
    m_paragraphs.clear();
    
    if (segments.empty()) return;
    
    // 按说话人合并连续片段
    TranscriptParagraph currentParagraph;
    currentParagraph.speakerLabel = "";
    currentParagraph.startTime = segments[0].startTime;
    currentParagraph.text = "";
    currentParagraph.avgConfidence = 0.0f;
    
    int currentSpeaker = segments[0].speakerId;
    float confidenceSum = 0.0f;
    int segmentCount = 0;
    
    for (const auto& seg : segments) {
        // 检查是否需要开始新段落
        bool newParagraph = false;
        
        if (seg.speakerId != currentSpeaker) {
            newParagraph = true;
        } else if (config.addParagraphBreaks) {
            double gap = seg.startTime - currentParagraph.startTime;
            if (gap > config.paragraphBreakTime) {
                newParagraph = true;
            }
        }
        
        if (newParagraph && segmentCount > 0) {
            // 保存当前段落
            currentParagraph.endTime = segments[segmentCount - 1].endTime;
            currentParagraph.avgConfidence = confidenceSum / segmentCount;
            
            // 设置说话人标签
            if (currentSpeaker >= 0) {
                currentParagraph.speakerLabel = "Speaker " + std::string(1, 'A' + currentSpeaker);
            }
            
            m_paragraphs.push_back(currentParagraph);
            
            // 开始新段落
            currentParagraph = TranscriptParagraph();
            currentParagraph.startTime = seg.startTime;
            currentParagraph.text = "";
            confidenceSum = 0.0f;
            segmentCount = 0;
        }
        
        // 添加到当前段落
        if (!currentParagraph.text.empty()) {
            currentParagraph.text += " ";
        }
        currentParagraph.text += seg.text;
        
        confidenceSum += seg.confidence;
        segmentCount++;
        currentSpeaker = seg.speakerId;
    }
    
    // 保存最后一个段落
    if (segmentCount > 0 && !currentParagraph.text.empty()) {
        currentParagraph.endTime = segments.back().endTime;
        currentParagraph.avgConfidence = confidenceSum / segmentCount;
        
        if (currentSpeaker >= 0) {
            currentParagraph.speakerLabel = "Speaker " + std::string(1, 'A' + currentSpeaker);
        }
        
        m_paragraphs.push_back(currentParagraph);
    }
}

void MeetingTranscriptDocument::calculateStatistics() {
    m_statistics.totalSegments = segments.size();
    m_statistics.totalParagraphs = m_paragraphs.size();
    m_statistics.speakerCount = speakers.size();
    
    if (segments.empty()) return;
    
    // 计算平均片段时长
    double totalDuration = 0.0;
    for (const auto& seg : segments) {
        totalDuration += (seg.endTime - seg.startTime);
    }
    m_statistics.avgSegmentDuration = totalDuration / segments.size();
    
    // 计算各说话人发言时间
    for (const auto& speaker : speakers) {
        m_statistics.speakerSpeakingTime[speaker.id] = speaker.totalSpeakingTime;
    }
}

// ============== TranscriptGenerator ==============

TranscriptGenerator::TranscriptGenerator() = default;
TranscriptGenerator::~TranscriptGenerator() = default;

void TranscriptGenerator::setConfig(const TranscriptConfig& config) {
    m_config = config;
}

const TranscriptConfig& TranscriptGenerator::getConfig() const {
    return m_config;
}

MeetingTranscriptDocument TranscriptGenerator::generate(
    const std::vector<ASRResult>& asrResults,
    const std::vector<SpeakerInfo>& speakers,
    const std::string& audioFilePath,
    double duration) {
    
    // 转换ASR结果为TranscriptionSegment
    std::vector<TranscriptionSegment> segments;
    segments.reserve(asrResults.size());
    
    for (const auto& asr : asrResults) {
        TranscriptionSegment seg;
        seg.startTime = asr.startTime;
        seg.endTime = asr.endTime;
        seg.text = asr.text;
        seg.confidence = asr.confidence;
        seg.speakerId = asr.speakerId;
        segments.push_back(seg);
    }
    
    return generate(segments, speakers, audioFilePath, duration);
}

MeetingTranscriptDocument TranscriptGenerator::generate(
    const std::vector<TranscriptionSegment>& segments,
    const std::vector<SpeakerInfo>& speakers,
    const std::string& audioFilePath,
    double duration) {
    
    MeetingTranscriptDocument document;
    
    // 设置基本信息
    document.setSourceFile(audioFilePath);
    document.setTitle(m_config.meetingTitle);
    document.setDate(m_config.meetingDate);
    document.setDuration(duration);
    
    for (const auto& participant : m_config.participants) {
        document.addParticipant(participant);
    }
    document.setNotes(m_config.notes);
    
    // 添加说话人信息
    for (const auto& speaker : speakers) {
        document.addSpeaker(speaker);
    }
    
    // 添加片段
    for (const auto& seg : segments) {
        document.addSegment(seg);
    }
    
    // 组织段落
    document.organizeParagraphs(m_config);
    document.calculateStatistics();
    
    return document;
}

std::vector<TranscriptionSegment> TranscriptGenerator::optimizeSegments(
    const std::vector<TranscriptionSegment>& segments) {
    // 应用各种优化
    auto result = mergeConsecutiveSameSpeaker(segments);
    result = mergeShortSegments(result, 0.5);
    result = splitLongSegments(result, 30.0);
    return result;
}

std::vector<TranscriptionSegment> TranscriptGenerator::mergeShortSegments(
    const std::vector<TranscriptionSegment>& segments, double minDuration) {
    std::vector<TranscriptionSegment> result;
    
    for (const auto& seg : segments) {
        double duration = seg.endTime - seg.startTime;
        
        if (duration < minDuration && !result.empty() && 
            result.back().speakerId == seg.speakerId) {
            // 合并到前一个片段
            result.back().endTime = seg.endTime;
            result.back().text += " " + seg.text;
            result.back().confidence = (result.back().confidence + seg.confidence) / 2.0f;
        } else {
            result.push_back(seg);
        }
    }
    
    return result;
}

std::vector<TranscriptionSegment> TranscriptGenerator::splitLongSegments(
    const std::vector<TranscriptionSegment>& segments, double maxDuration) {
    std::vector<TranscriptionSegment> result;
    
    for (const auto& seg : segments) {
        double duration = seg.endTime - seg.startTime;
        
        if (duration > maxDuration) {
            // 分割长片段
            int numParts = static_cast<int>(std::ceil(duration / maxDuration));
            double partDuration = duration / numParts;
            
            // 尝试在句子边界分割
            std::vector<std::string> sentences;
            std::string currentSentence;
            for (char c : seg.text) {
                currentSentence += c;
                if (c == '。' || c == '！' || c == '？' || c == '.' || c == '!' || c == '?') {
                    sentences.push_back(currentSentence);
                    currentSentence.clear();
                }
            }
            if (!currentSentence.empty()) {
                sentences.push_back(currentSentence);
            }
            
            // 将句子分配到各个片段
            int sentencesPerPart = std::max(1, static_cast<int>(sentences.size()) / numParts);
            
            for (int i = 0; i < numParts; ++i) {
                TranscriptionSegment part;
                part.startTime = seg.startTime + i * partDuration;
                part.endTime = std::min(seg.startTime + (i + 1) * partDuration, seg.endTime);
                part.speakerId = seg.speakerId;
                part.confidence = seg.confidence;
                
                // 组合句子
                int startSentence = i * sentencesPerPart;
                int endSentence = std::min(startSentence + sentencesPerPart, 
                                            static_cast<int>(sentences.size()));
                
                if (i == numParts - 1) {
                    endSentence = sentences.size();
                }
                
                part.text.clear();
                for (int j = startSentence; j < endSentence; ++j) {
                    part.text += sentences[j];
                }
                
                // 去除首尾空格
                size_t textStart = part.text.find_first_not_of(" \t\n\r");
                if (textStart != std::string::npos) {
                    size_t textEnd = part.text.find_last_not_of(" \t\n\r");
                    part.text = part.text.substr(textStart, textEnd - textStart + 1);
                }
                
                if (!part.text.empty()) {
                    result.push_back(part);
                }
            }
        } else {
            result.push_back(seg);
        }
    }
    
    return result;
}

std::vector<SpeakerInfo> TranscriptGenerator::assignSpeakerNames(
    const std::vector<SpeakerInfo>& speakers,
    const std::vector<std::string>& knownNames) {
    
    std::vector<SpeakerInfo> result = speakers;
    
    // 按发言时间排序说话人
    std::vector<size_t> indices(result.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
    
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return result[a].totalSpeakingTime > result[b].totalSpeakingTime;
    });
    
    // 分配名字
    for (size_t i = 0; i < indices.size(); ++i) {
        size_t idx = indices[i];
        if (i < knownNames.size()) {
            result[idx].label = knownNames[i];
        } else {
            result[idx].label = "Speaker " + std::string(1, 'A' + i);
        }
    }
    
    return result;
}

std::vector<TranscriptionSegment> TranscriptGenerator::mergeConsecutiveSameSpeaker(
    const std::vector<TranscriptionSegment>& segments) {
    
    std::vector<TranscriptionSegment> result;
    
    for (const auto& seg : segments) {
        if (!result.empty() && 
            result.back().speakerId == seg.speakerId &&
            (seg.startTime - result.back().endTime) < 2.0) {  // 2秒内的片段合并
            // 合并
            result.back().endTime = seg.endTime;
            result.back().text += " " + seg.text;
            result.back().confidence = (result.back().confidence + seg.confidence) / 2.0f;
        } else {
            result.push_back(seg);
        }
    }
    
    return result;
}

std::vector<TranscriptParagraph> TranscriptGenerator::createParagraphs(
    const std::vector<TranscriptionSegment>& segments) {
    
    std::vector<TranscriptParagraph> paragraphs;
    
    // 使用文档的organizeParagraphs方法
    MeetingTranscriptDocument tempDoc;
    for (const auto& seg : segments) {
        tempDoc.addSegment(seg);
    }
    
    tempDoc.organizeParagraphs(m_config);
    
    return tempDoc.getParagraphs();
}

// ============== TranscriptExporter ==============

bool TranscriptExporter::exportToFile(const MeetingTranscriptDocument& document,
                                     const TranscriptConfig& config,
                                     const std::string& outputPath) {
    std::string content;
    
    switch (config.outputFormat) {
        case TranscriptConfig::OutputFormat::MARKDOWN:
            content = document.toMarkdown();
            break;
        case TranscriptConfig::OutputFormat::TEXT:
            content = document.toText();
            break;
        case TranscriptConfig::OutputFormat::JSON:
            content = document.toJson();
            break;
        default:
            content = document.toMarkdown();
    }
    
    return FileUtils::writeTextFile(outputPath, content);
}

std::string TranscriptExporter::generateFilename(const std::string& inputAudioPath,
                                               const TranscriptConfig::OutputFormat format) {
    std::string baseName = FileUtils::getFilename(inputAudioPath);
    
    // 移除扩展名
    size_t dotPos = baseName.find_last_of('.');
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }
    
    std::string ext = getFileExtension(format);
    return baseName + ext;
}

std::string TranscriptExporter::getFileExtension(TranscriptConfig::OutputFormat format) {
    switch (format) {
        case TranscriptConfig::OutputFormat::MARKDOWN: return ".md";
        case TranscriptConfig::OutputFormat::TEXT: return ".txt";
        case TranscriptConfig::OutputFormat::JSON: return ".json";
        case TranscriptConfig::OutputFormat::CSV: return ".csv";
        case TranscriptConfig::OutputFormat::HTML: return ".html";
        case TranscriptConfig::OutputFormat::SRT: return ".srt";
        case TranscriptConfig::OutputFormat::VTT: return ".vtt";
        default: return ".txt";
    }
}

} // namespace meeting_transcriber