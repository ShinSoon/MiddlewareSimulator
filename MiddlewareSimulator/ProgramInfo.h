#ifndef PROGRAMINFO_H
#define PROGRAMINFO_H

#include <string>
#include <chrono> // Include for time_point or duration if used

struct ProgramInfo {
    int programId = 0;
    std::string programName = "Default Program";
    std::string description = "No description.";
    // Store time as epoch milliseconds for easier comparison/calculations
    long long startTimeMillis = 0;
    long long endTimeMillis = 0;
    // Optional: Keep original strings if needed for display formatting
    // std::string startTimeStr;
    // std::string endTimeStr;
};

#endif // PROGRAMINFO_H