#pragma once

#include "utils.h"
#include "audio_processor.h"
#include "asr_engine.h"
#include "speaker_diarization.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace meeting_transcriber {

// Transcript output format
enum class OutputFormat {
    TEXT,       // Plain text
    MARKDOWN,   // Markdown with formatting
    JSON,       // JSON for programmatic use
    SRT,        // Subtitle format
    VTT         // WebVTT format
};

// Transcript generator configuration
struct TranscriptConfig {
    OutputFormat format;
    bool includeTimestamps;
    bool includeSpeakerLabels;
    bool includeConfidence;
    std::string timestampFormat;      // "HH:MM:SS" or "MM:SS"
    int timestampPrecision;            // Decimal places for seconds
    bool groupConsecutiveSpeakers;    // Group same speaker segments
    std::string speakerLabelPrefix;    // "Speaker " or custom
    
    TranscriptConfig() 
        : format(OutputFormat::TEXT), includeTimestamps(true),
          includeSpeakerLabels(true), includeConfidence(false),
          timestampFormat("HH:MM:SS"), timestampPrecision(0),
          groupConsecutiveSpeakers(true), speakerLabelPrefix("Speaker ") {}
};

// Transcript segment with speaker info
struct RichTranscriptSegment {
    double startTime;
    double endTime;
    std::string text;
    int speakerId;
    std::string speakerLabel;
    float confidence;
    std::vector<std::string> words;
    std::vector<double> wordTimestamps;
};

// Transcript generator class
class TranscriptGenerator {
public:
    TranscriptGenerator();
    ~TranscriptGenerator();
    
    // Set configuration
    void setConfig(const TranscriptConfig& config);
    TranscriptConfig getConfig() const;
    
    // Generate transcript from meeting record
    std::string generate(const MeetingTranscript& transcript);
    
    // Generate from segments directly
    std::string generateFromSegments(const std::vector<TranscriptionSegment>& segments,
                                     const std::vector<SpeakerInfo>& speakers);
    
    // Generate rich transcript with word-level timestamps
    std::vector<RichTranscriptSegment> generateRichTranscript(
        const std::vector<TranscriptionSegment>& segments,
        const std::vector<SpeakerInfo>& speakers);
    
    // Save to file
    bool saveToFile(const MeetingTranscript& transcript, const std::string& filePath);
    bool saveToFile(const std::string& content, const std::string& filePath);
    
    // Format-specific generators
    std::string generateText(const MeetingTranscript& transcript);
    std::string generateMarkdown(const MeetingTranscript& transcript);
    std::string generateJSON(const MeetingTranscript& transcript);
    std::string generateSRT(const MeetingTranscript& transcript);
    std::string generateVTT(const MeetingTranscript& transcript);
    
    // Helper methods
    static std::string formatSpeakerLabel(int speakerId, const std::string& prefix = "Speaker ");
    static std::string escapeMarkdown(const std::string& text);
    static std::string escapeJSON(const std::string& text);
    
private:
    TranscriptConfig m_config;
    
    // Group consecutive segments by same speaker
    std::vector<TranscriptionSegment> groupSegments(const std::vector<TranscriptionSegment>& segments);
    
    // Format single segment
    std::string formatSegment(const TranscriptionSegment& segment, const SpeakerInfo* speaker);
    
    // Generate speaker map
    std::map<int, SpeakerInfo> createSpeakerMap(const std::vector<SpeakerInfo>& speakers);
};

// Meeting statistics generator
class MeetingStatistics {
public:
    struct Stats {
        double totalDuration;
        double totalSpeechDuration;
        int numSpeakers;
        int numSegments;
        int totalWords;
        std::map<int, double> speakerSpeakingTime;
        std::map<int, int> speakerWordCount;
        std::map<int, int> speakerSegmentCount;
        double averageSegmentDuration;
        double wordsPerMinute;
    };
    
    static Stats calculate(const MeetingTranscript& transcript);
    static std::string generateReport(const Stats& stats);
    static std::string generateMarkdownReport(const Stats& stats);
};

// Transcript editor (basic editing capabilities)
class TranscriptEditor {
public:
    TranscriptEditor();
    ~TranscriptEditor();
    
    // Load transcript
    bool load(const std::string& filePath);
    bool load(const MeetingTranscript& transcript);
    
    // Edit operations
    void editSegmentText(int index, const std::string& newText);
    void changeSpeaker(int segmentIndex, int newSpeakerId);
    void mergeSegments(int startIndex, int endIndex);
    void splitSegment(int index, double splitTime);
    void addSpeaker(const std::string& label);
    void removeSpeaker(int speakerId);
    void renameSpeaker(int speakerId, const std::string& newLabel);
    
    // Get edited transcript
    MeetingTranscript getTranscript() const;
    
    // Save
    bool save(const std::string& filePath);
    
private:
    MeetingTranscript m_transcript;
    bool m_modified;
};

} // namespace meeting_transcriber
