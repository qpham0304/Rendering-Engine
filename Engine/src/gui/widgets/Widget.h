#pragma once

#include <iostream>
#include <imgui.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>
#include "src/gui/widgets/IconsFontAwesome5.h"

class Widget {
protected:
    bool showWidget;

    Widget() : showWidget(true) {};
    Widget(const Widget& other) = default;
    Widget(Widget&& other) noexcept = default;
    virtual Widget& operator=(const Widget& other) = default;
    virtual Widget& operator=(Widget&& other) noexcept = default;

public:
    virtual ~Widget() = default;
	virtual void render() = 0;
};