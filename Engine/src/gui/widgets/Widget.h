#pragma once

#include "gui/Themes/IconsFontAwesome5.h"
#include "core/features/MathIncludes.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/MaterialManager.h" 
#include "core/resources/managers/BufferManager.h" 
#include "logging/logger.h"

class Widget {
public:
    virtual ~Widget() = default;
	virtual void render() = 0;
    virtual void update() {};

    const char* getName() { return m_name.c_str(); }
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }

protected:
    Widget(std::string name = "widget") : m_isVisible(true), m_name(name), m_firstFrame(true) {

        //TODO: might be better to move to a cpp file 
        m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
        textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
        modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
        materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
        bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    };

    Widget(const Widget& other) = default;
    Widget(Widget&& other) noexcept = default;
    virtual Widget& operator=(const Widget& other) = default;
    virtual Widget& operator=(Widget&& other) noexcept = default;

protected:
    bool m_isVisible;
    bool m_firstFrame;
    std::string m_name;
    Logger* m_logger;
    
    TextureManager* textureManager{ nullptr };
    MeshManager* meshManager{ nullptr };
    ModelManager* modelManager{ nullptr };
    MaterialManager* materialManager{ nullptr };
    BufferManager* bufferManager{ nullptr };
};