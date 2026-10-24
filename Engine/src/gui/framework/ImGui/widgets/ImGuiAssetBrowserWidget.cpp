#include "ImGuiAssetBrowserWidget.h"
#include <imgui_internal.h>
#include <algorithm>

namespace fs = std::filesystem;

namespace {
    std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }
}

ImGuiAssetBrowserWidget::ImGuiAssetBrowserWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiAssetBrowserWidget", sizeof(ImGuiAssetBrowserWidget), MemorySystem::Gui);

}

void ImGuiAssetBrowserWidget::render()
{
    if(!m_isVisible) {
        return;
    }

    ImGui::Begin("Asset Browser");

    if (m_needsRefresh) {
        refresh();
    }

    // toolbar
    ImGui::BeginDisabled(m_currentDirectory == m_rootDirectory);
    if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
        navigateTo(m_currentDirectory.parent_path());
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_SYNC)) {
        m_needsRefresh = true;
    }
    ImGui::SameLine();
    drawBreadcrumbs();

    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 200.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##AssetSearch", ICON_FA_SEARCH " Search...", m_searchFilter, IM_ARRAYSIZE(m_searchFilter));

    ImGui::Separator();

    // content grid, ctrl + scroll resizes the thumbnails instead of scrolling
    // going below the smallest thumbnail size switches to the tree view
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::BeginChild("AssetGrid", ImVec2(0, 0), ImGuiChildFlags_None, io.KeyCtrl ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None);
    if (io.KeyCtrl && io.MouseWheel != 0.0f && ImGui::IsWindowHovered()) {
        m_thumbnailSize = std::clamp(m_thumbnailSize + io.MouseWheel * c_thumbnailStep, c_minThumbnailSize - c_thumbnailStep, c_maxThumbnailSize);
    }

    const std::string filter = toLower(m_searchFilter);

    if (m_thumbnailSize < c_minThumbnailSize) {
        drawTree(m_currentDirectory, filter);
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    const float padding = 12.0f;
    const float cellSize = m_thumbnailSize + padding;
    const int columns = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellSize));

    fs::path pendingDirectory;
    if (ImGui::BeginTable("AssetGridTable", columns)) {
        for (const Entry& entry : m_entries) {
            if (!filter.empty() && toLower(entry.name).find(filter) == std::string::npos) {
                continue;
            }

            ImGui::TableNextColumn();
            ImGui::PushID(entry.path.generic_string().c_str());
            drawEntry(entry, m_thumbnailSize);

            // defer navigation so m_entries isn't rebuilt mid iteration
            if (entry.isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                pendingDirectory = entry.path;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (m_entries.empty()) {
        ImGui::TextDisabled("Empty folder");
    }

    ImGui::EndChild();
    ImGui::End();

    if (!pendingDirectory.empty()) {
        navigateTo(pendingDirectory);
    }
}

void ImGuiAssetBrowserWidget::navigateTo(const fs::path& directory)
{
    // never leave the assets root
    const fs::path relative = directory.lexically_relative(m_rootDirectory);
    if (relative.empty() || *relative.begin() == "..") {
        m_currentDirectory = m_rootDirectory;
    } else {
        m_currentDirectory = directory;
    }
    m_searchFilter[0] = '\0';
    m_needsRefresh = true;
}

void ImGuiAssetBrowserWidget::refresh()
{
    m_needsRefresh = false;
    m_treeCache.clear();

    std::error_code ec;
    if (!fs::is_directory(m_currentDirectory, ec)) {
        m_logger->warn("asset browser: directory not found {}", m_currentDirectory.generic_string());
        m_currentDirectory = m_rootDirectory;
    }

    m_entries = listDirectory(m_currentDirectory);
}

void ImGuiAssetBrowserWidget::drawTree(const fs::path& directory, const std::string& filter)
{
    // folders are listed lazily the first time they are expanded, cleared on refresh
    auto it = m_treeCache.find(directory.generic_string());
    if (it == m_treeCache.end()) {
        it = m_treeCache.emplace(directory.generic_string(), listDirectory(directory)).first;
    }

    for (const Entry& entry : it->second) {
        if (!entry.isDirectory && !filter.empty() && toLower(entry.name).find(filter) == std::string::npos) {
            continue;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth
                                | ImGuiTreeNodeFlags_DrawLinesToNodes;
        if (!entry.isDirectory) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (m_selectedPath == entry.path) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const std::string label = std::format("{}  {}", getIcon(entry), entry.name);
        const bool isOpen = ImGui::TreeNodeEx(entry.path.generic_string().c_str(), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            m_selectedPath = entry.path;
        }

        if (entry.isDirectory && isOpen) {
            drawTree(entry.path, filter);
            ImGui::TreePop();
        }
    }
}

std::vector<ImGuiAssetBrowserWidget::Entry> ImGuiAssetBrowserWidget::listDirectory(const fs::path& directory) const
{
    std::vector<Entry> entries;

    std::error_code ec;
    for (const auto& dirEntry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec)) {
        entries.push_back({ dirEntry.path(), dirEntry.path().filename().string(), dirEntry.is_directory(ec) });
    }

    // folders come first, then alphabetical
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory;
        }
        return toLower(a.name) < toLower(b.name);
    });

    return entries;
}

void ImGuiAssetBrowserWidget::drawBreadcrumbs()
{
    fs::path accumulated = m_rootDirectory;
    if (ImGui::SmallButton(m_rootDirectory.generic_string().c_str())) {
        navigateTo(accumulated);
    }

    const fs::path relative = m_currentDirectory.lexically_relative(m_rootDirectory);
    int depth = 0;
    for (const fs::path& part : relative) {
        if (part == ".") {
            continue;
        }
        accumulated /= part;
        ImGui::SameLine();
        ImGui::TextDisabled("/");
        ImGui::SameLine();
        ImGui::PushID(depth++);
        if (ImGui::SmallButton(part.generic_string().c_str())) {
            navigateTo(accumulated);
        }
        ImGui::PopID();
    }
}

void ImGuiAssetBrowserWidget::drawEntry(const Entry& entry, float thumbnailSize)
{
    ImVec2 cellPos = ImGui::GetCursorScreenPos();
    const bool isSelected = (m_selectedPath == entry.path);

    ImGui::BeginGroup();
    if (ImGui::Selectable("##entry", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(thumbnailSize, thumbnailSize))) {
        m_selectedPath = entry.path;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", entry.path.generic_string().c_str());
    }

    // large centered icon on top of the selectable
    const char* icon = getIcon(entry);
    const float iconFontSize = thumbnailSize * 0.55f;
    ImFont* font = ImGui::GetFont();

    // center on the glyph's visible bounds, the icon font is merged with a min advance and offset
    // so its advance box is wider than the drawn glyph and centering that leaves it skewed left
    unsigned int codepoint = 0;
    ImTextCharFromUtf8(&codepoint, icon, nullptr);
    const ImFontGlyph* glyph = font->GetFontBaked(iconFontSize)->FindGlyph(static_cast<ImWchar>(codepoint));
    const ImVec2 cellCenter = ImVec2(cellPos.x + thumbnailSize * 0.5f, cellPos.y + thumbnailSize * 0.5f);
    ImVec2 iconPos = glyph
        ? ImVec2(cellCenter.x - (glyph->X0 + glyph->X1) * 0.5f, cellCenter.y - (glyph->Y0 + glyph->Y1) * 0.5f)
        : cellPos;
    ImU32 iconColor = entry.isDirectory ? IM_COL32(160, 160, 160, 255) : IM_COL32(210, 210, 210, 255);
    ImGui::GetWindowDrawList()->AddText(font, iconFontSize, iconPos, iconColor, icon);

    // file name under the icon, wrapped to the cell width
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize);
    ImGui::TextWrapped("%s", entry.name.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
}

const char* ImGuiAssetBrowserWidget::getIcon(const Entry& entry) const
{
    if (entry.isDirectory) {
        return ICON_FA_FOLDER;
    }

    const std::string ext = toLower(entry.path.extension().string());
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr" || ext == ".ktx" || ext == ".dds") {
        return ICON_FA_FILE_IMAGE;
    }
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
        return ICON_FA_CUBE;
    }
    if (ext == ".lua" || ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".spv" || ext == ".hlsl"
        || ext == ".rgen" || ext == ".rchit" || ext == ".rmiss") {
        return ICON_FA_FILE_CODE;
    }
    if (ext == ".ttf" || ext == ".otf") {
        return ICON_FA_FONT;
    }
    if (ext == ".json" || ext == ".ini" || ext == ".txt" || ext == ".md") {
        return ICON_FA_FILE_ALT;
    }
    return ICON_FA_FILE;
}
