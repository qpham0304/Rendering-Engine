#pragma once

#include "../../../widgets/LeftSidebarWidget.h"
#include <imgui.h>

class Logger;
class ModelManager;
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
	Logger* m_logger;
	ModelManager* modelManager;

	void _EntityTabMenu();
	void _EntityContent();
	void _SearchFilter();

	void _RenameMenuItem(Entity& entity);
	void _AddModelMenuItem(Entity& entity, std::string_view text);
	void _AddLightMenuItem();
	void _AddCameraMenuItem();
	void _LoadAnimationMenuItem(Entity& entity);
	void _DeleteEntityMenuItem(Entity &entity, Scene* scene);
};

