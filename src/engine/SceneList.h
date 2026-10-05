// The set of scene fragment shaders in shaders/scenes, cycled with [ and ].
#pragma once

#include <filesystem>
#include <vector>

namespace satyr {

class SceneList {
public:
    explicit SceneList(std::filesystem::path scenesDir);

    // Rescans the directory (new files show up without restarting).
    void refresh();

    bool empty() const { return m_scenes.empty(); }
    size_t size() const { return m_scenes.size(); }
    const std::filesystem::path& current() const { return m_scenes[m_index]; }

    // Makes `file` current, adding it to the list if it lives outside the scenes directory.
    void select(const std::filesystem::path& file);
    void next();
    void previous();

private:
    std::filesystem::path m_dir;
    std::vector<std::filesystem::path> m_scenes;
    size_t m_index = 0;
};

} // namespace satyr
