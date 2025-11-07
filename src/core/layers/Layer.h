#pragma once

#include <string>
#include "../../src/core/events/Event.h"

class LayerManager;

class Layer
{
public:
	bool m_Enabled;

public:
	//Layer() = default;
	Layer(const std::string& name = "default")
		: m_LayerName(name), m_Enabled(true), m_Manager(nullptr)
	{
	}

	virtual void onAttach(LayerManager* manager) { this->m_Manager = manager; };
	virtual void onDetach() = 0;
	virtual void onUpdate() = 0;
	virtual void onGuiUpdate() = 0;
	virtual void onEvent(Event& event) = 0;

	const std::string& GetName() const { 
		return m_LayerName; 
	}

protected:
	std::string m_LayerName;
	LayerManager* m_Manager;
};

