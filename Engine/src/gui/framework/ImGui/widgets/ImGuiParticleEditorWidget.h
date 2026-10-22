#pragma once

#include "ImGuiWidget.h"

class Scene;
class ParticleManager;
struct ParticleEmitter;

// edits one emitter and shows it alone from its own orbit camera, docks as a tab next to the Application view
class ImGuiParticleEditorWidget : public ImGuiWidget
{
public:
    ImGuiParticleEditorWidget(std::string name = "Particle Editor");
    ~ImGuiParticleEditorWidget() override = default;

    virtual void render() override;

private:
    bool _pickEmitter(Scene* scene);
    void _drawPreview(ParticleEmitter& emitter, const glm::vec3& emitterPosition);
    void _resetView();
    void _drawGrid(const glm::mat4& viewProjection, const ImVec2& origin, const ImVec2& size, const glm::vec3& center) const;

    static constexpr uint32_t NO_ENTITY = UINT32_MAX;

    ParticleManager* particleManager { nullptr };
    uint32_t selectedEntityID { NO_ENTITY };
    uint32_t lastSceneSelection { NO_ENTITY };   // follows the scene selection only when it changes

    // orbit camera around the emitter
    float yaw { 45.0f };            // degrees
    float pitch { 20.0f };
    float distance { 10.0f };
    glm::vec3 panOffset { 0.0f };
    bool showGrid { true };

    bool simulating { true };
    float timeScale { 1.0f };
};
