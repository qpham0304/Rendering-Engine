#pragma once

#include "scripting/ScriptManager.h"
#include <sol/sol.hpp>

class ScriptManagerLua : public ScriptManager
{
public:
	ScriptManagerLua(std::string serviceName = "ScriptManager");	
	virtual ~ScriptManagerLua() override;

	virtual bool init(WindowConfig config) override;
    virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
    virtual void onUpdate() override;
	virtual void loadScript(Entity& entity, std::string_view path) override;
	virtual void loadScript(std::string_view path) override;
	virtual void reloadScript(std::string_view path) override;
	virtual void runScript(std::string_view path) override;

protected:
	sol::state m_luaState;
	std::unordered_map<std::string, sol::protected_function> m_scripts;
	std::unordered_map<std::string, sol::table> m_scriptCache;
	
	virtual void _reloadScript(const std::string& path) override;
};