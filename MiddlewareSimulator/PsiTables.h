#ifndef PSITABLES_H
#define PSITABLES_H

#include "BitWriter.h"
#include "BitReader.h"
#include "ChannelInfo.h"
#include <cstdint>
#include <vector>

// --- PSI table constants ---
constexpr uint8_t  TABLE_ID_PAT = 0x00;
constexpr uint8_t  TABLE_ID_PMT = 0x02;
constexpr uint16_t TRANSPORT_STREAM_ID = 0x0001;

// Elementary stream types (subset)
constexpr uint8_t STREAM_TYPE_H264 = 0x1B;   // AVC video
constexpr uint8_t STREAM_TYPE_AAC  = 0x0F;   // AAC audio

struct PatEntry {
    int programNumber = 0;
    int pmtPid = 0;
};

// --- PAT body: one (program_number, PMT PID) record per program ---
inline std::vector<uint8_t> buildPatBody(const std::vector<ChannelInfo>& channels) {
    BitWriter w;
    for (const auto& ch : channels) {
        w.writeBits(ch.channelId & 0xFFFF, 16);    // program_number
        w.writeBits(7, 3);                         // reserved
        w.writeBits(ch.pmtPid & 0x1FFF, 13);       // program_map_PID
    }
    return w.data();
}

inline std::vector<PatEntry> parsePatBody(const std::vector<uint8_t>& body) {
    std::vector<PatEntry> entries;
    BitReader r(body.data(), body.size());
    while (r.bytePosition() + 4 <= body.size()) {
        PatEntry e;
        e.programNumber = static_cast<int>(r.readBits(16));
        r.readBits(3);                             // reserved
        e.pmtPid = static_cast<int>(r.readBits(13));
        entries.push_back(e);
    }
    return entries;
}

// --- PMT body: PCR PID + one video + one audio elementary stream ---
inline std::vector<uint8_t> buildPmtBody(const ChannelInfo& ch) {
    BitWriter w;
    w.writeBits(7, 3);                             // reserved
    w.writeBits(ch.videoPid & 0x1FFF, 13);         // PCR_PID (reuse video PID)
    w.writeBits(15, 4);                            // reserved
    w.writeBits(0, 12);                            // program_info_length = 0

    // video elementary stream
    w.writeByte(STREAM_TYPE_H264);
    w.writeBits(7, 3);                             // reserved
    w.writeBits(ch.videoPid & 0x1FFF, 13);
    w.writeBits(15, 4);                            // reserved
    w.writeBits(0, 12);                            // ES_info_length = 0

    // audio elementary stream
    w.writeByte(STREAM_TYPE_AAC);
    w.writeBits(7, 3);
    w.writeBits(ch.audioPid & 0x1FFF, 13);
    w.writeBits(15, 4);
    w.writeBits(0, 12);
    return w.data();
}

inline void parsePmtBody(const std::vector<uint8_t>& body, ChannelInfo& ch) {
    BitReader r(body.data(), body.size());
    r.readBits(3);                                 // reserved
    r.readBits(13);                                // PCR_PID (ignored here)
    r.readBits(4);                                 // reserved
    int programInfoLength = static_cast<int>(r.readBits(12));
    for (int i = 0; i < programInfoLength; ++i) r.readByte();   // skip program descriptors

    while (r.bytePosition() + 5 <= body.size()) {
        int streamType = r.readByte();
        r.readBits(3);                             // reserved
        int elemPid = static_cast<int>(r.readBits(13));
        r.readBits(4);                             // reserved
        int esInfoLength = static_cast<int>(r.readBits(12));
        for (int i = 0; i < esInfoLength; ++i) r.readByte();    // skip ES descriptors
        if (streamType == STREAM_TYPE_H264)      ch.videoPid = elemPid;
        else if (streamType == STREAM_TYPE_AAC)  ch.audioPid = elemPid;
    }
}

#endif // PSITABLES_H
