#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

// color is left unpremultiplied so the combine pass blends over the scene with its alpha
void main() {
    outColor = fragColor;
}
