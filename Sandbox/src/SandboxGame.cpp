#include "SandboxGame.h"
#include "SandboxLayer.h"
#include <core/Engine.h>

// owned by the engine's LayerManager once pushed, kept here only to remove it again on unload
static SandBoxLayer* gameLayer = nullptr;

void wkGameLoad(Engine& engine)
{
	gameLayer = new SandBoxLayer("Sandbox Layer");
	engine.pushLayer(gameLayer);
}

void wkGameUnload(Engine& engine)
{
	if (gameLayer) {
		engine.removeLayer(gameLayer);
		gameLayer = nullptr;
	}
}
