#pragma once

#include "ImGuiWidget.h"

class AudioManager;

class ImGuiAudioWidget : public ImGuiWidget
{
public:
    ImGuiAudioWidget(std::string name = "Audio");
    ~ImGuiAudioWidget() override = default;

    virtual void render() override;

private:
    AudioManager* m_audioManager { nullptr };
    bool m_muted { false };
    float m_volumeBeforeMute { 1.0f };
};
