#pragma once

#include "gui/widgets/ConsoleLogWidget.h"
#include <imgui.h>
#include <unordered_map>

class TextureManager;
class MeshManager;
class ModelManager;
class BufferManager;
class DescriptorManagerVulkan;
class MaterialManager;

class ImGuiConsoleLogWidget : public ConsoleLogWidget
{
public:
	ImGuiConsoleLogWidget();

	void render() override;
	
protected:
	bool scrollToBottom = false;

	TextureManager* textureManager{ nullptr };
	MeshManager* meshManager{ nullptr };
	ModelManager* modelManager{ nullptr };
	BufferManager* bufferManager{ nullptr };
	DescriptorManagerVulkan* descriptorManagerVulkan{ nullptr };
	MaterialManager* materialManager{ nullptr };
	
	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;

	//associated textureID and it's view descriptor set
	std::unordered_map<uint32_t, uint32_t> textureIDs;	

private:
	void _listTextureManager();
	void _listMeshManager();
	void _listModelManager();
	void _listBufferManager();
	void _listDescriptorManager();
	void _listMaterialManager();

	void _createViewDescriptorBind();
	uint32_t _createViewDescriptorSet(uint32_t id);
};

