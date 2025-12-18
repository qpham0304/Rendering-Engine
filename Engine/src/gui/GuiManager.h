#pragma once

#include <vector>
#include <memory>
#include "widgets/widget.h"
#include "core/features/Configs.h"
#include "services/Service.h"

class TransformComponent;

class GuiManager : public Service {
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

protected:
	GuiManager(std::string serviceName = "GuiManager") : Service(serviceName) {};

public:
	virtual bool init(WindowConfig config) override { return true; };
	virtual bool onClose() override { return true; };
	virtual void onUpdate() override {};
	
	virtual void start(void* handle = nullptr) = 0;
	virtual void render(void* handle = nullptr) = 0;
	virtual void end(void* handle = nullptr) = 0;

	virtual void setTheme(bool darkTheme) = 0;
	virtual void useLightTheme() = 0;
	virtual void useDarkTheme() = 0;
	
	virtual void renderGuizmo(TransformComponent& transformComponent) = 0;
	virtual void guizmoTranslate() = 0;
	virtual void guizmoRotate() = 0;
	virtual void guizmoScale() = 0;
public:

};

