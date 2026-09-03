#pragma once
#include "gui/widgets/RightSidebarWidget.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include "core/events/Event.h"
#include <future>

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
	virtual void update() override;

protected:
	bool popupOpen;
	bool errorPopupOpen;
	unsigned int selectedTexture;
	void TextureModal(const ImTextureID& id);
	AsyncEvent asyncE;
	uint32_t tempID;
	Entity* myEntt;
	std::future<uint32_t> m_loadingFuture;
	std::vector<std::pair<const Mesh*, MaterialDesc>> m_meshesToUpdate;

private:
	Scene* scene{ nullptr };
	Logger* m_logger;

	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;

	void _listTextureManager();
	void _addModelDialog(Entity& entity);
	void errorModal(const char* message);
	void textInput(std::string* text, std::string message);

	//TODO: maybe reflection can help avoiding manual modification for every component?
	void _componentsControl();
	void _nameControl(const Entity& entity);
	void _transformControl(const Entity& entity);
	void _modelControl(const Entity& entity);
	void _meshControl(const Entity& entity);
	void _spriteControl(const Entity& entity);
	void _scriptControl(const Entity& entity);
	void _colliderControl(const Entity& entity);
	void _scenesControl();
};

