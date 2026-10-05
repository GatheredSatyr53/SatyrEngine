// Dependency-free PNG writer (stored/uncompressed deflate) for screenshots.
#pragma once

#include <cstdint>
#include <filesystem>

namespace satyr {

// `rgb` holds width*height*3 bytes. Set `bottomUp` when rows come from glReadPixels.
bool writePNG(const std::filesystem::path& file, int width, int height, const std::uint8_t* rgb, bool bottomUp);

} // namespace satyr
