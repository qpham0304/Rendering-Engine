#pragma once

#include "Sandbox/SandboxAPI.h"

class Engine;

// game module entry points, the runtime exe calls them directly, the editor resolves them from the dll by name for hot reload
extern "C" {
	SANDBOX_EXPORT void wkGameLoad(Engine& engine);
	SANDBOX_EXPORT void wkGameUnload(Engine& engine);
}
