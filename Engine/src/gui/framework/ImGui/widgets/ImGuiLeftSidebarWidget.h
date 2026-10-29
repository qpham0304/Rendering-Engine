#pragma once

#include "../../../widgets/LeftSidebarWidget.h"
#include <imgui.h>

class Scene;

class ImGuiLeftSidebarWidget : public LeftSidebarWidget
{
public:
	ImGuiLeftSidebarWidget();

	virtual void AddComponentDialog(Entity& entity);
	virtual void ErrorModal(const char* message);
	virtual void AddItemButton(const std::string&& label = "+ Add");
	virtual void LightTab();
	virtual void EntityTab();
	virtual void ModelsTab();
	virtual void MeshesTab();
	virtual void ScenesTab();
	virtual void render() override;

private:
	enum class HierarchyFilter { All, Meshes, Bones };
	HierarchyFilter m_hierarchyFilter { HierarchyFilter::All };

	void _EntityTabMenu();
	void _EntityContent();
	void _SearchFilter();

	void _EntityNode(Entity& entity, Scene* scene, bool showChildren);
	void _ModelDetails(Entity& entity, Scene* scene);
	void _AnimationControls(Entity& entity);
	void _HierarchyFilterBar();
	void _HierarchyNode(Entity& entity, Scene* scene, const SkeletonManager::SkeletonAsset& skeleton, uint32_t nodeIndex, const std::vector<uint8_t>& subtreeFlags);
	void _MeshRow(Entity& entity, Scene* scene, uint32_t meshID);
	void _SelectMesh(Entity& entity, Scene* scene, uint32_t meshID);
	bool _IsEntitySelected(Scene* scene, Entity& entity);

	void _RenameMenuItem(Entity& entity);
	void _DuplicateMenuItem(Entity& entity);
	void _AddModelMenuItem(Entity& entity, std::string_view text);
	void _AddLightMenuItem(Entity& entity);
	void _AddCameraMenuItem(Entity& entity);
	void _LoadAnimationMenuItem(Entity& entity);
	void _DeleteEntityMenuItem(Entity &entity, Scene* scene);
};
