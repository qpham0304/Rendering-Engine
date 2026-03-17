#pragma once

#include <vector>
#include <span>
#include <memory>
#include "widgets/widget.h"
#include "core/features/Configs.h"
#include "services/Service.h"

class TransformComponent;

class GuiManager : public Service 
{
protected:
	std::vector<std::shared_ptr<Widget>> widgets;

	bool darkTheme { false };
	bool closeable { true };
	int width { 0 };
	int height { 0 };
	int count { 0 };
	bool drawGrid { false };
	bool guizmoActive { false };
	bool editorActive { false };
	bool guiActive { false };

protected:
	GuiManager(std::string serviceName = "GuiManager") : Service(serviceName) {};

public:
	virtual bool init(WindowConfig config) override = 0;
	virtual bool onClose() override = 0;
	virtual void onUpdate() override = 0;
	
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

	template<typename T, typename... Args> requires std::derived_from<T, Widget>
	void addWidget(Args&&... args) {
		widgets.push_back(std::make_shared<T>(std::forward<Args>(args)...));
	};

	std::span<std::shared_ptr<Widget>> getWidgets() {
		return widgets;
	}

	void setGuizmoFocus(bool isFocused) { guizmoActive = isFocused; };
	bool isGuizmoFocus() { return guizmoActive; };

	void setEditorFocus(bool isFocused) {editorActive = isFocused; };
	bool isEditorFocus() { return editorActive; };
	
	void setActive(bool active) {guiActive = active; };
	bool isActive() { return guiActive; };
};

