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
    TEXT,
    MARKDOWN,
    JSON,
    CSV,
    HTML,
    SRT,
    VTT
};

// Transcript generator configuration
struct TranscriptConfig {
    OutputFormat format;
    OutputFormat outputFormat;
    bool includeTimestamps;
    bool includeSpeakerLabels;
    bool includeConfidence;
    std::string timestampFormat;
    int timestampPrecision;
    bool groupConsecutiveSpeakers;
    std::string speakerLabelPrefix;
    bool addParagraphBreaks;
    double paragraphBreakTime;
    std::string meetingTitle;
    std::string meetingDate;
    std::vector<std::string> participants;
    std::string notes;

    TranscriptConfig()
        : format(OutputFormat::TEXT), outputFormat(OutputFormat::TEXT),
          includeTimestamps(true),
          includeSpeakerLabels(true), includeConfidence(false),
          timestampFormat("HH:MM:SS"), timestampPrecision(0),
          groupConsecutiveSpeakers(true), speakerLabelPrefix("Speaker "),
          addParagraphBreaks(true), paragraphBreakTime(5.0) {}
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

struct TranscriptParagraph {
    double startTime;
    double endTime;
    std::string speakerLabel;
    std::string text;
    float avgConfidence;
    std::vector<int> segmentIndices;
};

class MeetingTranscriptDocument {
public:
    std::string sourceFile;
    double duration;
    std::chrono::system_clock::time_point processedAt;
    std::vector<SpeakerInfo> speakers;
    std::vector<TranscriptionSegment> segments;
    
    struct Statistics {
        int totalSegments;
        int totalParagraphs;
        int speakerCount;
        double avgSegmentDuration;
        std::map<int, double> speakerSpeakingTime;
    };

    MeetingTranscriptDocument();
    
    void setSourceFile(const std::string& path);
    void setTitle(const std::string& title);
    void setDate(const std::string& date);
    void setDuration(double dur);
    void addParticipant(const std::string& name);
    void setNotes(const std::string& notes);
    void addSpeaker(const SpeakerInfo& speaker);
    void addSegment(const TranscriptionSegment& segment);
    void addParagraph(const TranscriptParagraph& paragraph);
    void organizeParagraphs(const TranscriptConfig& config);
    void calculateStatistics();
    
    const std::vector<TranscriptParagraph>& getParagraphs() const { return m_paragraphs; }
    std::string toMarkdown() const;
    std::string toText() const;
    std::string toJson() const;

private:
    std::string m_title;
    std::string m_date;
    std::vector<std::string> m_participants;
    std::string m_notes;
    std::vector<TranscriptParagraph> m_paragraphs;
    Statistics m_statistics;
};

// Transcript generator class
class TranscriptGenerator {
public:
    TranscriptGenerator();
    ~TranscriptGenerator();
    
    void setConfig(const TranscriptConfig& config);
    const TranscriptConfig& getConfig() const;
    
    MeetingTranscriptDocument generate(
        const std::vector<ASRResult>& asrResults,
        const std::vector<SpeakerInfo>& speakers,
        const std::string& audioFilePath,
        double duration);

    MeetingTranscriptDocument generate(
        const std::vector<TranscriptionSegment>& segments,
        const std::vector<SpeakerInfo>& speakers,
        const std::string& audioFilePath,
        double duration);
    
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
    
    std::map<int, SpeakerInfo> createSpeakerMap(const std::vector<SpeakerInfo>& speakers);

    std::vector<TranscriptionSegment> optimizeSegments(const std::vector<TranscriptionSegment>& segments);
    std::vector<TranscriptionSegment> mergeShortSegments(const std::vector<TranscriptionSegment>& segments, double minDuration);
    std::vector<TranscriptionSegment> splitLongSegments(const std::vector<TranscriptionSegment>& segments, double maxDuration);
    std::vector<SpeakerInfo> assignSpeakerNames(const std::vector<SpeakerInfo>& speakers, const std::vector<std::string>& knownNames);
    std::vector<TranscriptionSegment> mergeConsecutiveSameSpeaker(const std::vector<TranscriptionSegment>& segments);
    std::vector<TranscriptParagraph> createParagraphs(const std::vector<TranscriptionSegment>& segments);
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

class TranscriptExporter {
public:
    static bool exportToFile(const MeetingTranscriptDocument& document,
                            const TranscriptConfig& config,
                            const std::string& outputPath);
    static std::string generateFilename(const std::string& inputAudioPath,
                                         const OutputFormat format);
    static std::string getFileExtension(OutputFormat format);
};

} // namespace meeting_transcriber
