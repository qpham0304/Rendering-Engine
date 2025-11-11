#pragma once

#include "../../src/core/events/Event.h"

class LayerManager;
class Logger;

class Layer
{
public:
	bool m_Enabled;

public:

	virtual void onAttach(LayerManager* manager);
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


protected:
	Layer(const std::string& name);
	
	void setLogScopeEngine();
	void setLogScopeClient();

	Logger& Log() const {
		return *m_Logger;
	}

private:
	Logger* m_Logger;

};

