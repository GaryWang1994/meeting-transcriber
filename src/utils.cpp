#include "utils.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/types.h>
#endif

namespace meeting_transcriber {

// Static member initialization
LogLevel Logger::s_level = LogLevel::INFO;

// TimeUtils implementation
std::string TimeUtils::formatTimestamp(double seconds) {
    int hours = static_cast<int>(seconds / 3600);
    int minutes = static_cast<int>((seconds - hours * 3600) / 60);
    double secs = seconds - hours * 3600 - minutes * 60;
    
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::fixed << std::setprecision(3) << std::setw(6) << secs;
    return oss.str();
}

std::string TimeUtils::formatDuration(double seconds) {
    if (seconds < 60) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << seconds << "s";
        return oss.str();
    } else if (seconds < 3600) {
        int minutes = static_cast<int>(seconds / 60);
        double secs = seconds - minutes * 60;
        std::ostringstream oss;
        oss << minutes << "m " << std::fixed << std::setprecision(0) << secs << "s";
        return oss.str();
    } else {
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds - hours * 3600) / 60);
        std::ostringstream oss;
        oss << hours << "h " << minutes << "m";
        return oss.str();
    }
}

double TimeUtils::parseTimestamp(const std::string& ts) {
    int hours = 0, minutes = 0;
    double seconds = 0.0;
    
    char colon;
    std::istringstream iss(ts);
    iss >> hours >> colon >> minutes >> colon >> seconds;
    
    return hours * 3600.0 + minutes * 60.0 + seconds;
}

// Logger implementation
void Logger::setLevel(LogLevel level) {
    s_level = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < s_level) return;
    
    std::string timestamp = getTimestamp();
    std::string levelStr = levelToString(level);
    
    std::cerr << "[" << timestamp << "] [" << levelStr << "] " << message << std::endl;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// FileUtils implementation
bool FileUtils::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool FileUtils::directoryExists(const std::string& path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) return false;
#ifdef _WIN32
    return (buffer.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(buffer.st_mode);
#endif
}

bool FileUtils::createDirectory(const std::string& path) {
    if (directoryExists(path)) return true;
    
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

std::string FileUtils::getExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    size_t sepPos = path.find_last_of("/\\");
    
    if (dotPos == std::string::npos) return "";
    if (sepPos != std::string::npos && dotPos < sepPos) return "";
    
    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string FileUtils::getFilename(const std::string& path) {
    size_t sepPos = path.find_last_of("/\\");
    if (sepPos == std::string::npos) return path;
    return path.substr(sepPos + 1);
}

std::string FileUtils::getDirectory(const std::string& path) {
    size_t sepPos = path.find_last_of("/\\");
    if (sepPos == std::string::npos) return "";
    return path.substr(0, sepPos);
}

std::string FileUtils::joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
    char lastA = a.back();
    char firstB = b.front();
    
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    
    if (lastA == sep && firstB == sep) {
        return a + b.substr(1);
    } else if (lastA != sep && firstB != sep) {
        return a + sep + b;
    } else {
        return a + b;
    }
}

std::string FileUtils::readTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileUtils::writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

// ConsoleProgressCallback implementation
void ConsoleProgressCallback::onProgress(const std::string& stage, int current, int total) {
    int percent = static_cast<int>((current * 100.0) / total);
    std::cerr << "\r" << stage << ": " << percent << "% (" << current << "/" << total << ")";
    if (current == total) {
        std::cerr << std::endl;
    }
    std::cerr.flush();
}

void ConsoleProgressCallback::onMessage(const std::string& message) {
    std::cerr << std::endl << message << std::endl;
}

// MeetingTranscript implementation
std::string MeetingTranscript::toMarkdown() const {
    std::ostringstream oss;
    
    oss << "# Meeting Transcript\n\n";
    oss << "**Source File:** " << sourceFile << "\n\n";
    oss << "**Duration:** " << TimeUtils::formatDuration(duration) << "\n\n";
    oss << "**Processed At:** " << std::chrono::system_clock::to_time_t(processedAt) << "\n\n";
    oss << "---\n\n";
    
    for (const auto& segment : segments) {
        oss << "[" << TimeUtils::formatTimestamp(segment.startTime) << "] ";
        if (segment.speakerId >= 0) {
            oss << "**Speaker " << static_cast<char>('A' + segment.speakerId) << "**: ";
        }
        oss << segment.text << "\n\n";
    }
    
    return oss.str();
}

std::string MeetingTranscript::toText() const {
    std::ostringstream oss;
    
    for (const auto& segment : segments) {
        oss << "[" << TimeUtils::formatTimestamp(segment.startTime) << "] ";
        if (segment.speakerId >= 0) {
            oss << "Speaker " << static_cast<char>('A' + segment.speakerId) << ": ";
        }
        oss << segment.text << "\n";
    }
    
    return oss.str();
}

std::string MeetingTranscript::toJson() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"sourceFile\": \"" << sourceFile << "\",\n";
    oss << "  \"duration\": " << duration << ",\n";
    oss << "  \"speakers\": [\n";
    
    for (size_t i = 0; i < speakers.size(); ++i) {
        oss << "    {\n";
        oss << "      \"id\": " << speakers[i].id << ",\n";
        oss << "      \"label\": \"" << speakers[i].label << "\"\n";
        oss << "    }";
        if (i < speakers.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ],\n";
    oss << "  \"segments\": [\n";
    
    for (size_t i = 0; i < segments.size(); ++i) {
        oss << "    {\n";
        oss << "      \"startTime\": " << segments[i].startTime << ",\n";
        oss << "      \"endTime\": " << segments[i].endTime << ",\n";
        oss << "      \"text\": \"" << segments[i].text << "\",\n";
        oss << "      \"speakerId\": " << segments[i].speakerId << "\n";
        oss << "    }";
        if (i < segments.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "  ]\n";
    oss << "}\n";
    
    return oss.str();
}

} // namespace meeting_transcriber
