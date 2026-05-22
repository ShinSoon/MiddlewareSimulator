#ifndef EITTABLE_H
#define EITTABLE_H

#include "BitWriter.h"
#include "BitReader.h"
#include "ProgramInfo.h"
#include "MjdTime.h"
#include "PsiTables.h"   // TRANSPORT_STREAM_ID
#include "SdtTable.h"    // ORIGINAL_NETWORK_ID
#include <cstdint>
#include <vector>
#include <string>

constexpr uint8_t TABLE_ID_EIT = 0x50;            // EIT schedule, actual transport stream
constexpr uint8_t DESC_TAG_SHORT_EVENT = 0x4D;
constexpr uint8_t DESC_TAG_CONTENT = 0x54;
constexpr uint8_t DESC_TAG_PARENTAL_RATING = 0x55;

// genre <-> DVB content_nibble level 1 (ETSI EN 300 468 Table 28, subset)
inline uint8_t genreToNibble(const std::string& g) {
    if (g == "Movie")     return 0x1;
    if (g == "News")      return 0x2;
    if (g == "Show")      return 0x3;
    if (g == "Sports")    return 0x4;
    if (g == "Children")  return 0x5;
    if (g == "Music")     return 0x6;
    if (g == "Arts")      return 0x7;
    if (g == "Social")    return 0x8;
    if (g == "Education") return 0x9;
    if (g == "Leisure")   return 0xA;
    return 0x0;
}
inline std::string nibbleToGenre(uint8_t n) {
    switch (n) {
        case 0x1: return "Movie";
        case 0x2: return "News";
        case 0x3: return "Show";
        case 0x4: return "Sports";
        case 0x5: return "Children";
        case 0x6: return "Music";
        case 0x7: return "Arts";
        case 0x8: return "Social";
        case 0x9: return "Education";
        case 0xA: return "Leisure";
        default:  return "";
    }
}

// parental rating <-> minimum age (DVB: rating = age - 3 for ages 4..18; 0 = undefined)
inline uint8_t ageToRating(int age) {
    return (age >= 4 && age <= 18) ? static_cast<uint8_t>(age - 3) : 0;
}
inline int ratingToAge(uint8_t rating) {
    return (rating >= 1 && rating <= 15) ? rating + 3 : 0;
}

inline std::vector<uint8_t> wrapDescriptor(uint8_t tag, const std::vector<uint8_t>& content) {
    std::vector<uint8_t> d;
    d.push_back(tag);
    d.push_back(static_cast<uint8_t>(content.size()));
    d.insert(d.end(), content.begin(), content.end());
    return d;
}

inline std::vector<uint8_t> buildShortEventDescriptor(const ProgramInfo& pg) {
    std::vector<uint8_t> c;
    c.push_back('e'); c.push_back('n'); c.push_back('g');   // ISO 639 language code
    c.push_back(static_cast<uint8_t>(pg.programName.size()));
    for (char ch : pg.programName) c.push_back(static_cast<uint8_t>(ch));
    c.push_back(static_cast<uint8_t>(pg.description.size()));
    for (char ch : pg.description) c.push_back(static_cast<uint8_t>(ch));
    return wrapDescriptor(DESC_TAG_SHORT_EVENT, c);
}

inline std::vector<uint8_t> buildContentDescriptor(const ProgramInfo& pg) {
    std::vector<uint8_t> c;
    c.push_back(static_cast<uint8_t>((genreToNibble(pg.genre) << 4) | 0x0));  // level1 | level2
    c.push_back(0xFF);                                                        // user nibbles
    return wrapDescriptor(DESC_TAG_CONTENT, c);
}

inline std::vector<uint8_t> buildParentalRatingDescriptor(const ProgramInfo& pg) {
    std::vector<uint8_t> c;
    c.push_back('G'); c.push_back('B'); c.push_back('R');   // country_code
    c.push_back(ageToRating(pg.parentalAgeRating));
    return wrapDescriptor(DESC_TAG_PARENTAL_RATING, c);
}

// EIT body: fixed header + one record per event (time + duration + descriptors).
inline std::vector<uint8_t> buildEitBody(const std::vector<ProgramInfo>& programs) {
    BitWriter w;
    w.writeBits(TRANSPORT_STREAM_ID, 16);
    w.writeBits(ORIGINAL_NETWORK_ID, 16);
    w.writeBits(0, 8);                  // segment_last_section_number
    w.writeBits(TABLE_ID_EIT, 8);       // last_table_id

    for (const auto& pg : programs) {
        w.writeBits(pg.programId & 0xFFFF, 16);                              // event_id
        for (uint8_t b : encodeStartTime(pg.startTimeMillis)) w.writeByte(b);   // 5 bytes
        long long durSec = (pg.endTimeMillis - pg.startTimeMillis) / 1000;
        for (uint8_t b : encodeDuration(durSec)) w.writeByte(b);                // 3 bytes
        w.writeBits(4, 3);             // running_status (4 = running)
        w.writeBits(0, 1);             // free_CA_mode

        std::vector<uint8_t> descs;
        auto se = buildShortEventDescriptor(pg);
        auto cd = buildContentDescriptor(pg);
        auto pr = buildParentalRatingDescriptor(pg);
        descs.insert(descs.end(), se.begin(), se.end());
        descs.insert(descs.end(), cd.begin(), cd.end());
        descs.insert(descs.end(), pr.begin(), pr.end());
        w.writeBits(static_cast<int>(descs.size()), 12);   // descriptors_loop_length
        for (uint8_t b : descs) w.writeByte(b);
    }
    return w.data();
}

inline std::vector<ProgramInfo> parseEitBody(const std::vector<uint8_t>& body) {
    std::vector<ProgramInfo> programs;
    BitReader r(body.data(), body.size());
    r.readBits(16);   // transport_stream_id
    r.readBits(16);   // original_network_id
    r.readBits(8);    // segment_last_section_number
    r.readBits(8);    // last_table_id

    // Each event is at least 12 bytes (id 2 + start 5 + duration 3 + flags/looplen 2).
    while (r.bytePosition() + 12 <= body.size()) {
        ProgramInfo pg;
        pg.programId = static_cast<int>(r.readBits(16));

        uint8_t st[5];
        for (int i = 0; i < 5; ++i) st[i] = r.readByte();
        pg.startTimeMillis = decodeStartTime(st);

        uint8_t dur[3];
        for (int i = 0; i < 3; ++i) dur[i] = r.readByte();
        pg.endTimeMillis = pg.startTimeMillis + decodeDuration(dur) * 1000;

        r.readBits(3);    // running_status
        r.readBits(1);    // free_CA_mode
        int loopLen = static_cast<int>(r.readBits(12));
        size_t loopEnd = r.bytePosition() + static_cast<size_t>(loopLen);

        while (r.bytePosition() + 2 <= loopEnd) {
            int tag = r.readByte();
            int len = r.readByte();
            size_t descEnd = r.bytePosition() + static_cast<size_t>(len);
            if (tag == DESC_TAG_SHORT_EVENT) {
                r.readByte(); r.readByte(); r.readByte();   // language code
                int nameLen = r.readByte();
                std::string name;
                for (int i = 0; i < nameLen; ++i) name.push_back(static_cast<char>(r.readByte()));
                int textLen = r.readByte();
                std::string text;
                for (int i = 0; i < textLen; ++i) text.push_back(static_cast<char>(r.readByte()));
                pg.programName = name;
                pg.description = text;
            }
            else if (tag == DESC_TAG_CONTENT) {
                uint8_t b0 = r.readByte();
                r.readByte();   // user nibbles
                pg.genre = nibbleToGenre(static_cast<uint8_t>((b0 >> 4) & 0x0F));
            }
            else if (tag == DESC_TAG_PARENTAL_RATING) {
                r.readByte(); r.readByte(); r.readByte();   // country_code
                pg.parentalAgeRating = ratingToAge(r.readByte());
            }
            while (r.bytePosition() < descEnd) r.readByte();   // skip remaining/unknown
        }
        programs.push_back(pg);
    }
    return programs;
}

#endif // EITTABLE_H
