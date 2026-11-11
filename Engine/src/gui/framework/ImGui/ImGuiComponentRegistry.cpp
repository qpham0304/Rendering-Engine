#include "ImGuiComponentRegistry.h"

#include "core/scene/EntitySerializer.h"
#include <imgui.h>

void ImGuiComponentRegistry::draw(Entity entity)
{
    for (const Entry& entry : m_entries) {
        if (!entry.has(entity)) {
            continue;
        }

        bool keep = true;
        ImGui::PushID(entry.typeName.c_str());
        if (ImGui::CollapsingHeader(entry.label, entry.removable ? &keep : nullptr, ImGuiTreeNodeFlags_DefaultOpen)) {
            entry.draw(entity);
        }
        ImGui::PopID();

        if (!keep) {
            _remove(entry, entity);
        }
    }
}

void ImGuiComponentRegistry::drawAddMenu(Entity entity)
{
    for (const Entry& entry : m_entries) {
        if (entry.addable && !entry.has(entity) && ImGui::Selectable(entry.label)) {
            _add(entry, entity);
        }
    }
}

void ImGuiComponentRegistry::_add(const Entry& entry, Entity entity)
{
    const auto& factory = EntitySerializer::getComponentFactory();
    auto loader = factory.find(entry.typeName);
    if (loader == factory.end()) {
        entry.emplace(entity);
        return;
    }
    loader->second(entity, nlohmann::json::object())();
}

void ImGuiComponentRegistry::_remove(const Entry& entry, Entity entity)
{
    const auto& destroyer = EntitySerializer::getComponentDestroyer();
    auto unloader = destroyer.find(entry.typeName);
    if (unloader == destroyer.end()) {
        entry.remove(entity);
        return;
    }
    unloader->second(entity);
}
