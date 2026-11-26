#include "WavIo.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace rangemaster::io {

namespace {

std::uint16_t readU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t readU32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

void writeU16(std::ostream& os, std::uint16_t v) {
    const std::uint8_t b[2]{static_cast<std::uint8_t>(v & 0xFFu),
                            static_cast<std::uint8_t>((v >> 8) & 0xFFu)};
    os.write(reinterpret_cast<const char*>(b), 2);
}

void writeU32(std::ostream& os, std::uint32_t v) {
    const std::uint8_t b[4]{static_cast<std::uint8_t>(v & 0xFFu),
                            static_cast<std::uint8_t>((v >> 8) & 0xFFu),
                            static_cast<std::uint8_t>((v >> 16) & 0xFFu),
                            static_cast<std::uint8_t>((v >> 24) & 0xFFu)};
    os.write(reinterpret_cast<const char*>(b), 4);
}

} // namespace

bool readWavMono(const std::string& path, WavData& out, std::string& err) {
    std::ifstream is(path, std::ios::binary);
    if (!is) {
        err = "Cannot open input WAV file: " + path;
        return false;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(is)),
                                    std::istreambuf_iterator<char>());
    if (bytes.size() < 44 ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        err = "Invalid RIFF/WAVE header in: " + path;
        return false;
    }

    bool hasFmt = false;
    bool hasData = false;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;

    std::size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const std::uint32_t chunkSize = readU32(bytes.data() + pos + 4);
        const std::size_t payload = pos + 8;
        if (payload + chunkSize > bytes.size()) {
            break;
        }
        if (std::memcmp(bytes.data() + pos, "fmt ", 4) == 0 && chunkSize >= 16) {
            audioFormat = readU16(bytes.data() + payload + 0);
            channels = readU16(bytes.data() + payload + 2);
            sampleRate = readU32(bytes.data() + payload + 4);
            bitsPerSample = readU16(bytes.data() + payload + 14);
            // Handle WAVE_FORMAT_EXTENSIBLE (0xFFFE)
            if (audioFormat == 0xFFFE && chunkSize >= 40) {
                audioFormat = readU16(bytes.data() + payload + 24);
            }
            hasFmt = true;
        } else if (std::memcmp(bytes.data() + pos, "data", 4) == 0) {
            dataOffset = payload;
            dataSize = chunkSize;
            hasData = true;
        }
        pos = payload + chunkSize + (chunkSize & 1u);
    }

    if (!hasFmt || !hasData || channels == 0 || sampleRate == 0) {
        err = "Missing fmt or data chunk in WAV: " + path;
        return false;
    }

    const std::size_t bytesPerSample = bitsPerSample / 8;
    const std::size_t frameSize = bytesPerSample * channels;
    if (frameSize == 0) {
        err = "Unsupported WAV bit depth";
        return false;
    }
    const std::size_t numFrames = dataSize / frameSize;
    out.sampleRate = sampleRate;
    out.numChannels = 1;
    out.bitsPerSample = bitsPerSample;
    out.samples.resize(numFrames);

    for (std::size_t i = 0; i < numFrames; ++i) {
        double sum = 0.0;
        for (std::uint16_t ch = 0; ch < channels; ++ch) {
            const std::uint8_t* p = bytes.data() + dataOffset + i * frameSize + ch * bytesPerSample;
            double v = 0.0;
            if (audioFormat == 1 && bitsPerSample == 16) {
                const auto s = static_cast<std::int16_t>(readU16(p));
                v = static_cast<double>(s) / 32768.0;
            } else if (audioFormat == 1 && bitsPerSample == 24) {
                std::int32_t s = static_cast<std::int32_t>(p[0]) |
                                 (static_cast<std::int32_t>(p[1]) << 8) |
                                 (static_cast<std::int32_t>(p[2]) << 16);
                if (s & 0x800000) {
                    s |= static_cast<std::int32_t>(0xFF000000u);
                }
                v = static_cast<double>(s) / 8388608.0;
            } else if (audioFormat == 1 && bitsPerSample == 32) {
                const auto s = static_cast<std::int32_t>(readU32(p));
                v = static_cast<double>(s) / 2147483648.0;
            } else if (audioFormat == 3 && bitsPerSample == 32) {
                float f = 0.0f;
                std::memcpy(&f, p, 4);
                v = static_cast<double>(f);
            } else if (audioFormat == 3 && bitsPerSample == 64) {
                double d = 0.0;
                std::memcpy(&d, p, 8);
                v = d;
            } else {
                err = "Unsupported WAV format/bit depth";
                return false;
            }
            sum += v;
        }
        out.samples[i] = sum / static_cast<double>(channels);
    }

    return true;
}

bool writeWavMono(const std::string& path,
                  const std::vector<double>& samples,
                  std::uint32_t sampleRate,
                  std::uint16_t bitsPerSample,
                  bool ieeeFloat,
                  std::string& err) {
    std::ofstream os(path, std::ios::binary);
    if (!os) {
        err = "Cannot open output WAV file: " + path;
        return false;
    }

    const std::uint16_t formatTag = ieeeFloat ? 3 : 1;
    const std::uint16_t bps = ieeeFloat ? 32 : bitsPerSample;
    const std::uint16_t bytesPerSample = bps / 8;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * bytesPerSample);
    const std::uint32_t riffSize = 36 + dataBytes;

    os.write("RIFF", 4);
    writeU32(os, riffSize);
    os.write("WAVE", 4);

    os.write("fmt ", 4);
    writeU32(os, 16);
    writeU16(os, formatTag);
    writeU16(os, 1);
    writeU32(os, sampleRate);
    writeU32(os, sampleRate * bytesPerSample);
    writeU16(os, bytesPerSample);
    writeU16(os, bps);

    os.write("data", 4);
    writeU32(os, dataBytes);

    for (double s : samples) {
        if (!std::isfinite(s)) {
            s = 0.0;
        }
        if (ieeeFloat) {
            const float f = static_cast<float>(s);
            os.write(reinterpret_cast<const char*>(&f), 4);
        } else if (bps == 16) {
            const double clamped = std::clamp(s, -1.0, 1.0);
            const auto q = static_cast<std::int16_t>(std::llround(clamped * 32767.0));
            writeU16(os, static_cast<std::uint16_t>(q));
        } else if (bps == 24) {
            const double clamped = std::clamp(s, -1.0, 1.0);
            const auto q = static_cast<std::int32_t>(std::llround(clamped * 8388607.0));
            const std::uint8_t b[3]{
                static_cast<std::uint8_t>(q & 0xFF),
                static_cast<std::uint8_t>((q >> 8) & 0xFF),
                static_cast<std::uint8_t>((q >> 16) & 0xFF)
            };
            os.write(reinterpret_cast<const char*>(b), 3);
        } else {
            err = "Unsupported output bit depth (use 16, 24, or float32)";
            return false;
        }
    }

    if (!os) {
        err = "Failed writing output WAV file: " + path;
        return false;
    }
    return true;
}

} // namespace rangemaster::io
