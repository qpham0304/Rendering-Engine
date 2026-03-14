#pragma once

#include "../widgets/widget.h"

class RightSidebarWidget : public Widget
{
protected:
	RightSidebarWidget() : Widget("RightSidebarWidget") {}

public:
	virtual void layersControl() = 0;
	virtual void textureInspector() = 0;
	virtual void environmentControl() = 0;
	virtual void render() override = 0;
};

