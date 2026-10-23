#pragma once

struct ParticleEmitter;
struct RibbonComponent;
class TextureManager;
class ModelManager;

// the emitter controls, shared by the Components panel and the Particle Editor so both stay the same
namespace ImGuiParticleInspector {
    void draw(ParticleEmitter& emitter, TextureManager* textureManager, ModelManager* modelManager);
    void drawRibbon(RibbonComponent& ribbon, TextureManager* textureManager);
}
