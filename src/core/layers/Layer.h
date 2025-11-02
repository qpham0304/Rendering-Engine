#pragma once

#include <string>
#include "../../src/core/events/Event.h"

class LayerManager;

class Layer
{
protected:
	std::string m_LayerName;
	LayerManager* manager;

public:
	bool m_Enabled;
	
	Layer(const std::string& name = "default")
		: m_LayerName(name), m_Enabled(true), manager(nullptr)
	{
	}

	virtual void OnAttach(LayerManager* manager) { this->manager = manager; };
	virtual void OnDetach() = 0;
	virtual void OnUpdate() = 0;
	virtual void OnGuiUpdate() = 0;
	virtual void OnEvent(Event& event) = 0;

	const std::string& GetName() const { 
		return m_LayerName; 
	}
};

