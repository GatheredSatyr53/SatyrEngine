#include "engine/SceneList.h"

#include <algorithm>

namespace fs = std::filesystem;

namespace satyr {

SceneList::SceneList(fs::path scenesDir) : m_dir(std::move(scenesDir)) { refresh(); }

void SceneList::refresh()
{
    fs::path keep = m_scenes.empty() ? fs::path{} : m_scenes[m_index];

    m_scenes.clear();
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_dir, ec)) {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".frag")
            m_scenes.push_back(entry.path().lexically_normal());
    }
    std::sort(m_scenes.begin(), m_scenes.end());

    m_index = 0;
    if (!keep.empty()) select(keep);
}

void SceneList::select(const fs::path& file)
{
    const fs::path normalized = file.lexically_normal();
    auto it = std::find(m_scenes.begin(), m_scenes.end(), normalized);
    if (it == m_scenes.end()) {
        m_scenes.push_back(normalized);
        it = m_scenes.end() - 1;
    }
    m_index = static_cast<size_t>(it - m_scenes.begin());
}

void SceneList::next()
{
    refresh();
    if (!m_scenes.empty()) m_index = (m_index + 1) % m_scenes.size();
}

void SceneList::previous()
{
    refresh();
    if (!m_scenes.empty()) m_index = (m_index + m_scenes.size() - 1) % m_scenes.size();
}

} // namespace satyr
