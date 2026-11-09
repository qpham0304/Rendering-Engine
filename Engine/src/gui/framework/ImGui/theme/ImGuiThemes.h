#pragma once

#include "../../../Themes/Themes.h"
#include "Imgui.h"

#include <functional>
#include <string>
#include <vector>

class ImGuiThemes : public Themes
{

public:
    struct Theme {
        std::string name;
        std::function<void()> apply;
    };

    static const std::vector<Theme>& list();

    static void purpleTheme();
    static void darkTheme();
    static void lightTheme();
    static void discordDark();
    static void photoshop();
    static void darcula();
    static void lightOrange();
};
