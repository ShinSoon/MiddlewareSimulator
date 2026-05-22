#ifndef BITWRITER_H
#define BITWRITER_H

#include <cstdint>
#include <vector>
#include <string>

// Accumulates bits MSB-first into a byte buffer. Used to pack MPEG-TS / PSI
// fields that are not byte-aligned (e.g. the 13-bit PID, the 12-bit
// section_length). MSB-first is the bit order the MPEG/DVB standards use.
class BitWriter {
public:
    // Append the low `numBits` bits of `value`, most-significant bit first.
    void writeBits(uint32_t value, int numBits) {
        for (int i = numBits - 1; i >= 0; --i) {
            uint8_t bit = (value >> i) & 0x1u;
            if (bitsFilled_ == 0) buffer_.push_back(0);          // start a new byte
            buffer_.back() |= static_cast<uint8_t>(bit << (7 - bitsFilled_));
            bitsFilled_ = (bitsFilled_ + 1) % 8;
        }
    }

    void writeByte(uint8_t b) { writeBits(b, 8); }

    void writeBytes(const std::string& bytes) {
        for (unsigned char c : bytes) writeByte(c);
    }

    bool isByteAligned() const { return bitsFilled_ == 0; }
    const std::vector<uint8_t>& data() const { return buffer_; }
    size_t sizeBytes() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
    int bitsFilled_ = 0;   // number of bits used in the current (last) byte, 0..7
};

#endif // BITWRITER_H
