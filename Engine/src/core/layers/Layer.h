#pragma once

#include "core/events/Event.h"
#include "Logging/Logger.h"

class LayerManager;
// class Logger;

class Layer
{
public:
	bool m_Enabled;

public:
	virtual bool init() = 0;
	virtual void onAttach(LayerManager* manager);
	virtual void onDetach() = 0;
	virtual void onUpdate() = 0;
	virtual void onGuiUpdate() = 0;
	virtual void onEvent(Event& event) = 0;

	const std::string& getName() const;

protected:
	friend class LayerManager;

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
	uint32_t m_id;
};

