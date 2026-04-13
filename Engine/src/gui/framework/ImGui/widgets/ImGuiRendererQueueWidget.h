#pragma once

#include <imgui.h>
#include "ImGuiWidget.h"
#include <functional>

class ImGuiRendererQueueWidget : public ImGuiWidget {
    public:
        ImGuiRendererQueueWidget(std::string name = "ImGuiRendererQueueWidget");
        virtual ~ImGuiRendererQueueWidget() override;
        virtual void render() override;

        void addRender();
    private:
        // std::function<void()> m_renderQueue;
};