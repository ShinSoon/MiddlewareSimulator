// Parser.cpp
#include "Parser.h"
#include <sstream>
#include <fstream> // For file input
#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <chrono> // For time parsing/conversion
#include <iomanip> // For std::get_time
#include <ctime> // For std::mktime, std::tm

// Helper function to convert time string to epoch milliseconds
// Expects format "YYYY-MM-DD HH:MM:SS" (UTC assumed for simplicity)
long long Parser::parseDateTimeString(const std::string& dateTimeStr) {
    std::tm t{};
    std::stringstream ss(dateTimeStr);

    // Use std::get_time to parse the string into a std::tm struct
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        // Handle parsing failure - return 0 or throw an exception
        std::cerr << "Error parsing date/time string: " << dateTimeStr << std::endl;
        return 0; // Indicate failure
    }

    // Convert std::tm to time_t (seconds since epoch), assuming UTC
    // Note: mktime interprets based on local timezone by default. timegm is better for UTC but not standard C++.
    // For this simulation, we'll use mktime and be aware of potential timezone issues in real systems.
    // A more robust solution involves timezone libraries or C++20 chrono features.
    std::time_t time_seconds = std::mktime(&t);
    if (time_seconds == -1) {
        std::cerr << "Error converting parsed time to time_t: " << dateTimeStr << std::endl;
        return 0; // Indicate failure
    }


    // Convert time_t (seconds) to milliseconds
    return static_cast<long long>(time_seconds) * 1000;
}


bool Parser::parseDataFromFile(const std::string& filename) {
    channels.clear();
    programsByChannelId.clear();
    std::cout << "Attempting to parse data from file: \"" << filename << "\"" << std::endl;

    std::ifstream inputFile(filename);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file: " << filename << std::endl;
        return false; // Indicate failure
    }

    std::string recordSegment;
    int lineNumber = 0;

    while (std::getline(inputFile, recordSegment)) {
        lineNumber++;
        if (recordSegment.empty() || recordSegment[0] == '#') continue; // Skip empty lines and comments

        std::stringstream ssRecord(recordSegment);
        std::string recordType;
        std::string part;

        if (!std::getline(ssRecord, recordType, '|')) {
            std::cerr << "Warning (Line " << lineNumber << "): Malformed record (missing type): \"" << recordSegment << "\"" << std::endl;
            continue;
        }

        if (recordType == "CH") {
            std::string idStr, nameStr;
            ChannelInfo currentChannel;
            int partsRead = 0;

            if (std::getline(ssRecord, idStr, '|')) partsRead++;
            if (std::getline(ssRecord, nameStr, '|')) partsRead++;

            if (partsRead == 2) {
                try {
                    currentChannel.channelId = std::stoi(idStr);
                    currentChannel.channelName = nameStr;
                    // TODO: Add check for duplicate channel ID?
                    channels.push_back(currentChannel);
                }
                catch (const std::exception& e) {
                    std::cerr << "Error (Line " << lineNumber << "): Parsing CH ID \"" << idStr << "\": " << e.what() << ". Skipping CH record: \"" << recordSegment << "\"" << std::endl;
                }
            }
            else {
                std::cerr << "Warning (Line " << lineNumber << "): Incomplete CH record (expected 2 parts, got " << partsRead << "): \"" << recordSegment << "\". Channel not added." << std::endl;
            }

        }
        else if (recordType == "PG") {
            std::string channelIdStr, programIdStr, startTimeStr, endTimeStr, nameStr, descStr;
            // Add more strings here if ProgramInfo struct has more fields to parse
            ProgramInfo currentProgram;
            int parsedChannelId = -1;
            int partsRead = 0;

            if (std::getline(ssRecord, channelIdStr, '|')) partsRead++;
            if (std::getline(ssRecord, programIdStr, '|')) partsRead++;
            if (std::getline(ssRecord, startTimeStr, '|')) partsRead++;
            if (std::getline(ssRecord, endTimeStr, '|')) partsRead++;
            if (std::getline(ssRecord, nameStr, '|')) partsRead++;
            if (std::getline(ssRecord, descStr, '|')) partsRead++;
            // Add more std::getline for optional fields

            if (partsRead == 6) { // Check for expected number of mandatory parts
                try {
                    parsedChannelId = std::stoi(channelIdStr);
                    currentProgram.programId = std::stoi(programIdStr);
                    currentProgram.startTimeMillis = parseDateTimeString(startTimeStr);
                    currentProgram.endTimeMillis = parseDateTimeString(endTimeStr);
                    currentProgram.programName = nameStr;
                    currentProgram.description = descStr;

                    // Basic validation: Ensure end time is after start time and times parsed correctly
                    if (currentProgram.startTimeMillis > 0 && currentProgram.endTimeMillis > currentProgram.startTimeMillis) {
                        // TODO: Add check for duplicate program ID within this channel/time?
                        programsByChannelId[parsedChannelId].push_back(currentProgram);
                    }
                    else {
                        std::cerr << "Warning (Line " << lineNumber << "): Invalid time data for PG record (Start: " << startTimeStr << ", End: " << endTimeStr << "). Program not added." << std::endl;
                    }

                }
                catch (const std::exception& e) {
                    std::cerr << "Error (Line " << lineNumber << "): Parsing PG IDs/Times (CH:\"" << channelIdStr << "\", PG:\"" << programIdStr << "\"): " << e.what() << ". Skipping PG record: \"" << recordSegment << "\"" << std::endl;
                }
            }
            else {
                std::cerr << "Warning (Line " << lineNumber << "): Incomplete PG record (expected 6 parts, got " << partsRead << "): \"" << recordSegment << "\". Program not added." << std::endl;
            }

        }
        else {
            std::cerr << "Warning (Line " << lineNumber << "): Unknown record type \"" << recordType << "\" found. Skipping record: \"" << recordSegment << "\"" << std::endl;
        }
    }

    inputFile.close();
    std::cout << "Parsing complete. Stored " << channels.size() << " channels and program entries for " << programsByChannelId.size() << " unique channel IDs." << std::endl;
    return true; // Indicate success
}

std::vector<ChannelInfo> Parser::getChannels() {
    return channels;
}

std::vector<ProgramInfo> Parser::getProgramsForChannel(int channelId) {
    auto it = programsByChannelId.find(channelId);
    if (it != programsByChannelId.end()) {
        return it->second;
    }
    else {
        return {}; // Return empty vector
    }
}

// Basic "on now" check using epoch milliseconds
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

// Check for programs overlapping a given time range
std::vector<ProgramInfo> Parser::getProgramsForTimeRange(int channelId, long long rangeStartMillis, long long rangeEndMillis) {
    std::vector<ProgramInfo> overlappingPrograms;
    if (rangeStartMillis >= rangeEndMillis) return overlappingPrograms; // Invalid range

    auto it = programsByChannelId.find(channelId);
    if (it != programsByChannelId.end()) {
        for (const auto& program : it->second) {
            // Check for overlap: Program starts before range ends AND Program ends after range starts
            if (program.startTimeMillis < rangeEndMillis && program.endTimeMillis > rangeStartMillis) {
                overlappingPrograms.push_back(program);
            }
        }
    }
    // Optionally sort overlappingPrograms by start time here
    return overlappingPrograms;
}