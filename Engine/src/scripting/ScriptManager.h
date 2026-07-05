#pragma once

#include "core/resources/managers/Manager.h"

class Entity;

class ScriptManager : public Manager
{
public:
	ScriptManager(std::string serviceName = "ScriptManager") : Manager(serviceName) {};	
	virtual ~ScriptManager() = default;

	virtual bool init(WindowConfig config) = 0;
    virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual std::vector<uint32_t> listIDs() const = 0;
    virtual void onUpdate() = 0;
	virtual void loadScript(Entity& entity, std::string_view path) = 0;
	virtual void loadScript(std::string_view path) = 0;
	virtual void reloadScript(std::string_view path) = 0;
	virtual void runScript(std::string_view path) = 0;

protected:
	virtual void _reloadScript(const std::string& path) = 0;
	std::vector<std::string> m_scriptsToReload;
};