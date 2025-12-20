#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include "gui/Themes/IconsFontAwesome5.h"
#include "core/features/serviceLocator.h"

#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/DescriptorManager.h"
#include "core/resources/managers/MaterialManager.h" 

// class TextureManager;
// class MeshManager;
// class ModelManager;
// class MaterialManager;

class Widget {
public:
    virtual ~Widget() = default;
	virtual void render() = 0;

protected:
    Widget() : showWidget(true) {
        //TODO: might be better to move to a cpp file 
        textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
        modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
        materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    };
    Widget(const Widget& other) = default;
    Widget(Widget&& other) noexcept = default;
    virtual Widget& operator=(const Widget& other) = default;
    virtual Widget& operator=(Widget&& other) noexcept = default;

protected:
    bool showWidget;

    TextureManager* textureManager{ nullptr };
    MeshManager* meshManager{ nullptr };
    ModelManager* modelManager{ nullptr };
    MaterialManager* materialManager{ nullptr };
};