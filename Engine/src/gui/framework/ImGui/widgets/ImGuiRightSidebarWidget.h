#pragma once
#include "gui/widgets/RightSidebarWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

class DescriptorManagerVulkan;
class Scene;
class Logger;
class Entity;

class ImGuiRightSidebarWidget : public RightSidebarWidget
{
public:
	ImGuiRightSidebarWidget();

	virtual void layersControl() override;
	virtual void textureInspector() override;
	virtual void environmentControl() override;
	virtual void render() override;

protected:
	bool popupOpen;
	unsigned int selectedTexture;
	void TextureModal(const ImTextureID& id);

private:
	Scene* scene{ nullptr };
	Logger* m_logger;

	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;

	void _listTextureManager();

	//TODO: maybe reflection can help avoiding manual modification for every component?
	void _componentsControl();
	void _nameControl(const Entity& entity);
	void _transformControl(const Entity& entity);
	void _modelControl(const Entity& entity);
	void _meshControl(const Entity& entity);
};

