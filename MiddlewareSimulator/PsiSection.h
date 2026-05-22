#ifndef PSISECTION_H
#define PSISECTION_H

#include "BitWriter.h"
#include "BitReader.h"
#include "Crc32.h"
#include <cstdint>
#include <vector>

// A generic PSI/SI section (long form, section_syntax_indicator = 1).
// For this simulator each section fits in a single packet (real sections can
// span packets - noted as a known limitation).
struct PsiSection {
    uint8_t  tableId = 0;
    uint16_t tableIdExtension = 0;     // transport_stream_id / program_number / service_id
    uint8_t  versionNumber = 0;
    uint8_t  sectionNumber = 0;
    uint8_t  lastSectionNumber = 0;
    std::vector<uint8_t> body;         // table-specific bytes between header and CRC
};

class PsiSectionWriter {
public:
    std::vector<uint8_t> serialize(const PsiSection& s) {
        BitWriter w;
        w.writeByte(s.tableId);
        w.writeBits(1, 1);                       // section_syntax_indicator
        w.writeBits(0, 1);                       // '0'
        w.writeBits(3, 2);                       // reserved
        // section_length counts everything after this field, incl. the 4-byte CRC:
        // table_id_extension(2) + flags(1) + section_number(1) + last(1) + body + CRC(4)
        int sectionLength = static_cast<int>(s.body.size()) + 9;
        w.writeBits(sectionLength, 12);
        w.writeBits(s.tableIdExtension, 16);
        w.writeBits(3, 2);                       // reserved
        w.writeBits(s.versionNumber & 0x1F, 5);
        w.writeBits(1, 1);                       // current_next_indicator
        w.writeByte(s.sectionNumber);
        w.writeByte(s.lastSectionNumber);
        for (uint8_t b : s.body) w.writeByte(b);

        std::vector<uint8_t> bytes = w.data();
        uint32_t crc = crc32Mpeg(bytes.data(), bytes.size());
        bytes.push_back(static_cast<uint8_t>((crc >> 24) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((crc >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(crc & 0xFF));
        return bytes;
    }
};

class PsiSectionReader {
public:
    // Parse one section. Returns false on truncation or CRC mismatch.
    bool parse(const uint8_t* data, size_t length, PsiSection& out) {
        if (length < 3) return false;
        BitReader r(data, length);
        out.tableId = r.readByte();
        r.readBits(1);                           // section_syntax_indicator
        r.readBits(1);                           // '0'
        r.readBits(2);                           // reserved
        int sectionLength = static_cast<int>(r.readBits(12));
        size_t totalSize = 3 + static_cast<size_t>(sectionLength);
        if (sectionLength < 9 || totalSize > length) return false;

        out.tableIdExtension = static_cast<uint16_t>(r.readBits(16));
        r.readBits(2);                           // reserved
        out.versionNumber = static_cast<uint8_t>(r.readBits(5));
        r.readBits(1);                           // current_next_indicator
        out.sectionNumber = r.readByte();
        out.lastSectionNumber = r.readByte();

        int bodyLen = sectionLength - 9;
        out.body.clear();
        out.body.reserve(bodyLen);
        for (int i = 0; i < bodyLen; ++i) out.body.push_back(r.readByte());

        uint32_t crcRead = (static_cast<uint32_t>(r.readByte()) << 24)
                         | (static_cast<uint32_t>(r.readByte()) << 16)
                         | (static_cast<uint32_t>(r.readByte()) << 8)
                         |  static_cast<uint32_t>(r.readByte());
        uint32_t crcCalc = crc32Mpeg(data, totalSize - 4);
        return crcRead == crcCalc;
    }
};

#endif // PSISECTION_H
