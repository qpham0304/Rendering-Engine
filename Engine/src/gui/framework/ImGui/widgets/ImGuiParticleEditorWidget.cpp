#include "ImGuiParticleEditorWidget.h"
#include "ImGuiParticleInspector.h"

#include "core/scene/SceneManager.h"
#include "core/components/MComponent.h"
#include "particle/ParticleManager.h"
#include "core/features/EngineStates.h"
#include <imgui_internal.h>
#include <algorithm>
#include <iterator>

ImGuiParticleEditorWidget::ImGuiParticleEditorWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiParticleEditorWidget", sizeof(ImGuiParticleEditorWidget), MemorySystem::Gui);
    particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");
}

void ImGuiParticleEditorWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    // first use only, afterwards imgui.ini remembers wherever it was docked
    if (ImGuiWindow* application = ImGui::FindWindowByName("Application"); application && application->DockId != 0) {
        ImGui::SetNextWindowDockID(application->DockId, ImGuiCond_FirstUseEver);
    }

    // hidden tabs return false, so the preview is only requested while it can be seen
    if (!ImGui::Begin(m_name.c_str(), &m_isVisible)) {
        ImGui::End();
        return;
    }

    Scene* scene = SceneManager::getInstance().getActiveScene();
    if (!scene || !_pickEmitter(scene)) {
        ImGui::TextDisabled("No particle emitter in the scene, add a ParticleEmitter component to an entity.");
        ImGui::End();
        return;
    }

    Entity entity = scene->getEntity(selectedEntityID);
    ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
    glm::vec3 emitterPosition = glm::vec3(entity.getComponent<TransformComponent>().getModelMatrix()[3]);

    if (ImGui::BeginTable("ParticleEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.62f);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        _drawPreview(emitter, emitterPosition);

        ImGui::TableNextColumn();
        if (ImGui::BeginChild("ParticleInspector", ImVec2(0.0f, 0.0f))) {
            ImGuiParticleInspector::draw(emitter, textureManager, modelManager);
        }
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
}

bool ImGuiParticleEditorWidget::_pickEmitter(Scene* scene)
{
    std::vector<Entity> emitters = scene->getEntitiesWith<ParticleEmitter>();
    if (emitters.empty()) {
        selectedEntityID = NO_ENTITY;
        return false;
    }

    // clicking an emitter in the scene switches the editor to it
    const std::vector<Entity>& selection = scene->getSelectedEntities();
    uint32_t sceneSelection = selection.empty() ? NO_ENTITY : selection[0].getID();
    if (sceneSelection != lastSceneSelection) {
        lastSceneSelection = sceneSelection;
        if (sceneSelection != NO_ENTITY && selection[0].hasComponent<ParticleEmitter>()) {
            selectedEntityID = sceneSelection;
            _resetView();
        }
    }

    auto isEmitter = [&](uint32_t id) {
        return std::any_of(emitters.begin(), emitters.end(), [&](const Entity& e) { return e.getID() == id; });
    };
    if (!isEmitter(selectedEntityID)) {
        selectedEntityID = emitters[0].getID();
        _resetView();
    }

    auto nameOf = [](const Entity& e) {
        return e.hasComponent<NameComponent>() ? e.getComponent<NameComponent>().name : std::string("Entity ") + std::to_string(e.getID());
    };

    // toolbar: the editor's clock, emitter picker and view options
    const bool gamePlaying = EngineState::isPlaying();
    ImGui::BeginDisabled(gamePlaying);
    if (ImGui::Button(simulating ? ICON_FA_PAUSE " Pause" : ICON_FA_PLAY " Simulate")) {
        simulating = !simulating;
    }
    ImGui::SameLine();
    const float speeds[] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f };
    const char* speedNames[] = { "0.1x", "0.25x", "0.5x", "1x", "2x" };
    int speedIndex = static_cast<int>(std::find(std::begin(speeds), std::end(speeds), timeScale) - std::begin(speeds));
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.5f);
    if (ImGui::Combo("##speed", &speedIndex, speedNames, IM_ARRAYSIZE(speedNames))) {
        timeScale = speeds[speedIndex];
    }
    ImGui::EndDisabled();
    if (gamePlaying && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("the game is playing, every emitter runs on game time");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_REDO " Restart")) {
        scene->getEntity(selectedEntityID).getComponent<ParticleEmitter>().resetRequested = true;
    }
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(ImGui::GetFontSize(), 0.0f));
    ImGui::SameLine();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Emitter");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
    if (ImGui::BeginCombo("##emitter", nameOf(scene->getEntity(selectedEntityID)).c_str())) {
        for (const Entity& e : emitters) {
            bool selected = e.getID() == selectedEntityID;
            ImGui::PushID(static_cast<int>(e.getID()));
            if (ImGui::Selectable(nameOf(e).c_str(), selected)) {
                selectedEntityID = e.getID();
                _resetView();
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid);
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) {
        _resetView();
    }

    return true;
}

void ImGuiParticleEditorWidget::_drawPreview(ParticleEmitter& emitter, const glm::vec3& emitterPosition)
{
    ImVec2 available = ImGui::GetContentRegionAvail();
    ImVec2 imagePos = ImGui::GetCursorScreenPos();

    // rounded to 16 px so dragging the panel edge does not recreate the targets every frame,
    // the image is shown at exactly that size so the grid and the particles share one projection
    uint32_t width = std::max(64u, (static_cast<uint32_t>(std::max(available.x, 0.0f)) / 16u) * 16u);
    uint32_t height = std::max(64u, (static_cast<uint32_t>(std::max(available.y, 0.0f)) / 16u) * 16u);
    ImVec2 imageSize = ImVec2(static_cast<float>(width), static_cast<float>(height));

    glm::vec3 target = emitterPosition + panOffset;
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    glm::vec3 eye = target + distance * glm::vec3(std::cos(pitchRad) * std::cos(yawRad), std::sin(pitchRad), std::cos(pitchRad) * std::sin(yawRad));
    glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.05f, 1000.0f);

    ParticleManager::PreviewRequest request{};
    request.containerID = emitter.containerID;
    request.width = width;
    request.height = height;
    request.view = view;
    request.projection = projection;
    request.cameraPosition = eye;
    request.simulate = simulating;
    request.timeScale = timeScale;
    particleManager->requestPreview(request);

    // the texture is drawn the frame after the first request
    uint32_t textureID = particleManager->getPreviewTextureID();
    void* texture = textureID != 0 ? textureManager->inspectTexture(textureID) : nullptr;
    if (texture) {
        ImGui::Image((ImTextureID)texture, imageSize);
    } else {
        ImGui::Dummy(imageSize);
        ImGui::GetWindowDrawList()->AddRectFilled(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), IM_COL32(18, 18, 20, 255));
    }

    if (showGrid) {
        _drawGrid(projection * view, imagePos, imageSize, emitterPosition);
    }

    // a button over the image takes the mouse so the scene camera does not move with it
    ImGui::SetCursorScreenPos(imagePos);
    ImGui::InvisibleButton("##previewInput", imageSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActive()) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            yaw += io.MouseDelta.x * 0.3f;
            pitch = std::clamp(pitch + io.MouseDelta.y * 0.3f, -89.0f, 89.0f);
        } else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            glm::vec3 forward = glm::normalize(target - eye);
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 up = glm::cross(right, forward);
            float panSpeed = distance * 0.0015f;
            panOffset += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * panSpeed;
        }
    }
    if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f) {
        distance = std::clamp(distance * (1.0f - io.MouseWheel * 0.1f), 0.5f, 500.0f);
    }

    // clock state top left, mouse controls bottom left
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const char* state = EngineState::isPlaying() ? "game time" : (simulating ? "simulating" : "paused");
    std::string status = std::string(state) + (emitter.isPlaying ? "" : ", not emitting");
    drawList->AddText(ImVec2(imagePos.x + 8.0f, imagePos.y + 6.0f), IM_COL32(230, 230, 230, 220), status.c_str());
    const char* hint = "drag: orbit   right drag: pan   wheel: zoom";
    float lineHeight = ImGui::GetTextLineHeight();
    drawList->AddText(ImVec2(imagePos.x + 8.0f, imagePos.y + imageSize.y - lineHeight - 6.0f), IM_COL32(160, 160, 160, 160), hint);
}

void ImGuiParticleEditorWidget::_drawGrid(const glm::mat4& viewProjection, const ImVec2& origin, const ImVec2& size, const glm::vec3& center) const
{
    const int halfLines = 10;           // 21 lines each way, one unit apart
    const float spacing = 1.0f;
    const float extent = halfLines * spacing;
    const float nearW = 0.01f;          // clip space w of the cut, anything closer is behind the camera

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    auto toScreen = [&](const glm::vec4& clip) {
        glm::vec2 ndc = glm::vec2(clip) / clip.w;
        return ImVec2(origin.x + (ndc.x * 0.5f + 0.5f) * size.x, origin.y + (0.5f - ndc.y * 0.5f) * size.y);
    };

    // cut the part of a line behind the camera instead of dropping the whole line
    auto drawLine = [&](const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness) {
        glm::vec4 clipA = viewProjection * glm::vec4(a, 1.0f);
        glm::vec4 clipB = viewProjection * glm::vec4(b, 1.0f);
        if (clipA.w < nearW && clipB.w < nearW) {
            return;
        }
        if (clipA.w < nearW) {
            clipA = glm::mix(clipA, clipB, (nearW - clipA.w) / (clipB.w - clipA.w));
        } else if (clipB.w < nearW) {
            clipB = glm::mix(clipB, clipA, (nearW - clipB.w) / (clipA.w - clipB.w));
        }
        drawList->AddLine(toScreen(clipA), toScreen(clipB), color, thickness);
    };

    for (int i = -halfLines; i <= halfLines; i++) {
        float offset = i * spacing;
        // lines fade toward the edge so the grid does not end in a hard square
        float fade = 1.0f - static_cast<float>(std::abs(i)) / static_cast<float>(halfLines + 1);
        int alpha = static_cast<int>(110.0f * fade);
        ImU32 lineColor = IM_COL32(150, 150, 150, alpha);
        bool axis = i == 0;

        // along z at this x, the center one is the z axis in blue
        drawLine(center + glm::vec3(offset, 0.0f, -extent), center + glm::vec3(offset, 0.0f, extent),
            axis ? IM_COL32(70, 110, 230, 200) : lineColor, axis ? 2.0f : 1.0f);
        // along x at this z, the center one is the x axis in red
        drawLine(center + glm::vec3(-extent, 0.0f, offset), center + glm::vec3(extent, 0.0f, offset),
            axis ? IM_COL32(220, 70, 70, 200) : lineColor, axis ? 2.0f : 1.0f);
    }

    drawList->PopClipRect();
}

void ImGuiParticleEditorWidget::_resetView()
{
    yaw = 45.0f;
    pitch = 20.0f;
    distance = 10.0f;
    panOffset = glm::vec3(0.0f);
}
