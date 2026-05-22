#ifndef MJDTIME_H
#define MJDTIME_H

#include <cstdint>
#include <vector>
#include <ctime>

// DVB time encoding (ETSI EN 300 468 Annex C):
//   start_time = 16-bit MJD date + 24-bit BCD HHMMSS
//   duration   = 24-bit BCD HHMMSS
// MJD = Modified Julian Date (days since 1858-11-17). BCD packs each decimal
// digit into a nibble (e.g. 45 -> 0x45).

inline uint8_t toBcd(int value) {
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}
inline int fromBcd(uint8_t bcd) {
    return ((bcd >> 4) & 0x0F) * 10 + (bcd & 0x0F);
}

// Calendar date (UTC) -> 16-bit MJD.
inline uint16_t toMjd(int year, int month, int day) {
    int Y = year - 1900;
    int L = (month == 1 || month == 2) ? 1 : 0;
    int mjd = 14956 + day
            + static_cast<int>((Y - L) * 365.25)
            + static_cast<int>((month + 1 + L * 12) * 30.6001);
    return static_cast<uint16_t>(mjd);
}

// 16-bit MJD -> calendar date (UTC, full year).
inline void fromMjd(uint16_t mjd, int& year, int& month, int& day) {
    int yp = static_cast<int>((mjd - 15078.2) / 365.25);
    int mp = static_cast<int>((mjd - 14956.1 - static_cast<int>(yp * 365.25)) / 30.6001);
    day   = mjd - 14956 - static_cast<int>(yp * 365.25) - static_cast<int>(mp * 30.6001);
    int k = (mp == 14 || mp == 15) ? 1 : 0;
    year  = yp + k + 1900;
    month = mp - 1 - k * 12;
}

// epoch milliseconds (UTC) -> 5-byte DVB start_time (MJD16 + BCD HHMMSS).
inline std::vector<uint8_t> encodeStartTime(long long epochMillis) {
    std::time_t t = static_cast<std::time_t>(epochMillis / 1000);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    uint16_t mjd = toMjd(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
    return {
        static_cast<uint8_t>((mjd >> 8) & 0xFF),
        static_cast<uint8_t>(mjd & 0xFF),
        toBcd(tmv.tm_hour),
        toBcd(tmv.tm_min),
        toBcd(tmv.tm_sec)
    };
}

// 5-byte DVB start_time -> epoch milliseconds (UTC).
inline long long decodeStartTime(const uint8_t* p) {
    uint16_t mjd = static_cast<uint16_t>((p[0] << 8) | p[1]);
    int year, month, day;
    fromMjd(mjd, year, month, day);
    std::tm tmv{};
    tmv.tm_year = year - 1900;
    tmv.tm_mon  = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = fromBcd(p[2]);
    tmv.tm_min  = fromBcd(p[3]);
    tmv.tm_sec  = fromBcd(p[4]);
#ifdef _WIN32
    std::time_t t = _mkgmtime(&tmv);
#else
    std::time_t t = timegm(&tmv);
#endif
    return static_cast<long long>(t) * 1000;
}

// duration in seconds -> 3-byte BCD HHMMSS.
inline std::vector<uint8_t> encodeDuration(long long seconds) {
    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>((seconds % 3600) / 60);
    int s = static_cast<int>(seconds % 60);
    return { toBcd(h), toBcd(m), toBcd(s) };
}

// 3-byte BCD HHMMSS -> duration in seconds.
inline long long decodeDuration(const uint8_t* p) {
    return static_cast<long long>(fromBcd(p[0])) * 3600
         + static_cast<long long>(fromBcd(p[1])) * 60
         + fromBcd(p[2]);
}

#endif // MJDTIME_H
