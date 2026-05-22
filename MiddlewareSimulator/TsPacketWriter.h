#ifndef TSPACKETWRITER_H
#define TSPACKETWRITER_H

#include "TsPacket.h"
#include "BitWriter.h"
#include <vector>
#include <cstdint>

// Serializes a TsPacket into exactly 188 bytes: a 4-byte header packed
// bit-by-bit, followed by the payload padded with 0xFF stuffing.
class TsPacketWriter {
public:
    std::vector<uint8_t> serialize(const TsPacket& pkt) {
        BitWriter w;
        w.writeByte(TS_SYNC_BYTE);                          // sync byte 0x47
        w.writeBits(0, 1);                                  // transport_error_indicator
        w.writeBits(pkt.payloadUnitStart ? 1 : 0, 1);       // payload_unit_start_indicator
        w.writeBits(0, 1);                                  // transport_priority
        w.writeBits(pkt.pid & 0x1FFF, 13);                  // PID
        w.writeBits(0, 2);                                  // transport_scrambling_control
        w.writeBits(1, 2);                                  // adaptation_field_control = 01 (payload only)
        w.writeBits(pkt.continuityCounter & 0xF, 4);        // continuity_counter

        std::vector<uint8_t> out = w.data();                // 4 header bytes
        for (int i = 0; i < TS_PAYLOAD_SIZE; ++i) {
            out.push_back(i < static_cast<int>(pkt.payload.size())
                              ? pkt.payload[i]
                              : TS_STUFFING_BYTE);
        }
        return out;                                         // exactly 188 bytes
    }
};

#endif // TSPACKETWRITER_H
