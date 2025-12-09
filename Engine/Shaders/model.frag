#version 330 core
out vec4 FragColor;

in vec2 uv;
in float UseTexture;
in vec4 color;

uniform sampler2D diffuse;

void main() {  
    vec4 textureColor = texture(diffuse, uv);
    if(textureColor.a < 0.1) {
        discard;
    }

    FragColor = textureColor;
}