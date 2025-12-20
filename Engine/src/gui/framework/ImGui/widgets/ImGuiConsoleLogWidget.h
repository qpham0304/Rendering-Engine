#pragma once

#include "gui/widgets/ConsoleLogWidget.h"
#include <imgui.h>
#include <unordered_map>

// class TextureManager;
// class MeshManager;
// class ModelManager;
// class BufferManager;
// class MaterialManager;
class DescriptorManagerVulkan;

class ImGuiConsoleLogWidget : public ConsoleLogWidget
{
public:
	ImGuiConsoleLogWidget();

	void render() override;
	
protected:
	bool scrollToBottom = false;


private:
};

