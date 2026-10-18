#pragma once

#include "ImGuiWidget.h"
#include <filesystem>
#include <vector>
#include <unordered_map>

class ImGuiAssetBrowserWidget : public ImGuiWidget
{
public:
    ImGuiAssetBrowserWidget(std::string name = "ImGuiAssetBrowserWidget");
    ~ImGuiAssetBrowserWidget() override = default;

    virtual void render() override;

private:
    static constexpr float c_minThumbnailSize = 48.0f;    // scrolling below this switches to the tree view
    static constexpr float c_maxThumbnailSize = 160.0f;
    static constexpr float c_thumbnailStep = 8.0f;

    struct Entry {
        std::filesystem::path path;
        std::string name;
        bool isDirectory;
    };

    void navigateTo(const std::filesystem::path& directory);
    void refresh();
    std::vector<Entry> listDirectory(const std::filesystem::path& directory) const;
    void drawTree(const std::filesystem::path& directory, const std::string& filter);
    void drawBreadcrumbs();
    void drawEntry(const Entry& entry, float thumbnailSize);
    const char* getIcon(const Entry& entry) const;

private:
    std::filesystem::path m_rootDirectory{ "assets" };
    std::filesystem::path m_currentDirectory{ "assets" };
    std::vector<Entry> m_entries;
    std::unordered_map<std::string, std::vector<Entry>> m_treeCache;
    std::filesystem::path m_selectedPath;
    char m_searchFilter[128]{ "" };
    float m_thumbnailSize{ 72.0f };
    bool m_needsRefresh{ true };
};
