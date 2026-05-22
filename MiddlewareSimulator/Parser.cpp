// Parser.cpp
#include "Parser.h"
#include <sstream>
#include <ostream>     // std::endl and the std::ostream manipulator type (no global objects)
#include <fstream>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <iomanip>
#include <ctime>

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <iostream>    // std::cout / std::cerr (desktop console only)
#endif

// Cross-platform line logger. Diagnostics stream into a buffer, then emit to
// logcat on Android (which has no stdout/stderr) or to std::cout / std::cerr on
// desktop. This keeps the parsing logic identical across both build targets.
namespace {
class LogLine {
public:
    explicit LogLine(bool isError) : isError_(isError) {}
    ~LogLine() {
#ifdef __ANDROID__
        __android_log_print(isError_ ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
                            "MiddlewareSim", "%s", buffer_.str().c_str());
#else
        (isError_ ? std::cerr : std::cout) << buffer_.str() << std::endl;
#endif
    }
    template <typename T>
    LogLine& operator<<(const T& value) { buffer_ << value; return *this; }
    // Swallow stream manipulators such as std::endl.
    LogLine& operator<<(std::ostream& (*)(std::ostream&)) { return *this; }
private:
    std::ostringstream buffer_;
    bool isError_;
};
}  // namespace

#define LOG_OUT LogLine(false)
#define LOG_ERR LogLine(true)

// Convert "YYYY-MM-DD HH:MM:SS" (interpreted as UTC) to epoch milliseconds.
long long Parser::parseDateTimeString(const std::string& dateTimeStr) {
    std::tm t{};
    std::stringstream ss(dateTimeStr);
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        LOG_ERR << "Error parsing date/time string: " << dateTimeStr << std::endl;
        return 0;
    }

    // std::mktime would interpret &t as LOCAL time, shifting by the system
    // timezone. We want UTC, so use the platform's UTC variant.
#ifdef _WIN32
    std::time_t time_seconds = _mkgmtime(&t);
#else
    std::time_t time_seconds = timegm(&t);
#endif

    if (time_seconds == -1) {
        LOG_ERR << "Error converting parsed time to time_t: " << dateTimeStr << std::endl;
        return 0;
    }

    return static_cast<long long>(time_seconds) * 1000;
}

// Split a line into '|'-separated fields.
static std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, '|')) fields.push_back(field);
    return fields;
}

bool Parser::parseDataFromFile(const std::string& filename) {
    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        LOG_ERR << "Error: Could not open file: " << filename << std::endl;
        return false;
    }
    LOG_OUT << "Attempting to parse data from file: \"" << filename << "\"" << std::endl;
    return parseStream(inputFile);
}

bool Parser::parseDataFromString(const std::string& data) {
    std::istringstream input(data);
    return parseStream(input);
}

bool Parser::parseStream(std::istream& input) {
    channels.clear();
    programsByChannelId.clear();
    knownChannelIds.clear();

    std::string recordSegment;
    int lineNumber = 0;

    while (std::getline(input, recordSegment)) {
        lineNumber++;
        if (recordSegment.empty() || recordSegment[0] == '#') continue; // Skip empty lines and comments

        std::vector<std::string> fields = splitFields(recordSegment);
        if (fields.empty()) {
            LOG_ERR << "Warning (Line " << lineNumber << "): Malformed record (missing type): \"" << recordSegment << "\"" << std::endl;
            continue;
        }

        const std::string& recordType = fields[0];

        if (recordType == "CH") {
            // CH|ID|Name|[Provider]|[ServiceType]|[LCN]
            if (fields.size() < 3) {
                LOG_ERR << "Warning (Line " << lineNumber << "): Incomplete CH record (need ID and Name): \"" << recordSegment << "\". Channel not added." << std::endl;
                continue;
            }
            try {
                ChannelInfo currentChannel;
                currentChannel.channelId = std::stoi(fields[1]);
                currentChannel.channelName = fields[2];
                if (fields.size() > 3) currentChannel.providerName = fields[3];
                if (fields.size() > 4) currentChannel.serviceType = std::stoi(fields[4]);
                if (fields.size() > 5) currentChannel.logicalChannelNumber = std::stoi(fields[5]);
                channels.push_back(currentChannel);
                knownChannelIds.insert(currentChannel.channelId);
            }
            catch (const std::exception& e) {
                LOG_ERR << "Error (Line " << lineNumber << "): Parsing CH record: " << e.what() << ". Skipping: \"" << recordSegment << "\"" << std::endl;
            }
        }
        else if (recordType == "PG") {
            // PG|ChannelID|ProgramID|StartTime|EndTime|Name|Description|[Genre]|[ParentalAge]
            if (fields.size() < 7) {
                LOG_ERR << "Warning (Line " << lineNumber << "): Incomplete PG record (need 6 fields after PG, got " << (fields.size() - 1) << "): \"" << recordSegment << "\". Program not added." << std::endl;
                continue;
            }
            try {
                int parsedChannelId = std::stoi(fields[1]);
                ProgramInfo currentProgram;
                currentProgram.programId = std::stoi(fields[2]);
                currentProgram.startTimeMillis = parseDateTimeString(fields[3]);
                currentProgram.endTimeMillis = parseDateTimeString(fields[4]);
                currentProgram.programName = fields[5];
                currentProgram.description = fields[6];
                if (fields.size() > 7) currentProgram.genre = fields[7];
                if (fields.size() > 8) currentProgram.parentalAgeRating = std::stoi(fields[8]);

                // Validation: end-time must follow a valid start-time.
                if (currentProgram.startTimeMillis <= 0 || currentProgram.endTimeMillis <= currentProgram.startTimeMillis) {
                    LOG_ERR << "Warning (Line " << lineNumber << "): Invalid time data for PG record (Start: " << fields[3] << ", End: " << fields[4] << "). Program not added." << std::endl;
                }
                // Validation: channel must have been declared by a prior CH record.
                else if (knownChannelIds.find(parsedChannelId) == knownChannelIds.end()) {
                    LOG_ERR << "Warning (Line " << lineNumber << "): Orphan PG record - channel " << parsedChannelId << " not declared. Skipping: \"" << recordSegment << "\"" << std::endl;
                }
                else {
                    programsByChannelId[parsedChannelId].push_back(currentProgram);
                }
            }
            catch (const std::exception& e) {
                LOG_ERR << "Error (Line " << lineNumber << "): Parsing PG record: " << e.what() << ". Skipping: \"" << recordSegment << "\"" << std::endl;
            }
        }
        else {
            LOG_ERR << "Warning (Line " << lineNumber << "): Unknown record type \"" << recordType << "\" found. Skipping record: \"" << recordSegment << "\"" << std::endl;
        }
    }

    LOG_OUT << "Parsing complete. Stored " << channels.size() << " channels and program entries for " << programsByChannelId.size() << " unique channel IDs." << std::endl;
    return true;
}

std::vector<ChannelInfo> Parser::getChannels() {
    return channels;
}

std::vector<ProgramInfo> Parser::getProgramsForChannel(int channelId) {
    auto it = programsByChannelId.find(channelId);
    if (it != programsByChannelId.end()) {
        return it->second;
    }
    return {}; // Return empty vector
}

// Basic "on now" check using epoch milliseconds.
std::vector<ProgramInfo> Parser::getProgramsOnNow(int channelId, long long currentTimestampMillis) {
    std::vector<ProgramInfo> onNowPrograms;
    auto it = programsByChannelId.find(channelId);
    if (it != programsByChannelId.end()) {
        for (const auto& program : it->second) {
            if (program.startTimeMillis <= currentTimestampMillis && currentTimestampMillis < program.endTimeMillis) {
                onNowPrograms.push_back(program);
            }
        }
    }
    return onNowPrograms;
}

// Check for programs overlapping a given time range.
std::vector<ProgramInfo> Parser::getProgramsForTimeRange(int channelId, long long rangeStartMillis, long long rangeEndMillis) {
    std::vector<ProgramInfo> overlappingPrograms;
    if (rangeStartMillis >= rangeEndMillis) return overlappingPrograms; // Invalid range

    auto it = programsByChannelId.find(channelId);
    if (it != programsByChannelId.end()) {
        for (const auto& program : it->second) {
            // Overlap: program starts before range ends AND program ends after range starts.
            if (program.startTimeMillis < rangeEndMillis && program.endTimeMillis > rangeStartMillis) {
                overlappingPrograms.push_back(program);
            }
        }
    }
    return overlappingPrograms;
}
