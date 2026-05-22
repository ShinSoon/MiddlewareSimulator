#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <map>
#include <iosfwd>
#include <set>       
#include "ChannelInfo.h"
#include "ProgramInfo.h"

class Parser {
public:
    Parser() = default;

    // Parses data from a file specified by filename
    bool parseDataFromFile(const std::string& filename);

    // Parses data directly from an in-memory string (used by the JNI bridge)
    bool parseDataFromString(const std::string& data);

    std::vector<ChannelInfo> getChannels();
    std::vector<ProgramInfo> getProgramsForChannel(int channelId);
    std::vector<ProgramInfo> getProgramsOnNow(int channelId, long long currentTimestampMillis); // Use timestamp
    std::vector<ProgramInfo> getProgramsForTimeRange(int channelId, long long rangeStartMillis, long long rangeEndMillis);
    long long parseDateTimeString(const std::string& dateTimeStr);

private:
    // Shared implementation behind parseDataFromFile / parseDataFromString.
    bool parseStream(std::istream& input);

    std::vector<ChannelInfo> channels;
    std::map<int, std::vector<ProgramInfo>> programsByChannelId;
    std::set<int> knownChannelIds;

    // Private helper to parse a single line record? (Optional refactor)
    // void parseRecord(const std::string& recordSegment);

    // Helper to convert "YYYY-MM-DD HH:MM:SS" string to epoch milliseconds
    
};

#endif // PARSER_H