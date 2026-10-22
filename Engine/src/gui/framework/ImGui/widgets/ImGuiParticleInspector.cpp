#include "ImGuiParticleInspector.h"

#include "core/components/MComponent.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/ModelManager.h"
#include "graphics/utils/Utils.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <string>

void ImGuiParticleInspector::draw(ParticleEmitter& emitter, TextureManager* textureManager, ModelManager* modelManager)
{
    ImGui::PushID("ParticleEmitter");

    // every section is a framed header over a label | widget table so the controls line up
    auto BeginSection = [](const char* name, bool defaultOpen) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (defaultOpen) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (!ImGui::TreeNodeEx(name, flags)) {
            return false;
        }
        if (!ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            ImGui::TreePop();
            return false;
        }
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.5f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    };
    auto EndSection = []() {
        ImGui::EndTable();
        ImGui::TreePop();
    };
    // label cell, then leaves the cursor in the value cell with a full width item
    auto Row = [](const char* label, const char* tooltip = nullptr) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    };
    auto Range = [](const char* id, float& minValue, float& maxValue, float speed, float lower, float upper, const char* format) {
        std::string minFormat = std::string("min ") + format;
        std::string maxFormat = std::string("max ") + format;
        return ImGui::DragFloatRange2(id, &minValue, &maxValue, speed, lower, upper, minFormat.c_str(), maxFormat.c_str(), ImGuiSliderFlags_AlwaysClamp);
    };

    // transport and status
    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    // only spawning, time itself comes from the game or the Particle Editor's Simulate
    if (ImGui::Button(emitter.isPlaying ? "Emit: On" : "Emit: Off", ImVec2(buttonWidth, 0.0f))) {
        emitter.isPlaying = !emitter.isPlaying;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Off stops new particles, the live ones still finish their life");
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart", ImVec2(buttonWidth, 0.0f))) {
        emitter.resetRequested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("kill every particle and start emitting again");
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst", ImVec2(buttonWidth, 0.0f))) {
        emitter.pendingBurst += std::max(emitter.burstCount, 1);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("spawn Burst count particles now");
    }
    ImGui::TextDisabled("container %u, up to %d particles", emitter.containerID, emitter.maxParticles);
    ImGui::Spacing();

    if (BeginSection("Emission", true)) {
        // resizing recreates the gpu buffers, so the typed value is held aside and applied once the edit is done
        Row("Max particles", "pool size, changing it restarts the emitter");
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int* pendingMaxParticles = storage->GetIntRef(ImGui::GetID("MaxParticlesPending"), emitter.maxParticles);
        ImGui::InputInt("##maxParticles", pendingMaxParticles, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            emitter.maxParticles = std::clamp(*pendingMaxParticles, 1, 1 << 20);
        } else if (!ImGui::IsItemActive()) {
            *pendingMaxParticles = emitter.maxParticles;    // follow changes from scripts or other entities
        }

        Row("Rate", "particles spawned per second");
        ImGui::DragFloat("##rate", &emitter.emitRate, 1.0f, 0.0f, 100000.0f, "%.1f / s");

        Row("Burst", "particles spawned at once, repeated on the interval");
        ImGui::DragInt("##burstCount", &emitter.burstCount, 1.0f, 0, 100000, "%d particles");

        Row("Burst every", "seconds between bursts, 0 only bursts from the Burst button");
        ImGui::DragFloat("##burstInterval", &emitter.burstInterval, 0.01f, 0.0f, 60.0f, emitter.burstInterval > 0.0f ? "%.2f s" : "manual only");

        Row("Lifetime", "seconds each particle lives, picked per particle");
        Range("##lifetime", emitter.lifetimeMin, emitter.lifetimeMax, 0.01f, 0.01f, 60.0f, "%.2f s");

        Row("Space", "Local: particles move with the entity, forces act in its space");
        int space = emitter.localSpace ? 1 : 0;
        if (ImGui::Combo("##space", &space, "World\0Local\0")) {
            emitter.localSpace = space == 1;
        }
        EndSection();
    }

    if (BeginSection("Spawn shape", true)) {
        const char* shapes[] = { "Point", "Box", "Sphere", "Hemisphere", "Cone", "Circle" };
        static_assert(IM_ARRAYSIZE(shapes) == static_cast<int>(ParticleSpawnShape::Count));
        Row("Shape", "where new particles appear, placed and rotated by the entity transform");
        ImGui::Combo("##shape", &emitter.spawnShape, shapes, IM_ARRAYSIZE(shapes));

        ParticleSpawnShape shape = static_cast<ParticleSpawnShape>(emitter.spawnShape);
        if (shape == ParticleSpawnShape::Box) {
            Row("Half extents");
            ImGui::DragFloat3("##extents", &emitter.shapeExtents[0], 0.01f, 0.0f, 1000.0f, "%.2f");
        }
        if (shape == ParticleSpawnShape::Sphere || shape == ParticleSpawnShape::Hemisphere
            || shape == ParticleSpawnShape::Cone || shape == ParticleSpawnShape::Circle) {
            Row("Radius");
            ImGui::DragFloat("##radius", &emitter.shapeRadius, 0.01f, 0.0f, 1000.0f, "%.2f");
        }
        if (shape == ParticleSpawnShape::Cone) {
            Row("Cone angle", "how far the edge of the cone tilts outward");
            ImGui::SliderFloat("##coneAngle", &emitter.shapeConeAngle, 0.0f, 90.0f, "%.1f deg");
        }
        if (shape != ParticleSpawnShape::Point) {
            Row("Spawn on");
            int surface = emitter.emitFromShell ? 1 : 0;
            if (ImGui::Combo("##spawnOn", &surface, "Volume\0Surface\0")) {
                emitter.emitFromShell = surface == 1;
            }
        }
        EndSection();
    }

    if (BeginSection("Velocity", true)) {
        Row("Direction", "Outward pushes each particle away from the shape, Point gives an explosion");
        int outward = emitter.velocityFromShape ? 1 : 0;
        if (ImGui::Combo("##directionMode", &outward, "Fixed direction\0Outward from shape\0")) {
            emitter.velocityFromShape = outward == 1;
        }
        if (!emitter.velocityFromShape) {
            Row("Vector", "in entity space, normalized");
            ImGui::DragFloat3("##direction", &emitter.direction[0], 0.01f, -1.0f, 1.0f, "%.2f");
        }

        Row("Spread", "random cone around the direction");
        ImGui::SliderFloat("##spread", &emitter.spreadAngle, 0.0f, 180.0f, "%.1f deg");

        Row("Speed");
        Range("##speed", emitter.speedMin, emitter.speedMax, 0.05f, 0.0f, 1000.0f, "%.2f");
        EndSection();
    }

    if (BeginSection("Forces", false)) {
        Row("Gravity", "multiplier on world gravity");
        ImGui::DragFloat("##gravity", &emitter.gravityScale, 0.01f, -10.0f, 10.0f, "%.2f x");

        Row("Constant force");
        ImGui::DragFloat3("##force", &emitter.force[0], 0.05f, -100.0f, 100.0f, "%.2f");

        Row("Drag", "slows particles down over time");
        ImGui::DragFloat("##drag", &emitter.drag, 0.01f, 0.0f, 50.0f, "%.2f");

        Row("Turbulence", "strength | frequency of the noise field");
        float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        ImGui::SetNextItemWidth(half);
        ImGui::DragFloat("##turbulence", &emitter.turbulenceStrength, 0.05f, 0.0f, 100.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##turbulenceFrequency", &emitter.turbulenceFrequency, 0.01f, 0.01f, 20.0f, "freq %.2f");

        Row("Vortex", "swirl around the entity's up axis");
        ImGui::DragFloat("##vortex", &emitter.vortexStrength, 0.05f, -100.0f, 100.0f, "%.2f");

        Row("Attract", "pull toward the entity, negative pushes away");
        ImGui::DragFloat("##attract", &emitter.attractStrength, 0.05f, -100.0f, 100.0f, "%.2f");
        EndSection();
    }

    if (BeginSection("Size & rotation", false)) {
        Row("Size", "picked per particle at spawn");
        Range("##size", emitter.sizeMin, emitter.sizeMax, 0.005f, 0.0f, 100.0f, "%.3f");

        Row("Size at death", "multiplier reached at the end of the lifetime");
        ImGui::DragFloat("##sizeEnd", &emitter.sizeEndScale, 0.01f, 0.0f, 20.0f, "%.2f x");

        Row("Start rotation");
        Range("##rotation", emitter.rotationMin, emitter.rotationMax, 1.0f, -360.0f, 360.0f, "%.0f deg");

        Row("Spin");
        Range("##spin", emitter.angularVelocityMin, emitter.angularVelocityMax, 1.0f, -3600.0f, 3600.0f, "%.0f deg/s");
        EndSection();
    }

    if (BeginSection("Color", false)) {
        ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar;
        Row("Start");
        ImGui::ColorEdit4("##colorStart", &emitter.colorStart[0], colorFlags);

        Row("End", "blended toward over the lifetime");
        ImGui::ColorEdit4("##colorEnd", &emitter.colorEnd[0], colorFlags);

        Row("Fade in", "part of the lifetime spent fading in");
        ImGui::SliderFloat("##fadeIn", &emitter.fadeIn, 0.0f, 1.0f, "%.2f of life");

        Row("Fade out", "part of the lifetime spent fading out");
        ImGui::SliderFloat("##fadeOut", &emitter.fadeOut, 0.0f, 1.0f, "%.2f of life");
        EndSection();
    }

    if (BeginSection("Rendering", true)) {
        const char* renderModes[] = { "Billboard", "Billboard (upright)", "Stretched by velocity", "Flat on ground", "Fixed to entity", "Mesh" };
        static_assert(IM_ARRAYSIZE(renderModes) == static_cast<int>(ParticleRenderMode::Count));
        Row("Draw as", "Billboard faces the camera, Fixed to entity keeps the entity's orientation");
        ImGui::Combo("##renderMode", &emitter.renderMode, renderModes, IM_ARRAYSIZE(renderModes));

        const char* blendModes[] = { "Additive (glow)", "Alpha (sorted)" };
        static_assert(IM_ARRAYSIZE(blendModes) == static_cast<int>(ParticleBlendMode::Count));
        Row("Blend", "Additive suits fire and sparks, Alpha suits smoke and debris");
        ImGui::Combo("##blend", &emitter.blendMode, blendModes, IM_ARRAYSIZE(blendModes));

        ParticleRenderMode renderMode = static_cast<ParticleRenderMode>(emitter.renderMode);
        if (renderMode == ParticleRenderMode::VelocityStretched) {
            Row("Stretch", "extra length per unit of speed");
            ImGui::DragFloat("##stretch", &emitter.stretchFactor, 0.005f, 0.0f, 10.0f, "%.3f");
        }

        if (renderMode == ParticleRenderMode::Mesh) {
            const std::string primitivePrefix = "$prim$";
            auto isPrimitive = [&](const std::string& path) { return path.rfind(primitivePrefix, 0) == 0; };
            auto displayName = [&](const std::string& path) {
                return isPrimitive(path) ? path.substr(primitivePrefix.size()) : std::filesystem::path(path).filename().string();
            };

            Row("Mesh", "primitives or any model the scene has loaded, Browse loads a new one");
            float browseWidth = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::BeginCombo("##mesh", displayName(emitter.meshPath).c_str())) {
                auto MeshItem = [&](const std::string& path) {
                    bool selected = emitter.meshPath == path;
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(displayName(path).c_str(), selected)) {
                        emitter.meshPath = path;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (!isPrimitive(path) && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", path.c_str());
                    }
                    ImGui::PopID();
                };

                ImGui::SeparatorText("Primitives");
                for (const std::string& name : ModelManager::listPrimitives()) {
                    MeshItem(name);
                }
                ImGui::SeparatorText("Loaded models");
                for (const std::string& path : modelManager->listModelPaths()) {
                    if (!isPrimitive(path)) {
                        MeshItem(path);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse##mesh")) {
                std::string path = Utils::fileDialog();
                if (!path.empty()) {
                    emitter.meshPath = path;    // the renderer loads it on its next update
                }
            }
            if (emitter.meshModelID == 0 && emitter.loadedMeshPath == emitter.meshPath) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "failed to load, drawing billboards");
            }

            Row("Shading", "0 flat color, 1 fully lit by a fixed light");
            ImGui::SliderFloat("##shading", &emitter.meshShading, 0.0f, 1.0f, "%.2f");

            Row("Double sided", "turn on if the mesh looks inside out or has holes");
            ImGui::Checkbox("##doubleSided", &emitter.meshDoubleSided);
        }
        EndSection();
    }

    if (BeginSection("Texture", true)) {
        ParticleTextureSource source = static_cast<ParticleTextureSource>(emitter.activeTextureSource);
        const char* sourceNames[] = { "none, flat color", "set here", "from the entity's sprite", "from the model's material" };

        Row("Texture", "Browse sets a texture for this emitter, it wins over sprite and model textures");
        float thumbSize = ImGui::GetFrameHeight();
        if (emitter.activeTextureID != 0) {
            ImGui::Image((ImTextureID)textureManager->inspectTexture(emitter.activeTextureID), ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
        }
        std::string textureName = emitter.texturePath != "None" ? std::filesystem::path(emitter.texturePath).filename().string() : "(none)";
        float buttonsWidth = ImGui::CalcTextSize("BrowseClear").x + ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - buttonsWidth, 1.0f));
        ImGui::BeginDisabled();
        ImGui::InputText("##textureName", &textureName, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        if (emitter.texturePath != "None" && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", emitter.texturePath.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##texture")) {
            std::string path = Utils::fileDialog();
            if (!path.empty()) {
                emitter.texturePath = path;     // the renderer loads it on its next update
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##texture")) {
            emitter.texturePath = "None";
            emitter.loadedTexturePath = "None";
            emitter.textureID = 0;
        }

        Row("Drawing");
        ImGui::TextDisabled("%s", sourceNames[std::clamp(static_cast<int>(source), 0, IM_ARRAYSIZE(sourceNames) - 1)]);

        if (static_cast<ParticleRenderMode>(emitter.renderMode) == ParticleRenderMode::Mesh) {
            Row("Model texture", "use the model's albedo texture when none is set here");
            ImGui::Checkbox("##useModelTexture", &emitter.useModelTexture);
        }

        Row("Flipbook grid", "columns x rows of the sprite sheet, 1 x 1 for a single image");
        int grid[2] = { emitter.flipbookCols, emitter.flipbookRows };
        if (ImGui::DragInt2("##flipbookGrid", grid, 0.1f, 1, 64)) {
            emitter.flipbookCols = grid[0];
            emitter.flipbookRows = grid[1];
        }

        if (emitter.flipbookCols * emitter.flipbookRows > 1) {
            Row("Flipbook speed", "frames per second, 0 plays the sheet once over each lifetime");
            ImGui::DragFloat("##flipbookFPS", &emitter.flipbookFPS, 0.1f, 0.0f, 120.0f, emitter.flipbookFPS > 0.0f ? "%.1f fps" : "once per life");
        }
        EndSection();
    }

    ImGui::PopID();
}
