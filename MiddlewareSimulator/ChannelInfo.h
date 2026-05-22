#ifndef CHANNELINFO_H
#define CHANNELINFO_H

#include <string>

struct ChannelInfo {
    int channelId = 0;                  // = DVB service_id / program_number
    std::string channelName = "Default Channel";
    std::string providerName;           // SDT service_descriptor
    int serviceType = 0x01;             // 0x01 = digital TV (SDT service_type)
    int pmtPid = 0;                     // PID carrying this service's PMT (from PAT); assigned at mux time
    int videoPid = 0;                   // PMT elementary stream PID
    int audioPid = 0;                   // PMT elementary stream PID
    int logicalChannelNumber = 0;       // LCN - the number the viewer dials
};

#endif // CHANNELINFO_H