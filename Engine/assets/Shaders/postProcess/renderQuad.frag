#version 460 core
out vec4 FragColor;

in vec2 uv;

uniform sampler2D scene;
uniform sampler2D effect;

void main() {
    // FragColor = texture(effect, uv) * texture(scene, uv);
    FragColor = mix(texture(effect, uv), texture(scene, uv), 0.9);
}