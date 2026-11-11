#pragma once

#include <string>

namespace ImGuiScenePicker {
    void open();
    void draw();

    bool openScene(const std::string& name, const std::string& path);
    void newScene();
    bool saveActiveScene();
}
