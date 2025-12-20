#pragma once
#include "gui/widgets/RightSidebarWidget.h"
#include <imgui.h>

class DescriptorManagerVulkan;

class ImGuiRightSidebarWidget : public RightSidebarWidget
{
public:
	ImGuiRightSidebarWidget();

	virtual void layersControl() override;
	virtual void textureView() override;
	virtual void environmentControl() override;
	virtual void render() override;

protected:
	bool popupOpen;
	unsigned int selectedTexture;
	void TextureModal(const ImTextureID& id);

private:
	//associated textureID and it's view descriptor set
	std::unordered_map<uint32_t, uint32_t> textureIDs;	
	DescriptorManagerVulkan* descriptorManagerVulkan{ nullptr };
	uint32_t imGuilayoutID;
	uint32_t imGuipoolID;


	void _listTextureManager();
	void _createViewDescriptorBind();
	uint32_t _createViewDescriptorSet(uint32_t id);

};

