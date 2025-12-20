#pragma once

#include "../../../widgets/LeftSidebarWidget.h"
#include <imgui.h>

class Logger;
class ModelManager;

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
};

