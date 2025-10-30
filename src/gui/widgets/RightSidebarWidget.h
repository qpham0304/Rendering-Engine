#pragma once

#include "../widgets/widget.h"

class RightSidebarWidget : public Widget
{
protected:
	RightSidebarWidget() : Widget() {};

public:

	virtual void layersControl() = 0;
	virtual void textureView() = 0;
	virtual void environmentControl() = 0;
	virtual void render() override = 0;
};

