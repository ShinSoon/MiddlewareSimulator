#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>   // For getting current time
#include <ctime>    // For formatting time
#include <iomanip>  // For formatting time
#include <cassert>  // For basic tests
#include <fstream>
#include <sstream>

#include "ChannelInfo.h"
#include "ProgramInfo.h"
#include "Parser.h"

// --- Basic Test Functions (Example using assert) ---
void run_parser_tests() {
    std::cout << "\n--- Running Basic Parser Tests ---" << std::endl;
    Parser testParser;
    bool result;

    // Test 1: Valid CH record
    result = testParser.parseDataFromFile("test_data_valid_ch.txt"); // Needs corresponding file
    assert(result && testParser.getChannels().size() == 1 && testParser.getChannels()[0].channelId == 1);
    std::cout << "Test Valid CH: PASS" << std::endl;

    // Test 2: Valid PG record (requires CH record first usually for map key)
    result = testParser.parseDataFromFile("test_data_valid_pg.txt"); // Needs corresponding file
    assert(result && !testParser.getProgramsForChannel(1).empty() && testParser.getProgramsForChannel(1)[0].programId == 101);
    std::cout << "Test Valid PG: PASS" << std::endl;

    // Test 3: Invalid ID
    result = testParser.parseDataFromFile("test_data_invalid_id.txt");
    assert(result && testParser.getChannels().empty()); // Expect parser to skip invalid CH
    std::cout << "Test Invalid ID: PASS" << std::endl;

    // Test 4: Incomplete Record
    result = testParser.parseDataFromFile("test_data_incomplete.txt");
    assert(result && testParser.getChannels().empty() && testParser.getProgramsForChannel(1).empty());
    std::cout << "Test Incomplete Record: PASS" << std::endl;

    // Test 5: EPG Query (requires valid CH/PG data file)
    result = testParser.parseDataFromFile("test_data_epg_query.txt");
    assert(result);
    // Get current time for "on now" test (approximate)
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch().count();
    // Note: This test depends heavily on the content of test_data_epg_query.txt and current time
    // Need programs spanning 'now' in the test file for getProgramsOnNow to return something.
    // std::vector<ProgramInfo> onNow = testParser.getProgramsOnNow(1, now_ms);
    // assert(!onNow.empty()); // This assertion might fail depending on test data timing
    // std::cout << "Test EPG On Now Query: PASS (or check manually based on data)" << std::endl;

    // Test 6: Time Range Query
    // Assuming test_data_epg_query.txt has PG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|...
    long long rangeStart = testParser.parseDateTimeString("2025-04-16 09:30:00");
    long long rangeEnd = testParser.parseDateTimeString("2025-04-16 10:30:00");
    std::vector<ProgramInfo> rangeOverlap = testParser.getProgramsForTimeRange(1, rangeStart, rangeEnd);
    assert(!rangeOverlap.empty() && rangeOverlap[0].programId == 101);
    std::cout << "Test EPG Range Query: PASS" << std::endl;


    std::cout << "--- Basic Parser Tests Finished ---" << std::endl;
}

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


int main(int argc, char* argv[]) {

    // --- Argument Parsing ---
    std::string dataFilename;
    bool runTests = false;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <data_file_path> [--test]" << std::endl;
        return 1;
    }

    dataFilename = argv[1]; // First argument is filename

    // Check for optional --test flag
    if (argc > 2 && std::string(argv[2]) == "--test") {
        runTests = true;
    }

    // --- Run Tests OR Normal Simulation ---
    if (runTests) {
        // NOTE: Test functions need dedicated test files (e.g., test_data_valid_ch.txt)
        // Create these files with the appropriate content for tests to pass.
        std::cout << "Test mode requested. Creating dummy test files if they don't exist..." << std::endl;
        // Example: Create dummy files - replace with actual content needed for your asserts
        std::ofstream("test_data_valid_ch.txt") << "CH|10|Valid Name";
        std::ofstream("test_data_valid_pg.txt") << "CH|1|Chan1\nPG|1|101|2025-01-01 10:00:00|2025-01-01 11:00:00|Prog1|Desc1";
        std::ofstream("test_data_invalid_id.txt") << "CH|abc|Invalid";
        std::ofstream("test_data_incomplete.txt") << "PG|1|102";
        std::ofstream("test_data_epg_query.txt") << "CH|1|RangeTest\nPG|1|101|2025-04-16 09:00:00|2025-04-16 10:00:00|InRange|DescInRange";

        run_parser_tests();
        // Clean up dummy files
        // remove("test_data_valid_ch.txt"); remove("test_data_valid_pg.txt"); ...
        return 0; // Exit after running tests
    }


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


    }

    std::cout << std::endl << "--- Middleware Simulator Finished ---" << std::endl;
    return 0;
}