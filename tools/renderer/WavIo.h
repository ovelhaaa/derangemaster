#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rangemaster::io {

struct WavData {
    std::uint32_t sampleRate{48000};
    std::uint16_t numChannels{1};
    std::uint16_t bitsPerSample{32};
    std::vector<double> samples; // mono (if input is stereo, downmixed or channel 0 extracted)
};

bool readWavMono(const std::string& path, WavData& out, std::string& err);
bool writeWavMono(const std::string& path,
                  const std::vector<double>& samples,
                  std::uint32_t sampleRate,
                  std::uint16_t bitsPerSample,
                  bool ieeeFloat,
                  std::string& err);

} // namespace rangemaster::io
