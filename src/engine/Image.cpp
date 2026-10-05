#include "engine/Image.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <vector>

namespace satyr {

namespace {

std::uint32_t crc32(const std::uint8_t* data, size_t len, std::uint32_t crc = 0)
{
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t n = 0; n < 256; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* data, size_t len)
{
    std::uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void writeChunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& payload)
{
    putU32(out, static_cast<std::uint32_t>(payload.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), payload.begin(), payload.end());
    putU32(out, crc32(out.data() + start, out.size() - start));
}

} // namespace

bool writePNG(const std::filesystem::path& file, int width, int height, const std::uint8_t* rgb, bool bottomUp)
{
    if (width <= 0 || height <= 0 || !rgb) return false;
    const size_t rowBytes = static_cast<size_t>(width) * 3;

    // Raw scanlines: filter byte (0 = none) followed by the pixels, top row first.
    std::vector<std::uint8_t> raw;
    raw.reserve((rowBytes + 1) * static_cast<size_t>(height));
    for (int y = 0; y < height; ++y) {
        const int srcRow = bottomUp ? (height - 1 - y) : y;
        raw.push_back(0);
        const std::uint8_t* row = rgb + static_cast<size_t>(srcRow) * rowBytes;
        raw.insert(raw.end(), row, row + rowBytes);
    }

    // zlib stream made of stored (uncompressed) deflate blocks, 65535 bytes max each.
    std::vector<std::uint8_t> idat;
    idat.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    idat.push_back(0x78);
    idat.push_back(0x01);
    size_t pos = 0;
    do {
        const size_t len = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        idat.push_back(last ? 1 : 0);
        idat.push_back(static_cast<std::uint8_t>(len & 0xFF));
        idat.push_back(static_cast<std::uint8_t>(len >> 8));
        idat.push_back(static_cast<std::uint8_t>(~len & 0xFF));
        idat.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFF));
        idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    putU32(idat, adler32(raw.data(), raw.size()));

    std::vector<std::uint8_t> ihdr;
    putU32(ihdr, static_cast<std::uint32_t>(width));
    putU32(ihdr, static_cast<std::uint32_t>(height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // colour type: truecolour RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace

    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    writeChunk(png, "IHDR", ihdr);
    writeChunk(png, "IDAT", idat);
    writeChunk(png, "IEND", {});

    std::ofstream out(file, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "[image] cannot write %s\n", file.string().c_str());
        return false;
    }
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(out);
}

} // namespace satyr
