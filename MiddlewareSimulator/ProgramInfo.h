#ifndef PROGRAMINFO_H
#define PROGRAMINFO_H

#include <string>
#include <chrono> // Include for time_point or duration if used

struct ProgramInfo {
    int programId = 0;                  // = EIT event_id
    std::string programName = "Default Program";
    std::string description = "No description.";
    // Store time as epoch milliseconds for easier comparison/calculations
    long long startTimeMillis = 0;
    long long endTimeMillis = 0;
    std::string genre;                  // EIT content_descriptor
    int parentalAgeRating = 0;          // EIT parental_rating_descriptor (min age; 0 = unrated)
};

#endif // PROGRAMINFO_H