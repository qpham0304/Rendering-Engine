#pragma once

#include "widget.h"
#include "EntityControlWidget.h"
#include <vector>
#include <string>
#include <unordered_map>
#include "../../core/entities/Entity.h"

class LeftSidebarWidget : public Widget
{

protected:
	std::vector<std::string> nodes;
	size_t selectedIndex;
	Entity* selectedEntity;
	std::string selectedModel;
	bool errorPopupOpen = false;

	LeftSidebarWidget() : Widget() {}

public:

	virtual void AddComponentDialog(Entity& entity) = 0;
	virtual void ErrorModal(const char* message) = 0;
	virtual void AddItemButton(const std::string&& label = "+ Add") = 0;
	virtual void LightTab() = 0;
	virtual void EntityTab() = 0;
	virtual void ModelsTab() = 0;
	virtual void MeshesTab() = 0;
	virtual void render() override = 0;
};

