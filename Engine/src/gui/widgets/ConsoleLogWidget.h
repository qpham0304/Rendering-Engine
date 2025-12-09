#pragma once

#include "../widgets/widget.h"

class ConsoleLogWidget : public Widget
{
protected:
	ConsoleLogWidget() = default;

public:

	virtual void render() = 0;
};

