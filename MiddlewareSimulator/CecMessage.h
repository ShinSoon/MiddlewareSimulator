#ifndef CECMESSAGE_H
#define CECMESSAGE_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// --- HDMI-CEC logical addresses (assigned by device type) ---
constexpr int CEC_ADDR_TV            = 0;
constexpr int CEC_ADDR_RECORDING_1   = 1;
constexpr int CEC_ADDR_RECORDING_2   = 2;
constexpr int CEC_ADDR_TUNER_1       = 3;
constexpr int CEC_ADDR_PLAYBACK_1    = 4;
constexpr int CEC_ADDR_AUDIO_SYSTEM  = 5;
constexpr int CEC_ADDR_TUNER_2       = 6;
constexpr int CEC_ADDR_PLAYBACK_2    = 8;
constexpr int CEC_ADDR_BROADCAST     = 15;

// --- CEC opcodes (representative subset, including ARC) ---
constexpr uint8_t CEC_OP_IMAGE_VIEW_ON            = 0x04;
constexpr uint8_t CEC_OP_TEXT_VIEW_ON             = 0x0D;
constexpr uint8_t CEC_OP_STANDBY                  = 0x36;
constexpr uint8_t CEC_OP_USER_CONTROL_PRESSED     = 0x44;
constexpr uint8_t CEC_OP_USER_CONTROL_RELEASED    = 0x45;
constexpr uint8_t CEC_OP_ACTIVE_SOURCE            = 0x82;
constexpr uint8_t CEC_OP_GIVE_PHYSICAL_ADDRESS    = 0x83;
constexpr uint8_t CEC_OP_REPORT_PHYSICAL_ADDRESS  = 0x84;
constexpr uint8_t CEC_OP_DEVICE_VENDOR_ID         = 0x87;
constexpr uint8_t CEC_OP_GIVE_DEVICE_VENDOR_ID    = 0x8C;
constexpr uint8_t CEC_OP_GIVE_DEVICE_POWER_STATUS = 0x8F;
constexpr uint8_t CEC_OP_REPORT_POWER_STATUS      = 0x90;
constexpr uint8_t CEC_OP_CEC_VERSION              = 0x9E;
constexpr uint8_t CEC_OP_GET_CEC_VERSION          = 0x9F;
// ARC (Audio Return Channel)
constexpr uint8_t CEC_OP_INITIATE_ARC             = 0xC0;
constexpr uint8_t CEC_OP_REPORT_ARC_INITIATED     = 0xC1;
constexpr uint8_t CEC_OP_REPORT_ARC_TERMINATED    = 0xC2;
constexpr uint8_t CEC_OP_REQUEST_ARC_INITIATION   = 0xC3;
constexpr uint8_t CEC_OP_REQUEST_ARC_TERMINATION  = 0xC4;
constexpr uint8_t CEC_OP_TERMINATE_ARC            = 0xC5;

// --- Power status operand values (for Report Power Status) ---
constexpr uint8_t CEC_POWER_ON            = 0x00;
constexpr uint8_t CEC_POWER_STANDBY       = 0x01;
constexpr uint8_t CEC_POWER_STANDBY_TO_ON = 0x02;
constexpr uint8_t CEC_POWER_ON_TO_STANDBY = 0x03;

constexpr int CEC_MAX_FRAME_BYTES = 16;   // header + opcode + up to 14 operands

// A CEC message (frame). A header-only frame (no opcode) is a "polling" message.
struct CecMessage {
    int initiator = 0;             // 4-bit source logical address
    int destination = 0;          // 4-bit destination logical address
    bool hasOpcode = false;
    uint8_t opcode = 0;
    std::vector<uint8_t> operands;
};

class CecCodec {
public:
    std::vector<uint8_t> serialize(const CecMessage& m) {
        std::vector<uint8_t> out;
        out.push_back(static_cast<uint8_t>(((m.initiator & 0x0F) << 4) | (m.destination & 0x0F)));
        if (m.hasOpcode) {
            out.push_back(m.opcode);
            out.insert(out.end(), m.operands.begin(), m.operands.end());
        }
        return out;
    }

    // Returns false for an empty frame or one exceeding the 16-byte CEC limit.
    bool parse(const uint8_t* data, size_t length, CecMessage& out) {
        if (length < 1 || length > CEC_MAX_FRAME_BYTES) return false;
        out.initiator   = (data[0] >> 4) & 0x0F;
        out.destination = data[0] & 0x0F;
        if (length >= 2) {
            out.hasOpcode = true;
            out.opcode = data[1];
            out.operands.assign(data + 2, data + length);
        } else {
            out.hasOpcode = false;
            out.opcode = 0;
            out.operands.clear();
        }
        return true;
    }
};

// --- High-level message builders ---
inline CecMessage cecPolling(int src, int dst) {
    CecMessage m; m.initiator = src; m.destination = dst; m.hasOpcode = false; return m;
}
inline CecMessage cecImageViewOn(int src, int dst) {
    CecMessage m; m.initiator = src; m.destination = dst; m.hasOpcode = true; m.opcode = CEC_OP_IMAGE_VIEW_ON; return m;
}
inline CecMessage cecStandby(int src, int dst) {
    CecMessage m; m.initiator = src; m.destination = dst; m.hasOpcode = true; m.opcode = CEC_OP_STANDBY; return m;
}
inline CecMessage cecActiveSource(int src, uint16_t physicalAddress) {
    CecMessage m; m.initiator = src; m.destination = CEC_ADDR_BROADCAST; m.hasOpcode = true;
    m.opcode = CEC_OP_ACTIVE_SOURCE;
    m.operands = { static_cast<uint8_t>((physicalAddress >> 8) & 0xFF),
                   static_cast<uint8_t>(physicalAddress & 0xFF) };
    return m;
}
inline CecMessage cecReportPowerStatus(int src, int dst, uint8_t status) {
    CecMessage m; m.initiator = src; m.destination = dst; m.hasOpcode = true;
    m.opcode = CEC_OP_REPORT_POWER_STATUS; m.operands = { status };
    return m;
}
inline CecMessage cecRequestArcInitiation(int src, int dst) {
    CecMessage m; m.initiator = src; m.destination = dst; m.hasOpcode = true;
    m.opcode = CEC_OP_REQUEST_ARC_INITIATION; return m;
}

// --- Human-readable names (for logging, like a CEC bus sniffer) ---
inline std::string cecAddressName(int addr) {
    switch (addr) {
        case CEC_ADDR_TV:           return "TV";
        case CEC_ADDR_RECORDING_1:  return "Recording 1";
        case CEC_ADDR_RECORDING_2:  return "Recording 2";
        case CEC_ADDR_TUNER_1:      return "Tuner 1";
        case CEC_ADDR_PLAYBACK_1:   return "Playback 1";
        case CEC_ADDR_AUDIO_SYSTEM: return "Audio System";
        case CEC_ADDR_TUNER_2:      return "Tuner 2";
        case CEC_ADDR_PLAYBACK_2:   return "Playback 2";
        case CEC_ADDR_BROADCAST:    return "Broadcast";
        default:                    return "Unknown";
    }
}
inline std::string cecOpcodeName(uint8_t op) {
    switch (op) {
        case CEC_OP_IMAGE_VIEW_ON:            return "Image View On";
        case CEC_OP_TEXT_VIEW_ON:             return "Text View On";
        case CEC_OP_STANDBY:                  return "Standby";
        case CEC_OP_USER_CONTROL_PRESSED:     return "User Control Pressed";
        case CEC_OP_USER_CONTROL_RELEASED:    return "User Control Released";
        case CEC_OP_ACTIVE_SOURCE:            return "Active Source";
        case CEC_OP_GIVE_PHYSICAL_ADDRESS:    return "Give Physical Address";
        case CEC_OP_REPORT_PHYSICAL_ADDRESS:  return "Report Physical Address";
        case CEC_OP_DEVICE_VENDOR_ID:         return "Device Vendor ID";
        case CEC_OP_GIVE_DEVICE_VENDOR_ID:    return "Give Device Vendor ID";
        case CEC_OP_GIVE_DEVICE_POWER_STATUS: return "Give Device Power Status";
        case CEC_OP_REPORT_POWER_STATUS:      return "Report Power Status";
        case CEC_OP_CEC_VERSION:              return "CEC Version";
        case CEC_OP_GET_CEC_VERSION:          return "Get CEC Version";
        case CEC_OP_INITIATE_ARC:             return "Initiate ARC";
        case CEC_OP_REPORT_ARC_INITIATED:     return "Report ARC Initiated";
        case CEC_OP_REPORT_ARC_TERMINATED:    return "Report ARC Terminated";
        case CEC_OP_REQUEST_ARC_INITIATION:   return "Request ARC Initiation";
        case CEC_OP_REQUEST_ARC_TERMINATION:  return "Request ARC Termination";
        case CEC_OP_TERMINATE_ARC:            return "Terminate ARC";
        default:                              return "Unknown opcode";
    }
}

#endif // CECMESSAGE_H
