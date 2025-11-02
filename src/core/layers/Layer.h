#pragma once

#include <string>
#include "../../src/core/events/Event.h"

class Layer
{
protected:
	std::string m_LayerName;

public:
	bool m_Enabled;
	
	Layer(const std::string& name = "default")
		: m_LayerName(name), m_Enabled(true) 
	{
	}

	virtual void OnAttach() = 0;
	virtual void OnDetach() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnGuiUpdate() = 0;
	virtual void OnEvent(Event& event) = 0;

	const std::string& GetName() const { 
		return m_LayerName; 
	}
};

