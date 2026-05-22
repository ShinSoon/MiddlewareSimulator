#define _CRT_SECURE_NO_WARNINGS
#include "gtest/gtest.h"
#include "Parser.h"
#include "BitWriter.h"
#include "BitReader.h"
#include "TsPacket.h"
#include "TsPacketWriter.h"
#include "TsPacketReader.h"
#include "Crc32.h"
#include "PsiSection.h"
#include "PsiTables.h"
#include "TsMuxer.h"
#include "TsDemuxer.h"
#include "MjdTime.h"
#include "EitTable.h"
#include "CecMessage.h"
#include <fstream>
#include <map>
#include <string>
#include <vector>

// --- Helper: write a temporary data file and return its path ---
static std::string writeTempFile(const std::string& name, const std::string& content) {
    std::ofstream f(name);
    f << content;
    f.close();
    return name;
}

// ============================================================================
// Time parsing tests (validates Task A � timezone fix)
// ============================================================================

TEST(ParseDateTimeString, ValidUtcString_ReturnsCorrectEpoch) {
    Parser p;
    // 2025-04-16 14:00:00 UTC = 1744812000 seconds since epoch = 1744812000000 ms
    EXPECT_EQ(p.parseDateTimeString("2025-04-16 14:00:00"), 1744812000000LL);
}

TEST(ParseDateTimeString, EpochString_ReturnsZero) {
    Parser p;
    EXPECT_EQ(p.parseDateTimeString("1970-01-01 00:00:00"), 0LL);
}

TEST(ParseDateTimeString, InvalidFormat_ReturnsZero) {
    Parser p;
    EXPECT_EQ(p.parseDateTimeString("not a date"), 0LL);
}

// ============================================================================
// File parsing tests
// ============================================================================

TEST(ParseDataFromFile, ValidChannel_IsStored) {
    auto path = writeTempFile("test_valid_ch.txt", "CH|1|BBC One");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto channels = p.getChannels();
    ASSERT_EQ(channels.size(), 1u);
    EXPECT_EQ(channels[0].channelId, 1);
    EXPECT_EQ(channels[0].channelName, "BBC One");
}

TEST(ParseDataFromFile, ValidChannelAndProgram_BothStored) {
    auto path = writeTempFile("test_valid_pg.txt",
        "CH|1|BBC One\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|News|Headlines");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto programs = p.getProgramsForChannel(1);
    ASSERT_EQ(programs.size(), 1u);
    EXPECT_EQ(programs[0].programId, 101);
    EXPECT_EQ(programs[0].programName, "News");
}

TEST(ParseDataFromFile, InvalidChannelId_ChannelRejected) {
    auto path = writeTempFile("test_invalid_id.txt", "CH|notanumber|Bad");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    EXPECT_TRUE(p.getChannels().empty());
}

TEST(ParseDataFromFile, IncompleteRecord_Rejected) {
    auto path = writeTempFile("test_incomplete.txt", "CH|5");  // missing name
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    EXPECT_TRUE(p.getChannels().empty());
}

// Validates Task B � orphan rejection
TEST(ParseDataFromFile, OrphanProgram_Rejected) {
    auto path = writeTempFile("test_orphan.txt",
        "PG|99|901|2025-04-16 14:00:00|2025-04-16 15:00:00|Ghost|No channel");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    EXPECT_TRUE(p.getProgramsForChannel(99).empty());
}

// ============================================================================
// Enriched data-model tests (Phase 2.0) - optional trailing fields
// ============================================================================

TEST(ParseDataFromFile, ChannelOptionalFields_Parsed) {
    auto path = writeTempFile("test_ch_optional.txt", "CH|1|BBC One|BBC|1|101");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto channels = p.getChannels();
    ASSERT_EQ(channels.size(), 1u);
    EXPECT_EQ(channels[0].providerName, "BBC");
    EXPECT_EQ(channels[0].serviceType, 1);
    EXPECT_EQ(channels[0].logicalChannelNumber, 101);
}

TEST(ParseDataFromFile, ChannelWithoutOptionalFields_UsesDefaults) {
    auto path = writeTempFile("test_ch_defaults.txt", "CH|1|BBC One");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto channels = p.getChannels();
    ASSERT_EQ(channels.size(), 1u);
    EXPECT_TRUE(channels[0].providerName.empty());
    EXPECT_EQ(channels[0].serviceType, 0x01);   // default
    EXPECT_EQ(channels[0].logicalChannelNumber, 0);
}

TEST(ParseDataFromFile, ProgramOptionalFields_Parsed) {
    auto path = writeTempFile("test_pg_optional.txt",
        "CH|1|BBC One\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|News|Headlines|News|12");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto programs = p.getProgramsForChannel(1);
    ASSERT_EQ(programs.size(), 1u);
    EXPECT_EQ(programs[0].genre, "News");
    EXPECT_EQ(programs[0].parentalAgeRating, 12);
}

TEST(ParseDataFromFile, ProgramWithoutOptionalFields_UsesDefaults) {
    auto path = writeTempFile("test_pg_defaults.txt",
        "CH|1|BBC One\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|News|Headlines");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    auto programs = p.getProgramsForChannel(1);
    ASSERT_EQ(programs.size(), 1u);
    EXPECT_TRUE(programs[0].genre.empty());
    EXPECT_EQ(programs[0].parentalAgeRating, 0);
}

// ============================================================================
// parseDataFromString tests (Android/JNI prerequisite)
// ============================================================================

TEST(ParseDataFromString, ParsesInMemoryData) {
    Parser p;
    ASSERT_TRUE(p.parseDataFromString(
        "CH|1|BBC One|BBC|1|101\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|News|Headlines|News|12"));
    auto channels = p.getChannels();
    ASSERT_EQ(channels.size(), 1u);
    EXPECT_EQ(channels[0].channelName, "BBC One");
    auto programs = p.getProgramsForChannel(1);
    ASSERT_EQ(programs.size(), 1u);
    EXPECT_EQ(programs[0].programName, "News");
    EXPECT_EQ(programs[0].genre, "News");
}

// ============================================================================
// EPG query tests
// ============================================================================

TEST(GetProgramsOnNow, DuringProgram_ReturnsIt) {
    auto path = writeTempFile("test_onnow.txt",
        "CH|1|TestCh\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|Live|Now");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    long long midProgram = p.parseDateTimeString("2025-04-16 14:30:00");
    auto onNow = p.getProgramsOnNow(1, midProgram);
    ASSERT_EQ(onNow.size(), 1u);
    EXPECT_EQ(onNow[0].programId, 101);
}

TEST(GetProgramsOnNow, OutsideProgram_ReturnsEmpty) {
    auto path = writeTempFile("test_onnow_out.txt",
        "CH|1|TestCh\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|Live|Now");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    long long afterProgram = p.parseDateTimeString("2025-04-16 16:00:00");
    EXPECT_TRUE(p.getProgramsOnNow(1, afterProgram).empty());
}

TEST(GetProgramsForTimeRange, OverlappingProgram_Returned) {
    auto path = writeTempFile("test_range.txt",
        "CH|1|TestCh\n"
        "PG|1|101|2025-04-16 14:00:00|2025-04-16 15:00:00|InRange|Desc");
    Parser p;
    ASSERT_TRUE(p.parseDataFromFile(path));
    long long start = p.parseDateTimeString("2025-04-16 14:30:00");
    long long end = p.parseDateTimeString("2025-04-16 15:30:00");
    auto results = p.getProgramsForTimeRange(1, start, end);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].programId, 101);
}

// ============================================================================
// Bit-level I/O tests (Phase 2.1) - foundation for binary TS/PSI parsing
// ============================================================================

TEST(BitIO, ByteRoundTrip) {
    BitWriter w;
    w.writeByte(0x47);                       // the TS sync byte
    ASSERT_EQ(w.sizeBytes(), 1u);
    EXPECT_EQ(w.data()[0], 0x47);

    BitReader r(w.data().data(), w.sizeBytes());
    EXPECT_EQ(r.readByte(), 0x47);
}

TEST(BitIO, SubByteFieldsPackMSBFirst) {
    // Pack a 3-bit value (0b101 = 5) then a 5-bit value (0b01010 = 10).
    BitWriter w;
    w.writeBits(5, 3);
    w.writeBits(10, 5);
    ASSERT_EQ(w.sizeBytes(), 1u);
    // 101 01010 = 1010 1010 = 0xAA
    EXPECT_EQ(w.data()[0], 0xAA);

    BitReader r(w.data().data(), w.sizeBytes());
    EXPECT_EQ(r.readBits(3), 5u);
    EXPECT_EQ(r.readBits(5), 10u);
}

TEST(BitIO, ThirteenBitPidRoundTrip) {
    // Mirrors the TS header: 3 flag bits, then the 13-bit PID.
    BitWriter w;
    w.writeBits(0, 3);          // TEI / PUSI / priority cleared
    w.writeBits(0x0100, 13);    // PID 0x0100
    ASSERT_EQ(w.sizeBytes(), 2u);
    EXPECT_EQ(w.data()[0], 0x01);
    EXPECT_EQ(w.data()[1], 0x00);

    BitReader r(w.data().data(), w.sizeBytes());
    EXPECT_EQ(r.readBits(3), 0u);
    EXPECT_EQ(r.readBits(13), 0x0100u);
}

TEST(BitIO, MultiByteStringRoundTrip) {
    BitWriter w;
    w.writeBytes("BBC");
    ASSERT_EQ(w.sizeBytes(), 3u);
    BitReader r(w.data().data(), w.sizeBytes());
    EXPECT_EQ(r.readBytesAsString(3), "BBC");
}

// ============================================================================
// TS packet tests (Phase 2.1b) - 188-byte packet serialize/parse + resync
// ============================================================================

TEST(TsPacket, SerializeProducesValidPacket) {
    TsPacket pkt;
    pkt.payloadUnitStart = true;
    pkt.pid = PID_PAT;                 // 0x0000
    pkt.continuityCounter = 5;
    pkt.payload = { 0xDE, 0xAD, 0xBE, 0xEF };

    TsPacketWriter writer;
    auto bytes = writer.serialize(pkt);

    ASSERT_EQ(bytes.size(), static_cast<size_t>(TS_PACKET_SIZE));  // exactly 188
    EXPECT_EQ(bytes[0], TS_SYNC_BYTE);                             // starts with 0x47
    EXPECT_EQ(bytes[4], 0xDE);                                     // payload after 4-byte header
    EXPECT_EQ(bytes[5], 0xAD);
    EXPECT_EQ(bytes[8], TS_STUFFING_BYTE);                        // unused payload is 0xFF
}

TEST(TsPacket, HeaderRoundTrip) {
    TsPacket in;
    in.payloadUnitStart = true;
    in.pid = 0x0100;
    in.continuityCounter = 9;
    in.payload = { 0x01, 0x02, 0x03 };

    TsPacketWriter writer;
    auto bytes = writer.serialize(in);

    TsPacketReader reader(bytes.data(), bytes.size());
    TsPacket out;
    ASSERT_TRUE(reader.readNext(out));
    EXPECT_TRUE(out.payloadUnitStart);
    EXPECT_EQ(out.pid, 0x0100);
    EXPECT_EQ(out.continuityCounter, 9);
    ASSERT_GE(out.payload.size(), 3u);
    EXPECT_EQ(out.payload[0], 0x01);
    EXPECT_EQ(out.payload[1], 0x02);
    EXPECT_EQ(out.payload[2], 0x03);
}

TEST(TsPacket, ReaderResyncsOnGarbagePrefix) {
    TsPacket in;
    in.pid = 0x0042;
    in.continuityCounter = 1;
    in.payload = { 0xAB };

    TsPacketWriter writer;
    auto good = writer.serialize(in);

    // Prepend junk bytes (none equal to 0x47) before a valid packet.
    std::vector<uint8_t> stream = { 0x00, 0x11, 0x22, 0x33 };
    stream.insert(stream.end(), good.begin(), good.end());

    TsPacketReader reader(stream.data(), stream.size());
    TsPacket out;
    ASSERT_TRUE(reader.readNext(out));   // skips junk, locks on 0x47
    EXPECT_EQ(out.pid, 0x0042);
    EXPECT_EQ(out.payload[0], 0xAB);
}

TEST(TsPacket, MultiplePacketsReadInSequence) {
    TsPacketWriter writer;
    TsPacket a; a.pid = 0x0000; a.continuityCounter = 0; a.payload = { 0x0A };
    TsPacket b; b.pid = 0x0100; b.continuityCounter = 1; b.payload = { 0x0B };

    std::vector<uint8_t> stream;
    for (const auto& pkt : { a, b }) {
        auto bytes = writer.serialize(pkt);
        stream.insert(stream.end(), bytes.begin(), bytes.end());
    }

    TsPacketReader reader(stream.data(), stream.size());
    TsPacket out;
    ASSERT_TRUE(reader.readNext(out));
    EXPECT_EQ(out.pid, 0x0000);
    EXPECT_EQ(out.payload[0], 0x0A);
    ASSERT_TRUE(reader.readNext(out));
    EXPECT_EQ(out.pid, 0x0100);
    EXPECT_EQ(out.payload[0], 0x0B);
    EXPECT_FALSE(reader.readNext(out));   // no more complete packets
}

// ============================================================================
// CRC-32 / PSI section tests (Phase 2.2)
// ============================================================================

TEST(Crc32, KnownMpeg2CheckValue) {
    // Published check value for CRC-32/MPEG-2 over ASCII "123456789".
    const char* s = "123456789";
    uint32_t crc = crc32Mpeg(reinterpret_cast<const uint8_t*>(s), 9);
    EXPECT_EQ(crc, 0x0376E6E7u);
}

TEST(Crc32, DifferentDataYieldsDifferentCrc) {
    uint8_t a[] = { 0x01, 0x02, 0x03 };
    uint8_t b[] = { 0x01, 0x02, 0x04 };
    EXPECT_NE(crc32Mpeg(a, 3), crc32Mpeg(b, 3));
}

TEST(PsiSection, RoundTripPreservesHeaderAndBody) {
    PsiSection in;
    in.tableId = 0x00;                 // PAT table_id
    in.tableIdExtension = 0x0001;      // transport_stream_id
    in.versionNumber = 3;
    in.sectionNumber = 0;
    in.lastSectionNumber = 0;
    in.body = { 0xDE, 0xAD, 0xBE, 0xEF };

    PsiSectionWriter writer;
    auto bytes = writer.serialize(in);

    PsiSection out;
    PsiSectionReader reader;
    ASSERT_TRUE(reader.parse(bytes.data(), bytes.size(), out));
    EXPECT_EQ(out.tableId, 0x00);
    EXPECT_EQ(out.tableIdExtension, 0x0001);
    EXPECT_EQ(out.versionNumber, 3);
    EXPECT_EQ(out.sectionNumber, 0);
    EXPECT_EQ(out.lastSectionNumber, 0);
    ASSERT_EQ(out.body.size(), 4u);
    EXPECT_EQ(out.body[0], 0xDE);
    EXPECT_EQ(out.body[3], 0xEF);
}

TEST(PsiSection, CorruptedByteFailsCrc) {
    PsiSection in;
    in.tableId = 0x42;
    in.tableIdExtension = 0x1234;
    in.body = { 0x01, 0x02, 0x03 };

    PsiSectionWriter writer;
    auto bytes = writer.serialize(in);
    bytes[5] ^= 0xFF;                  // flip a byte inside the section

    PsiSection out;
    PsiSectionReader reader;
    EXPECT_FALSE(reader.parse(bytes.data(), bytes.size(), out));  // CRC must reject
}

TEST(PsiSection, SectionLengthMatchesSpec) {
    PsiSection in;
    in.tableId = 0x00;
    in.body = { 0xAA, 0xBB };          // 2-byte body

    PsiSectionWriter writer;
    auto bytes = writer.serialize(in);

    // total = 3 (table_id + flags/length bytes) + section_length
    // section_length = body(2) + 9 = 11; total = 14
    ASSERT_EQ(bytes.size(), 14u);
    int sectionLength = ((bytes[1] & 0x0F) << 8) | bytes[2];
    EXPECT_EQ(sectionLength, 11);
}

// ============================================================================
// PAT/PMT mux + demux tests (Phase 2.3) - full TS round-trip
// ============================================================================

TEST(TsMuxDemux, PatPmtRoundTrip) {
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 1; a.channelName = "BBC One";
    a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    ChannelInfo b; b.channelId = 2; b.channelName = "ITV";
    b.pmtPid = 0x0200; b.videoPid = 0x0201; b.audioPid = 0x0202;
    in.push_back(a); in.push_back(b);

    TsMuxer muxer;
    auto ts = muxer.mux(in);
    EXPECT_EQ(ts.size() % TS_PACKET_SIZE, 0u);   // whole packets only

    TsDemuxer demuxer;
    auto out = demuxer.demux(ts);
    ASSERT_EQ(out.size(), 2u);

    EXPECT_EQ(out[0].channelId, 1);
    EXPECT_EQ(out[0].pmtPid, 0x0100);
    EXPECT_EQ(out[0].videoPid, 0x0101);
    EXPECT_EQ(out[0].audioPid, 0x0102);

    EXPECT_EQ(out[1].channelId, 2);
    EXPECT_EQ(out[1].pmtPid, 0x0200);
    EXPECT_EQ(out[1].videoPid, 0x0201);
    EXPECT_EQ(out[1].audioPid, 0x0202);
}

TEST(TsMuxDemux, AutoAssignsPidsWhenZero) {
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 1; a.channelName = "Ch1";   // PIDs left at 0
    in.push_back(a);

    TsMuxer muxer;
    auto ts = muxer.mux(in);
    TsDemuxer demuxer;
    auto out = demuxer.demux(ts);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_NE(out[0].pmtPid, 0);
    EXPECT_NE(out[0].videoPid, 0);
    EXPECT_NE(out[0].audioPid, 0);
}

TEST(TsMuxDemux, StreamStartsWithPatOnPid0) {
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 7; a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    in.push_back(a);

    TsMuxer muxer;
    auto ts = muxer.mux(in);

    TsPacketReader reader(ts.data(), ts.size());
    TsPacket pkt;
    ASSERT_TRUE(reader.readNext(pkt));
    EXPECT_EQ(pkt.pid, PID_PAT);
    EXPECT_TRUE(pkt.payloadUnitStart);
}

TEST(TsMuxDemux, KnownPatByteLayout) {
    // One program (program_number=1, pmtPid=0x0100) -> 4-byte PAT body:
    //   program_number 0x0001          -> 00 01
    //   reserved(111) + pmtPid(0x0100) -> 1110 0001 0000 0000 -> E1 00
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 1; a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    in.push_back(a);

    TsMuxer muxer;
    auto ts = muxer.mux(in);

    TsPacketReader reader(ts.data(), ts.size());
    TsPacket pkt;
    ASSERT_TRUE(reader.readNext(pkt));            // PAT packet
    // payload[0]=pointer_field, then 8-byte section header, so body starts at [9].
    ASSERT_GE(pkt.payload.size(), 13u);
    EXPECT_EQ(pkt.payload[9],  0x00);
    EXPECT_EQ(pkt.payload[10], 0x01);
    EXPECT_EQ(pkt.payload[11], 0xE1);
    EXPECT_EQ(pkt.payload[12], 0x00);
}

// ============================================================================
// DVB time encoding tests (Phase 2.4a) - MJD + BCD
// ============================================================================

TEST(MjdTime, KnownDateToMjd) {
    EXPECT_EQ(toMjd(2025, 4, 16), 60781);
    EXPECT_EQ(toMjd(1993, 10, 13), 49273);   // DVB spec Annex C worked example
}

TEST(MjdTime, MjdToDateRoundTrip) {
    int y = 0, m = 0, d = 0;
    fromMjd(60781, y, m, d);
    EXPECT_EQ(y, 2025);
    EXPECT_EQ(m, 4);
    EXPECT_EQ(d, 16);
}

TEST(MjdTime, BcdRoundTrip) {
    EXPECT_EQ(toBcd(45), 0x45);
    EXPECT_EQ(fromBcd(0x30), 30);
    EXPECT_EQ(fromBcd(toBcd(59)), 59);
}

TEST(MjdTime, StartTimeRoundTrip) {
    Parser p;                                 // reuse the UTC string -> epoch helper
    long long ms = p.parseDateTimeString("2025-04-16 14:30:45");
    auto bytes = encodeStartTime(ms);
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(decodeStartTime(bytes.data()), ms);
}

TEST(MjdTime, DurationRoundTrip) {
    auto bytes = encodeDuration(3600 + 30 * 60 + 15);   // 01:30:15
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x01);
    EXPECT_EQ(bytes[1], 0x30);
    EXPECT_EQ(bytes[2], 0x15);
    EXPECT_EQ(decodeDuration(bytes.data()), 3600 + 30 * 60 + 15);
}

// ============================================================================
// SDT tests (Phase 2.4b) - service names survive the round-trip
// ============================================================================

TEST(Sdt, ServiceDescriptorHasCorrectTag) {
    ChannelInfo ch; ch.channelId = 1; ch.channelName = "X"; ch.providerName = "Y"; ch.serviceType = 0x01;
    auto desc = buildServiceDescriptor(ch);
    ASSERT_GE(desc.size(), 2u);
    EXPECT_EQ(desc[0], 0x48);                                  // service_descriptor tag
    EXPECT_EQ(static_cast<size_t>(desc[1]), desc.size() - 2);  // length excludes tag+len
}

TEST(Sdt, StreamContainsSdtOnPid0x11) {
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 1; a.channelName = "Ch"; a.providerName = "P";
    a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    in.push_back(a);

    TsMuxer muxer;
    auto ts = muxer.mux(in);

    TsPacketReader reader(ts.data(), ts.size());
    TsPacket pkt;
    bool foundSdt = false;
    while (reader.readNext(pkt)) {
        if (pkt.pid == PID_SDT) { foundSdt = true; break; }
    }
    EXPECT_TRUE(foundSdt);
}

TEST(Sdt, ServiceNamesRoundTrip) {
    std::vector<ChannelInfo> in;
    ChannelInfo a; a.channelId = 1; a.channelName = "BBC One"; a.providerName = "BBC"; a.serviceType = 0x01;
    a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    ChannelInfo b; b.channelId = 2; b.channelName = "ITV"; b.providerName = "ITV plc"; b.serviceType = 0x01;
    b.pmtPid = 0x0200; b.videoPid = 0x0201; b.audioPid = 0x0202;
    in.push_back(a); in.push_back(b);

    TsMuxer muxer;
    auto ts = muxer.mux(in);
    TsDemuxer demuxer;
    auto out = demuxer.demux(ts);

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].channelName, "BBC One");
    EXPECT_EQ(out[0].providerName, "BBC");
    EXPECT_EQ(out[0].serviceType, 0x01);
    EXPECT_EQ(out[1].channelName, "ITV");
    EXPECT_EQ(out[1].providerName, "ITV plc");
}

// ============================================================================
// EIT tests (Phase 2.4c) - the EPG survives the round-trip
// ============================================================================

TEST(Eit, ProgramRoundTrip) {
    std::vector<ChannelInfo> chans;
    ChannelInfo a; a.channelId = 1; a.channelName = "BBC One"; a.providerName = "BBC";
    a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    chans.push_back(a);

    Parser p;
    std::map<int, std::vector<ProgramInfo>> progs;
    ProgramInfo pg;
    pg.programId = 101;
    pg.programName = "Afternoon News";
    pg.description = "Latest headlines";
    pg.startTimeMillis = p.parseDateTimeString("2025-04-16 14:00:00");
    pg.endTimeMillis = p.parseDateTimeString("2025-04-16 15:30:00");
    pg.genre = "News";
    pg.parentalAgeRating = 12;
    progs[1].push_back(pg);

    TsMuxer muxer;
    auto ts = muxer.mux(chans, progs);

    TsDemuxer demuxer;
    std::map<int, std::vector<ProgramInfo>> recovered;
    demuxer.demux(ts, &recovered);

    ASSERT_EQ(recovered.count(1), 1u);
    ASSERT_EQ(recovered[1].size(), 1u);
    const ProgramInfo& rp = recovered[1][0];
    EXPECT_EQ(rp.programId, 101);
    EXPECT_EQ(rp.programName, "Afternoon News");
    EXPECT_EQ(rp.description, "Latest headlines");
    EXPECT_EQ(rp.startTimeMillis, pg.startTimeMillis);
    EXPECT_EQ(rp.endTimeMillis, pg.endTimeMillis);
    EXPECT_EQ(rp.genre, "News");
    EXPECT_EQ(rp.parentalAgeRating, 12);
}

TEST(Eit, MultipleServicesAndEvents) {
    std::vector<ChannelInfo> chans;
    ChannelInfo a; a.channelId = 1; a.channelName = "A"; a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    ChannelInfo b; b.channelId = 2; b.channelName = "B"; b.pmtPid = 0x0200; b.videoPid = 0x0201; b.audioPid = 0x0202;
    chans.push_back(a); chans.push_back(b);

    Parser p;
    std::map<int, std::vector<ProgramInfo>> progs;
    ProgramInfo p1; p1.programId = 101; p1.programName = "News"; p1.description = "d";
    p1.startTimeMillis = p.parseDateTimeString("2025-04-16 09:00:00");
    p1.endTimeMillis = p.parseDateTimeString("2025-04-16 10:00:00"); p1.genre = "News";
    ProgramInfo p2; p2.programId = 102; p2.programName = "Movie"; p2.description = "d";
    p2.startTimeMillis = p.parseDateTimeString("2025-04-16 10:00:00");
    p2.endTimeMillis = p.parseDateTimeString("2025-04-16 12:00:00"); p2.genre = "Movie";
    ProgramInfo p3; p3.programId = 201; p3.programName = "Sport"; p3.description = "d";
    p3.startTimeMillis = p.parseDateTimeString("2025-04-16 11:00:00");
    p3.endTimeMillis = p.parseDateTimeString("2025-04-16 13:00:00"); p3.genre = "Sports";
    progs[1].push_back(p1); progs[1].push_back(p2); progs[2].push_back(p3);

    TsMuxer muxer;
    auto ts = muxer.mux(chans, progs);
    TsDemuxer demuxer;
    std::map<int, std::vector<ProgramInfo>> recovered;
    demuxer.demux(ts, &recovered);

    ASSERT_EQ(recovered[1].size(), 2u);
    ASSERT_EQ(recovered[2].size(), 1u);
    EXPECT_EQ(recovered[1][0].programName, "News");
    EXPECT_EQ(recovered[1][1].genre, "Movie");
    EXPECT_EQ(recovered[2][0].programName, "Sport");
    EXPECT_EQ(recovered[2][0].genre, "Sports");
}

TEST(Eit, StreamContainsEitOnPid0x12) {
    std::vector<ChannelInfo> chans;
    ChannelInfo a; a.channelId = 1; a.channelName = "A"; a.pmtPid = 0x0100; a.videoPid = 0x0101; a.audioPid = 0x0102;
    chans.push_back(a);

    Parser p;
    std::map<int, std::vector<ProgramInfo>> progs;
    ProgramInfo pg; pg.programId = 101; pg.programName = "X"; pg.description = "Y";
    pg.startTimeMillis = p.parseDateTimeString("2025-04-16 09:00:00");
    pg.endTimeMillis = p.parseDateTimeString("2025-04-16 10:00:00");
    progs[1].push_back(pg);

    TsMuxer muxer;
    auto ts = muxer.mux(chans, progs);

    TsPacketReader reader(ts.data(), ts.size());
    TsPacket pkt;
    bool found = false;
    while (reader.readNext(pkt)) {
        if (pkt.pid == PID_EIT) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

// ============================================================================
// HDMI-CEC tests (Priority 3) - message codec + ARC
// ============================================================================

TEST(Cec, HeaderByteLayout) {
    // Playback 1 (4) -> TV (0), Image View On
    auto bytes = CecCodec().serialize(cecImageViewOn(CEC_ADDR_PLAYBACK_1, CEC_ADDR_TV));
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0x40);                 // initiator 4 | destination 0
    EXPECT_EQ(bytes[1], CEC_OP_IMAGE_VIEW_ON); // 0x04
}

TEST(Cec, MessageRoundTrip) {
    CecCodec codec;
    auto in = cecReportPowerStatus(CEC_ADDR_TV, CEC_ADDR_PLAYBACK_1, CEC_POWER_ON);
    auto bytes = codec.serialize(in);

    CecMessage out;
    ASSERT_TRUE(codec.parse(bytes.data(), bytes.size(), out));
    EXPECT_EQ(out.initiator, CEC_ADDR_TV);
    EXPECT_EQ(out.destination, CEC_ADDR_PLAYBACK_1);
    EXPECT_TRUE(out.hasOpcode);
    EXPECT_EQ(out.opcode, CEC_OP_REPORT_POWER_STATUS);
    ASSERT_EQ(out.operands.size(), 1u);
    EXPECT_EQ(out.operands[0], CEC_POWER_ON);
}

TEST(Cec, PollingMessageHasNoOpcode) {
    CecCodec codec;
    auto bytes = codec.serialize(cecPolling(CEC_ADDR_PLAYBACK_1, CEC_ADDR_TV));
    ASSERT_EQ(bytes.size(), 1u);               // header only
    CecMessage out;
    ASSERT_TRUE(codec.parse(bytes.data(), bytes.size(), out));
    EXPECT_FALSE(out.hasOpcode);
    EXPECT_TRUE(out.operands.empty());
}

TEST(Cec, ActiveSourceCarriesPhysicalAddress) {
    CecCodec codec;
    auto bytes = codec.serialize(cecActiveSource(CEC_ADDR_PLAYBACK_1, 0x1000));  // 1.0.0.0
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x4F);                 // initiator 4 -> broadcast (15)
    EXPECT_EQ(bytes[1], CEC_OP_ACTIVE_SOURCE);
    EXPECT_EQ(bytes[2], 0x10);
    EXPECT_EQ(bytes[3], 0x00);

    CecMessage out;
    ASSERT_TRUE(codec.parse(bytes.data(), bytes.size(), out));
    EXPECT_EQ(out.destination, CEC_ADDR_BROADCAST);
    ASSERT_EQ(out.operands.size(), 2u);
    EXPECT_EQ((out.operands[0] << 8) | out.operands[1], 0x1000);
}

TEST(Cec, ArcRequestOpcode) {
    auto bytes = CecCodec().serialize(cecRequestArcInitiation(CEC_ADDR_AUDIO_SYSTEM, CEC_ADDR_TV));
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0x50);                 // audio system (5) -> TV (0)
    EXPECT_EQ(bytes[1], 0xC3);                 // Request ARC Initiation
}

TEST(Cec, RejectsEmptyAndOversizedFrames) {
    CecCodec codec;
    CecMessage out;
    EXPECT_FALSE(codec.parse(nullptr, 0, out));               // empty frame
    std::vector<uint8_t> big(17, 0x00);
    EXPECT_FALSE(codec.parse(big.data(), big.size(), out));   // > 16 bytes
}

TEST(Cec, NameLookups) {
    EXPECT_EQ(cecAddressName(CEC_ADDR_AUDIO_SYSTEM), "Audio System");
    EXPECT_EQ(cecOpcodeName(CEC_OP_INITIATE_ARC), "Initiate ARC");
    EXPECT_EQ(cecOpcodeName(CEC_OP_USER_CONTROL_PRESSED), "User Control Pressed");
}

// ============================================================================
// Test runner entry point
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}