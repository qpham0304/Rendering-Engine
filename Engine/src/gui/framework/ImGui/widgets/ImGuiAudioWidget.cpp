#include "ImGuiAudioWidget.h"

#include "audio/AudioManager.h"
#include <imgui.h>

ImGuiAudioWidget::ImGuiAudioWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiAudioWidget", sizeof(ImGuiAudioWidget), MemorySystem::Gui);
    m_audioManager = &ServiceLocator::GetService<AudioManager>("AudioManager");
}

void ImGuiAudioWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(getName(), &m_isVisible);

    // muting drops the master to 0, the slider shows the level it comes back to
    float master = m_muted ? m_volumeBeforeMute : m_audioManager->getMasterVolume();
    if (ImGui::SliderFloat("Master", &master, 0.0f, 1.0f, "%.2f")) {
        if (m_muted) {
            m_volumeBeforeMute = master;
        } else {
            m_audioManager->setMasterVolume(master);
        }
    }

    float music = m_audioManager->getMusicVolume();
    if (ImGui::SliderFloat("Music", &music, 0.0f, 1.0f, "%.2f")) {
        m_audioManager->setMusicVolume(music);
    }

    if (ImGui::Checkbox("Mute", &m_muted)) {
        if (m_muted) {
            m_volumeBeforeMute = m_audioManager->getMasterVolume();
            m_audioManager->setMasterVolume(0.0f);
        } else {
            m_audioManager->setMasterVolume(m_volumeBeforeMute);
        }
    }

    ImGui::Text("Playing: %zu", m_audioManager->listIDs().size());
    ImGui::SameLine();
    if (ImGui::Button("Stop All")) {
        m_audioManager->stopAll();
    }

    ImGui::End();
}
