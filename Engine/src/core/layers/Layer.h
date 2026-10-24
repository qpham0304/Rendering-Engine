#pragma once

#include "Engine/EngineCoreAPI.h"
#include "core/events/Event.h"
#include "Logging/Logger.h"
#include "core/features/FrameTime.h"
#include "core/events/EventSubscription.h"
#include <vector>
#include "core/features/Object.h"

class LayerManager;

class MYGRAPHICSENGINE_EXPORT Layer : public Object
{
public:
	bool m_Enabled;

public:
	virtual ~Layer() = default;
	Layer(const Layer&) = delete;
	Layer& operator=(const Layer&) = delete;

	virtual bool init() = 0;
	virtual void onAttach(LayerManager* manager);
	virtual void onDetach() = 0;
	virtual void onFixedUpdate(float fixedDeltaTime) {}
	virtual void onUpdate(const FrameTime& frameTime) = 0;
	virtual void onGuiUpdate() = 0;
	virtual void onEvent(Event& event) = 0;

	const std::string& getName() const;

protected:
	friend class LayerManager;

	std::string m_LayerName;
	LayerManager* m_Manager;
	std::vector<EventSubscription> m_eventSubscriptions;


protected:
	Layer(std::string_view name = "Undefined");
	
	void setLogScopeEngine();
	void setLogScopeClient();

	Logger& Log() const {
		return *m_Logger;
	}

private:
	Logger* m_Logger;
	uint32_t m_id;
};

