#pragma once

#include "../widgets/widget.h"

class ConsoleLogWidget : public Widget
{
protected:
	ConsoleLogWidget() : Widget() {}
	virtual ~ConsoleLogWidget() override = default;
	
public:

	virtual void render() override = 0;
};

