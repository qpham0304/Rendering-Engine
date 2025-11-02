#version 460 core

#define MAX_WEIGHT 200

out vec4 FragColor;
in vec2 uv;

// uniform sampler2D prevScene;
uniform sampler2D rayTracedScene;

uniform int frameIndex;
uniform float iTime;

void main() {
    // vec4 oldScene = texture(prevScene, uv);
    vec4 newScene = texture(rayTracedScene, uv);

    if(frameIndex == 1) {
        FragColor = newScene;
        return;
    }
    FragColor = newScene / frameIndex;
    // FragColor = vec4(1.0,1.0,0.0,1.0);
}