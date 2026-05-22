#ifndef SDTTABLE_H
#define SDTTABLE_H

#include "BitWriter.h"
#include "BitReader.h"
#include "ChannelInfo.h"
#include <cstdint>
#include <vector>
#include <string>

constexpr uint8_t  TABLE_ID_SDT = 0x42;          // SDT, actual transport stream
constexpr uint16_t ORIGINAL_NETWORK_ID = 0x0001;
constexpr uint8_t  DESC_TAG_SERVICE = 0x48;      // service_descriptor

// service_descriptor (tag 0x48): service_type + provider name + service name.
inline std::vector<uint8_t> buildServiceDescriptor(const ChannelInfo& ch) {
    std::vector<uint8_t> content;
    content.push_back(static_cast<uint8_t>(ch.serviceType));
    content.push_back(static_cast<uint8_t>(ch.providerName.size()));
    for (char c : ch.providerName) content.push_back(static_cast<uint8_t>(c));
    content.push_back(static_cast<uint8_t>(ch.channelName.size()));
    for (char c : ch.channelName) content.push_back(static_cast<uint8_t>(c));

    std::vector<uint8_t> desc;
    desc.push_back(DESC_TAG_SERVICE);
    desc.push_back(static_cast<uint8_t>(content.size()));
    desc.insert(desc.end(), content.begin(), content.end());
    return desc;
}

// SDT body: original_network_id then a per-service loop carrying a service_descriptor.
inline std::vector<uint8_t> buildSdtBody(const std::vector<ChannelInfo>& channels) {
    BitWriter w;
    w.writeBits(ORIGINAL_NETWORK_ID, 16);
    w.writeBits(0xFF, 8);                          // reserved_future_use
    for (const auto& ch : channels) {
        w.writeBits(ch.channelId & 0xFFFF, 16);    // service_id
        w.writeBits(0x3F, 6);                      // reserved_future_use
        w.writeBits(0, 1);                         // EIT_schedule_flag
        w.writeBits(1, 1);                         // EIT_present_following_flag
        w.writeBits(4, 3);                         // running_status (4 = running)
        w.writeBits(0, 1);                         // free_CA_mode
        std::vector<uint8_t> sd = buildServiceDescriptor(ch);
        w.writeBits(static_cast<int>(sd.size()), 12);   // descriptors_loop_length
        for (uint8_t b : sd) w.writeByte(b);
    }
    return w.data();
}

// Fill name / provider / service_type on channels already created from the PAT.
inline void parseSdtBody(const std::vector<uint8_t>& body, std::vector<ChannelInfo>& channels) {
    BitReader r(body.data(), body.size());
    r.readBits(16);                                // original_network_id
    r.readBits(8);                                 // reserved_future_use

    while (r.bytePosition() + 5 <= body.size()) {
        int serviceId = static_cast<int>(r.readBits(16));
        r.readBits(6);                             // reserved_future_use
        r.readBits(1);                             // EIT_schedule_flag
        r.readBits(1);                             // EIT_present_following_flag
        r.readBits(3);                             // running_status
        r.readBits(1);                             // free_CA_mode
        int loopLen = static_cast<int>(r.readBits(12));
        size_t loopEnd = r.bytePosition() + static_cast<size_t>(loopLen);

        ChannelInfo* target = nullptr;
        for (auto& c : channels) if (c.channelId == serviceId) { target = &c; break; }

        while (r.bytePosition() + 2 <= loopEnd) {
            int tag = r.readByte();
            int len = r.readByte();
            size_t descEnd = r.bytePosition() + static_cast<size_t>(len);
            if (tag == DESC_TAG_SERVICE && target) {
                target->serviceType = r.readByte();
                int provLen = r.readByte();
                std::string prov;
                for (int i = 0; i < provLen; ++i) prov.push_back(static_cast<char>(r.readByte()));
                int nameLen = r.readByte();
                std::string name;
                for (int i = 0; i < nameLen; ++i) name.push_back(static_cast<char>(r.readByte()));
                target->providerName = prov;
                target->channelName = name;
            }
            while (r.bytePosition() < descEnd) r.readByte();   // skip unknown/remaining bytes
        }
    }
}

#endif // SDTTABLE_H
