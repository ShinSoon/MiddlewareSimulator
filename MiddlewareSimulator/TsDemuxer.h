#ifndef TSDEMUXER_H
#define TSDEMUXER_H

#include "ChannelInfo.h"
#include "ProgramInfo.h"
#include "PsiTables.h"
#include "SdtTable.h"
#include "EitTable.h"
#include "PsiSection.h"
#include "TsPacket.h"
#include "TsPacketReader.h"
#include <vector>
#include <map>
#include <cstdint>

// Decodes a transport stream back into a channel list: parse the PAT to learn
// each program's PMT PID, then parse each PMT for its elementary stream PIDs.
class TsDemuxer {
public:
    std::vector<ChannelInfo> demux(const std::vector<uint8_t>& tsBytes,
                                   std::map<int, std::vector<ProgramInfo>>* programsOut = nullptr) {
        std::map<int, std::vector<uint8_t>> sectionByPid;
        std::vector<std::vector<uint8_t>> eitSections;   // EITs share PID 0x12 - collect them all

        TsPacketReader reader(tsBytes.data(), tsBytes.size());
        TsPacket pkt;
        while (reader.readNext(pkt)) {
            if (!pkt.payloadUnitStart) continue;
            if (pkt.pid == PID_EIT) {
                eitSections.push_back(extractSection(pkt));
            } else if (!sectionByPid.count(pkt.pid)) {
                sectionByPid[pkt.pid] = extractSection(pkt);
            }
        }

        std::vector<ChannelInfo> channels;
        PsiSectionReader sr;

        auto patIt = sectionByPid.find(PID_PAT);
        if (patIt == sectionByPid.end()) return channels;

        PsiSection patSection;
        if (!sr.parse(patIt->second.data(), patIt->second.size(), patSection)) return channels;

        for (const auto& entry : parsePatBody(patSection.body)) {
            ChannelInfo ch;
            ch.channelId = entry.programNumber;
            ch.pmtPid = entry.pmtPid;

            auto pmtIt = sectionByPid.find(entry.pmtPid);
            if (pmtIt != sectionByPid.end()) {
                PsiSection pmtSection;
                if (sr.parse(pmtIt->second.data(), pmtIt->second.size(), pmtSection))
                    parsePmtBody(pmtSection.body, ch);
            }
            channels.push_back(ch);
        }

        // SDT: fill in service names / provider / type for the channels above.
        auto sdtIt = sectionByPid.find(PID_SDT);
        if (sdtIt != sectionByPid.end()) {
            PsiSection sdtSection;
            if (sr.parse(sdtIt->second.data(), sdtIt->second.size(), sdtSection))
                parseSdtBody(sdtSection.body, channels);
        }

        // EIT: recover each service's events (the EPG), keyed by service_id.
        if (programsOut) {
            for (const auto& sec : eitSections) {
                PsiSection eitSection;
                if (sr.parse(sec.data(), sec.size(), eitSection))
                    (*programsOut)[eitSection.tableIdExtension] = parseEitBody(eitSection.body);
            }
        }

        return channels;
    }

private:
    // A PUSI packet's payload begins with a 1-byte pointer_field; the section
    // starts pointer_field bytes after it. (Trailing 0xFF stuffing is harmless -
    // the section reader stops at section_length.)
    static std::vector<uint8_t> extractSection(const TsPacket& pkt) {
        if (pkt.payload.empty()) return {};
        size_t start = 1 + static_cast<size_t>(pkt.payload[0]);
        if (start > pkt.payload.size()) return {};
        return std::vector<uint8_t>(pkt.payload.begin() + start, pkt.payload.end());
    }
};

#endif // TSDEMUXER_H
