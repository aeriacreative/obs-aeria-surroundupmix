#pragma once

#include <cstddef>
#include <string>

namespace AeriaSurround {

enum class ChannelIndex : size_t {
    FrontLeft   = 0, // FL
    FrontRight  = 1, // FR
    Center      = 2, // FC / C
    LFE         = 3, // Subwoofer
    RearLeft    = 4, // RL
    RearRight   = 5, // RR
    SideLeft    = 6, // SL (in 7.1)
    SideRight   = 7, // SR (in 7.1)
    MaxChannels = 8
};

enum class UpmixMode {
    Auto = 0,       // Detect from OBS master audio output
    Force5Point1 = 1,
    Force7Point1 = 2,
    Direct = 3      // Gain adjustment only, pass through original
};

inline const char* ChannelName(ChannelIndex ch) {
    switch (ch) {
    case ChannelIndex::FrontLeft:  return "L";
    case ChannelIndex::FrontRight: return "R";
    case ChannelIndex::Center:     return "C";
    case ChannelIndex::LFE:        return "LFE";
    case ChannelIndex::RearLeft:   return "RL";
    case ChannelIndex::RearRight:  return "RR";
    case ChannelIndex::SideLeft:   return "SL";
    case ChannelIndex::SideRight:  return "SR";
    default:                       return "?";
    }
}

inline const char* ChannelFullName(ChannelIndex ch) {
    switch (ch) {
    case ChannelIndex::FrontLeft:  return "Front Left";
    case ChannelIndex::FrontRight: return "Front Right";
    case ChannelIndex::Center:     return "Center";
    case ChannelIndex::LFE:        return "Subwoofer (LFE)";
    case ChannelIndex::RearLeft:   return "Rear Left";
    case ChannelIndex::RearRight:  return "Rear Right";
    case ChannelIndex::SideLeft:   return "Side Left";
    case ChannelIndex::SideRight:  return "Side Right";
    default:                       return "Unknown";
    }
}

} // namespace AeriaSurround
