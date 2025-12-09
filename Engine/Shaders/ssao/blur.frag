#version 460 core
out float FragColor;
  
in vec2 uv;
  
uniform sampler2D ssaoInput;

const int numSamples = 2;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -numSamples; x < numSamples; x++) {
        for (int y = -numSamples; y < numSamples; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, uv + offset).r;
        }
    }
    FragColor = result / (4.0 * 4.0);
}  