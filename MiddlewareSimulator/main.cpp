#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>   // For getting current time
#include <ctime>    // For formatting time
#include <iomanip>  // For formatting time
#include <fstream>
#include <sstream>
#include <cstdint>

#include "ChannelInfo.h"
#include "ProgramInfo.h"
#include "Parser.h"
#include "TsMuxer.h"
#include "TsDemuxer.h"
#include "CecMessage.h"


// Helper to format epoch milliseconds to readable time string (UTC)
std::string formatTime(long long timeMillis) {
    if (timeMillis <= 0) return "N/A";
    auto timePoint = std::chrono::system_clock::time_point(std::chrono::milliseconds(timeMillis));
    std::time_t timeT = std::chrono::system_clock::to_time_t(timePoint);
    // Use gmtime for UTC, localtime for local time zone
    std::tm* utcTm = std::gmtime(&timeT); // Be cautious with gmtime/localtime (not thread-safe)
    if (!utcTm) return "Invalid Time";
    std::stringstream ss;
    ss << std::put_time(utcTm, "%Y-%m-%d %H:%M:%S"); // ISO 8601-like format
    return ss.str();
}

// Write a raw byte buffer to a file (used for the .ts transport stream).
static bool writeBinaryFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return f.good();
}

// Read a whole file into a byte buffer.
static std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> data;
    if (!f) return data;
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (size > 0) {
        data.resize(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(data.data()), size);
    }
    return data;
}

// Format a byte buffer as space-separated uppercase hex.
static std::string cecHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) ss << " ";
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// Print one CEC message as a decoded, sniffer-style trace line.
static void printCecTrace(CecCodec& codec, const CecMessage& m) {
    std::vector<uint8_t> bytes = codec.serialize(m);
    std::cout << "  [" << cecHex(bytes) << "]  "
              << cecAddressName(m.initiator) << " -> " << cecAddressName(m.destination) << " : ";
    if (!m.hasOpcode) {
        std::cout << "(polling)";
    } else {
        std::cout << cecOpcodeName(m.opcode);
        if (!m.operands.empty()) std::cout << " [" << cecHex(m.operands) << "]";
    }
    std::cout << std::endl;
}


int main(int argc, char* argv[]) {

    // --- Argument Parsing ---
    std::string dataFilename;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_file_path> " << std::endl;
        return 1;
    }

    dataFilename = argv[1]; // First argument is filename

    // --- Normal Simulation Run ---
    std::cout << "--- Middleware Simulator Started (Normal Run) ---" << std::endl << std::endl;

    Parser dataParser;

    std::cout << "Calling parser for file: " << dataFilename << " ..." << std::endl;
    if (!dataParser.parseDataFromFile(dataFilename)) {
        std::cerr << "Failed to parse data file. Exiting." << std::endl;
        return 1; // Exit if parsing failed
    }
    std::cout << std::endl;

    std::cout << "Retrieving parsed channels..." << std::endl;
    std::vector<ChannelInfo> parsedChannels = dataParser.getChannels();

    if (parsedChannels.empty()) {
        std::cout << "No valid channels were parsed from the file." << std::endl;
    }
    else {
        std::cout << "Parsed Channel List:" << std::endl;
        std::cout << "--------------------" << std::endl;
        for (const ChannelInfo& channel : parsedChannels) {
            std::cout << "ID: " << channel.channelId << ", Name: \"" << channel.channelName << "\"" << std::endl;
        }
        std::cout << "--------------------" << std::endl << std::endl;

        std::cout << "Retrieving program info for each channel..." << std::endl;
        for (const ChannelInfo& channel : parsedChannels) {
            std::cout << "\n--- Programs for Channel " << channel.channelId << " (" << channel.channelName << ") ---" << std::endl;
            std::vector<ProgramInfo> programs = dataParser.getProgramsForChannel(channel.channelId);

            if (programs.empty()) {
                std::cout << "  No programs found for this channel." << std::endl;
            }
            else {
                for (const ProgramInfo& program : programs) {
                    std::cout << "  Prog ID: " << program.programId
                        << ", Name: \"" << program.programName << "\""
                        << ", Start: " << formatTime(program.startTimeMillis)
                        << ", End: " << formatTime(program.endTimeMillis)
                        << ", Desc: \"" << program.description << "\"" << std::endl;
                }
            }
        }
        std::cout << "-----------------------------------------" << std::endl;

        // Example: Test "on now" query
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
        std::cout << "\n--- Programs On Now (" << formatTime(now_ms) << ") for Channel 1 ---" << std::endl;
        std::vector<ProgramInfo> onNow = dataParser.getProgramsOnNow(1, now_ms); // Test for Channel 1
        if (onNow.empty()) {
            std::cout << "  No programs currently running on Channel 1." << std::endl;
        }
        else {
            for (const auto& p : onNow) {
                std::cout << "  * " << p.programName << " (ID: " << p.programId << ")" << std::endl;
            }
        }
        std::cout << "-----------------------------------------" << std::endl;


        // --- MPEG-TS Pipeline Demo: channels -> .ts file -> channels ---
        std::cout << "\n=== MPEG-TS Pipeline Demo ===" << std::endl;

        // 1. Encode the parsed channels into a transport stream (PAT + PMTs).
        TsMuxer muxer;
        std::vector<uint8_t> tsStream = muxer.mux(parsedChannels);
        std::cout << "Muxed " << parsedChannels.size() << " channels into "
            << tsStream.size() << " bytes ("
            << (tsStream.size() / TS_PACKET_SIZE) << " TS packets)." << std::endl;

        // 2. Write the transport stream to a .ts file on disk.
        const std::string tsFilename = "output.ts";
        if (writeBinaryFile(tsFilename, tsStream))
            std::cout << "Wrote transport stream to \"" << tsFilename << "\"." << std::endl;
        else
            std::cerr << "Failed to write \"" << tsFilename << "\"." << std::endl;

        // 3. Read the .ts file back and demux it.
        std::vector<uint8_t> readBack = readBinaryFile(tsFilename);
        TsDemuxer demuxer;
        std::vector<ChannelInfo> recovered = demuxer.demux(readBack);

        // 4. Print the recovered service / PID map.
        std::cout << "Demuxed " << recovered.size() << " services from the stream:" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        for (const ChannelInfo& ch : recovered) {
            std::cout << "  Service " << ch.channelId
                << " | PMT PID 0x" << std::hex << std::setw(4) << std::setfill('0') << ch.pmtPid
                << " | Video PID 0x" << std::setw(4) << std::setfill('0') << ch.videoPid
                << " | Audio PID 0x" << std::setw(4) << std::setfill('0') << ch.audioPid
                << std::dec << std::setfill(' ') << std::endl;
        }
        std::cout << "----------------------------------------" << std::endl;

    }

    // --- HDMI-CEC Sniffer Demo: build and decode a few control messages ---
    std::cout << "\n=== HDMI-CEC Sniffer Demo ===" << std::endl;
    {
        CecCodec cec;
        std::vector<CecMessage> trace = {
            cecImageViewOn(CEC_ADDR_PLAYBACK_1, CEC_ADDR_TV),
            cecActiveSource(CEC_ADDR_PLAYBACK_1, 0x1000),
            cecReportPowerStatus(CEC_ADDR_TV, CEC_ADDR_PLAYBACK_1, CEC_POWER_ON),
            cecRequestArcInitiation(CEC_ADDR_AUDIO_SYSTEM, CEC_ADDR_TV),
            cecPolling(CEC_ADDR_PLAYBACK_1, CEC_ADDR_TV),
            cecStandby(CEC_ADDR_TV, CEC_ADDR_BROADCAST),
        };
        for (const CecMessage& m : trace) printCecTrace(cec, m);
    }
    std::cout << "----------------------------------------" << std::endl;

    std::cout << std::endl << "--- Middleware Simulator Finished ---" << std::endl;
    return 0;
}