#ifndef BITREADER_H
#define BITREADER_H

#include <cstdint>
#include <cstddef>
#include <string>

// Reads bits MSB-first from a byte buffer. The exact mirror of BitWriter.
// Reading past the end yields zero bits (so a truncated buffer fails cleanly
// rather than reading out of bounds).
class BitReader {
public:
    BitReader(const uint8_t* data, size_t lengthBytes)
        : data_(data), lengthBytes_(lengthBytes) {}

    // Read `numBits` bits MSB-first; result is right-aligned in the low bits.
    uint32_t readBits(int numBits) {
        uint32_t value = 0;
        for (int i = 0; i < numBits; ++i) {
            uint8_t byte = (bytePos_ < lengthBytes_) ? data_[bytePos_] : 0;
            uint8_t bit = (byte >> (7 - bitPos_)) & 0x1u;
            value = (value << 1) | bit;
            if (++bitPos_ == 8) { bitPos_ = 0; ++bytePos_; }
        }
        return value;
    }

    uint8_t readByte() { return static_cast<uint8_t>(readBits(8)); }

    std::string readBytesAsString(int numBytes) {
        std::string s;
        s.reserve(numBytes);
        for (int i = 0; i < numBytes; ++i) s.push_back(static_cast<char>(readByte()));
        return s;
    }

    size_t bytePosition() const { return bytePos_; }
    bool exhausted() const { return bytePos_ >= lengthBytes_; }

private:
    const uint8_t* data_;
    size_t lengthBytes_;
    size_t bytePos_ = 0;
    int bitPos_ = 0;   // bit offset within the current byte, 0..7, MSB-first
};

#endif // BITREADER_H
