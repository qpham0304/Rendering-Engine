#version 460 core

out vec4 FragColor;

in vec2 uv;

uniform sampler2D blackSceneTexture;
uniform sampler2D sceneTexture;
uniform mat4 projectionMatrix;
uniform vec2 lightPos2D;

uniform float decay = 0.96815;
uniform float exposure = 0.2;
uniform float density = 0.926;
uniform float weight = 0.58767;

void main() {
    int NUM_SAMPLES = 100;
    vec2 tc = uv;
    vec2 deltaTexCoord = (tc - lightPos2D);
    deltaTexCoord *= density / float(NUM_SAMPLES);
    float illuminationDecay = 1.0;
    vec4 color = texture(blackSceneTexture, tc) * 0.4;
    for(int i = 0; i < NUM_SAMPLES; i++) {
        tc -= deltaTexCoord;
        vec4 smple = texture(blackSceneTexture, tc) * 0.4;
        smple *= illuminationDecay * weight;
        color += smple;
        illuminationDecay *= decay;
    }

    vec4 scene = texture(sceneTexture, uv);
    FragColor = color * scene;
}