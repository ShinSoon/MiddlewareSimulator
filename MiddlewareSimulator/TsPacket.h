#ifndef TSPACKET_H
#define TSPACKET_H

#include <cstdint>
#include <vector>

// --- MPEG-TS packet constants ---
constexpr uint8_t TS_SYNC_BYTE   = 0x47;
constexpr int     TS_PACKET_SIZE = 188;
constexpr int     TS_HEADER_SIZE = 4;
constexpr int     TS_PAYLOAD_SIZE = 184;
constexpr uint8_t TS_STUFFING_BYTE = 0xFF;

// --- Well-known PIDs (fixed by the MPEG/DVB standards) ---
constexpr int PID_PAT  = 0x0000;   // Program Association Table
constexpr int PID_SDT  = 0x0011;   // Service Description Table (DVB)
constexpr int PID_EIT  = 0x0012;   // Event Information Table (DVB) - the EPG
constexpr int PID_NULL = 0x1FFF;   // null / stuffing packets

// A single transport-stream packet. For this simulator we model only
// payload-only packets (no adaptation field), which is all the PSI tables need.
struct TsPacket {
    bool payloadUnitStart = false;       // PUSI: payload starts a new PSI section
    int  pid = PID_NULL;                 // 13-bit packet identifier
    int  continuityCounter = 0;          // 4-bit per-PID counter
    std::vector<uint8_t> payload;        // up to 184 bytes of section data
};

#endif // TSPACKET_H
