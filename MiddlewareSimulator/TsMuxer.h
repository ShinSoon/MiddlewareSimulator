#ifndef TSMUXER_H
#define TSMUXER_H

#include "ChannelInfo.h"
#include "ProgramInfo.h"
#include "PsiTables.h"
#include "SdtTable.h"
#include "EitTable.h"
#include "PsiSection.h"
#include "TsPacket.h"
#include "TsPacketWriter.h"
#include <vector>
#include <map>
#include <cstdint>

// Encodes a channel list into a transport stream: one PAT on PID 0x0000,
// followed by one PMT per channel on its assigned PID.
class TsMuxer {
public:
    std::vector<uint8_t> mux(std::vector<ChannelInfo> channels,
                             const std::map<int, std::vector<ProgramInfo>>& programsByChannel = {}) {
        assignPidsIfNeeded(channels);

        std::vector<uint8_t> stream;

        // PAT
        PsiSection pat;
        pat.tableId = TABLE_ID_PAT;
        pat.tableIdExtension = TRANSPORT_STREAM_ID;
        pat.body = buildPatBody(channels);
        appendSectionPacket(stream, PID_PAT, sectionWriter_.serialize(pat));

        // one PMT per channel
        for (const auto& ch : channels) {
            PsiSection pmt;
            pmt.tableId = TABLE_ID_PMT;
            pmt.tableIdExtension = static_cast<uint16_t>(ch.channelId);
            pmt.body = buildPmtBody(ch);
            appendSectionPacket(stream, ch.pmtPid, sectionWriter_.serialize(pmt));
        }

        // SDT (service names / provider / type)
        PsiSection sdt;
        sdt.tableId = TABLE_ID_SDT;
        sdt.tableIdExtension = TRANSPORT_STREAM_ID;
        sdt.body = buildSdtBody(channels);
        appendSectionPacket(stream, PID_SDT, sectionWriter_.serialize(sdt));

        // EIT (one section per service that has events) - the EPG
        for (const auto& ch : channels) {
            auto it = programsByChannel.find(ch.channelId);
            if (it == programsByChannel.end() || it->second.empty()) continue;
            PsiSection eit;
            eit.tableId = TABLE_ID_EIT;
            eit.tableIdExtension = static_cast<uint16_t>(ch.channelId);
            eit.body = buildEitBody(it->second);
            appendSectionPacket(stream, PID_EIT, sectionWriter_.serialize(eit));
        }

        return stream;
    }

private:
    // Assign deterministic PIDs to any channel that doesn't already have them.
    void assignPidsIfNeeded(std::vector<ChannelInfo>& channels) {
        int idx = 0;
        for (auto& ch : channels) {
            if (ch.pmtPid == 0)   ch.pmtPid   = 0x0100 + idx * 0x20;
            if (ch.videoPid == 0) ch.videoPid = ch.pmtPid + 1;
            if (ch.audioPid == 0) ch.audioPid = ch.pmtPid + 2;
            ++idx;
        }
    }

    void appendSectionPacket(std::vector<uint8_t>& stream, int pid, const std::vector<uint8_t>& section) {
        TsPacket pkt;
        pkt.pid = pid;
        pkt.payloadUnitStart = true;
        pkt.continuityCounter = 0;
        pkt.payload.push_back(0x00);   // pointer_field = 0 (section starts immediately)
        pkt.payload.insert(pkt.payload.end(), section.begin(), section.end());
        auto bytes = packetWriter_.serialize(pkt);
        stream.insert(stream.end(), bytes.begin(), bytes.end());
    }

    TsPacketWriter packetWriter_;
    PsiSectionWriter sectionWriter_;
};

#endif // TSMUXER_H
