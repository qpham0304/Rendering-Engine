#pragma once

#include <vector>
#include <memory>
#include "./widgets/widget.h"
#include "../../src/core/features/Configs.h"
class TransformComponent;

class GuiManager {
private:

protected:
	std::vector<std::unique_ptr<Widget>> widgets;
	bool darkTheme = false;
	bool closeable = true;
	int width = 0;
	int height = 0;
	int count = 0;
	bool GuizmoActive = false;
	bool drawGrid = false;

public:
	virtual void init(WindowConfig config) = 0;
	virtual void start() = 0;
	virtual void render() = 0;
	virtual void end() = 0;
	virtual void onClose() = 0;

	virtual void setTheme(bool darkTheme) = 0;
	virtual void useLightTheme() = 0;
	virtual void useDarkTheme() = 0;
	
	virtual void renderGuizmo(TransformComponent& transformComponent) = 0;
	virtual void guizmoTranslate() = 0;
	virtual void guizmoRotate() = 0;
	virtual void guizmoScale() = 0;
public:

};

