#pragma once

#include "../widgets/widget.h"

class ConsoleLogWidget : public Widget
{
protected:
	ConsoleLogWidget() : Widget() {}

public:

	virtual void render() = 0;
};

