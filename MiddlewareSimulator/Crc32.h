#ifndef CRC32_H
#define CRC32_H

#include <cstdint>
#include <cstddef>
#include <array>

// CRC-32/MPEG-2: polynomial 0x04C11DB7, init 0xFFFFFFFF, no input/output
// reflection, no final XOR. This is the CRC every MPEG/DVB PSI section carries.
// The 256-entry table is built once on first use (thread-safe Meyers singleton).
inline const std::array<uint32_t, 256>& crc32Table() {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i << 24;
            for (int j = 0; j < 8; ++j)
                crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04C11DB7u : (crc << 1);
            t[i] = crc;
        }
        return t;
    }();
    return table;
}

inline uint32_t crc32Mpeg(const uint8_t* data, size_t length) {
    const auto& table = crc32Table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i)
        crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xFFu];
    return crc;
}

#endif // CRC32_H
