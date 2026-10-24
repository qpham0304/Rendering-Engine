#pragma once

#include "../widgets/widget.h"

class ConsoleLogWidget : public Widget
{
protected:
	ConsoleLogWidget() : Widget("ConsoleLogWidget") { setFootprint("ConsoleLogWidget", sizeof(ConsoleLogWidget), MemorySystem::Gui); }
	virtual ~ConsoleLogWidget() override = default;
	
public:
	virtual void render() override = 0;

};

