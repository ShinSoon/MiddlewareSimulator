#ifndef TSPACKETREADER_H
#define TSPACKETREADER_H

#include "TsPacket.h"
#include "BitReader.h"
#include <cstdint>
#include <cstddef>

// Reads 188-byte TS packets from a contiguous buffer. Before each packet it
// scans for the 0x47 sync byte, so a stream with leading garbage or a few
// corrupted bytes still re-locks - the same behaviour a hardware demux needs.
class TsPacketReader {
public:
    TsPacketReader(const uint8_t* data, size_t length)
        : data_(data), length_(length) {}

    // Parse the next packet into `out`. Returns false when no complete
    // 188-byte packet remains.
    bool readNext(TsPacket& out) {
        // Resynchronize: advance to the next sync byte.
        while (pos_ < length_ && data_[pos_] != TS_SYNC_BYTE) ++pos_;
        if (pos_ + TS_PACKET_SIZE > length_) return false;

        const uint8_t* p = data_ + pos_;
        BitReader r(p, TS_PACKET_SIZE);
        r.readBits(8);                                 // sync byte (already matched)
        r.readBits(1);                                 // transport_error_indicator
        out.payloadUnitStart = r.readBits(1) != 0;     // PUSI
        r.readBits(1);                                 // transport_priority
        out.pid = static_cast<int>(r.readBits(13));    // PID
        r.readBits(2);                                 // transport_scrambling_control
        r.readBits(2);                                 // adaptation_field_control (assumed 01)
        out.continuityCounter = static_cast<int>(r.readBits(4));

        out.payload.assign(p + TS_HEADER_SIZE, p + TS_PACKET_SIZE);  // 184 bytes
        pos_ += TS_PACKET_SIZE;
        return true;
    }

private:
    const uint8_t* data_;
    size_t length_;
    size_t pos_ = 0;
};

#endif // TSPACKETREADER_H
