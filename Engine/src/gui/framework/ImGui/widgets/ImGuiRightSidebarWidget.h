#pragma once
#include "../../src/gui/widgets/RightSidebarWidget.h"

class ImGuiRightSidebarWidget : public RightSidebarWidget
{
protected:
	bool popupOpen;
	unsigned int selectedTexture;
	void TextureModal(const ImTextureID& id);

public:
	ImGuiRightSidebarWidget();

	virtual void layersControl() override;
	virtual void textureView() override;
	virtual void environmentControl() override;
	virtual void render() override;
};

