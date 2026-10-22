#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "common/particles.glsl"

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec3 fragNormal;   // view space, zero for quads which are unlit
layout(location = 3) flat out int fragTextureID;
layout(location = 4) flat out float fragShading;

const vec2 quadCorners[6] = vec2[](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5),  vec2(-0.5, 0.5)
);

vec3 safeNormalize(vec3 v, vec3 fallback) {
    float len = length(v);
    return len > 1e-5 ? v / len : fallback;
}

void main() {
    Emitter e = pc.emittersRef.emitters[pc.containerIdx];
    Container c = pc.containersRef.containers[pc.containerIdx];

    // instances are the draw list the simulate pass wrote, the count comes from the indirect command
    uint particleID = DrawList(c.aliveListRef).e[gl_InstanceIndex].particleID;
    Particle p = ParticlesBuffer(c.particlesRef).p[particleID];

    float t = clamp(p.age / p.lifetime, 0.0, 1.0);
    float size = p.size * mix(1.0, e.sizeEndScale, t);

    vec3 center = p.position;
    vec3 velocity = p.velocity;
    if(e.localSpace != 0) {
        center = (e.transform * vec4(center, 1.0)).xyz;
        velocity = mat3(e.transform) * velocity;
    }

    vec4 color = mix(e.colorStart, e.colorEnd, t);
    if(e.fadeIn > 0.0) {
        color.a *= clamp(t / e.fadeIn, 0.0, 1.0);
    }
    if(e.fadeOut > 0.0) {
        color.a *= clamp((1.0 - t) / e.fadeOut, 0.0, 1.0);
    }

    vec3 camRight = ubo.invView[0].xyz;
    vec3 camUp = ubo.invView[1].xyz;
    vec3 toCamera = safeNormalize(ubo.cameraPos.xyz - center, -ubo.invView[2].xyz);

    vec3 worldPos;
    vec2 uv;
    fragNormal = vec3(0.0);

    if(e.renderMode == RENDER_MESH) {
        // non indexed draw over the index count, the index is fetched here
        uint index = Indices(e.meshIndicesRef).i[gl_VertexIndex];
        Vertex v = Vertices(e.meshVerticesRef).v[index];
        worldPos = center + rotateAxisAngle(v.pos * size, p.axis, p.rotation);
        uv = v.uv;
        fragNormal = mat3(ubo.view) * rotateAxisAngle(v.normal, p.axis, p.rotation);
    } else {
        vec2 corner = quadCorners[gl_VertexIndex % 6];
        uv = corner + vec2(0.5);

        float cr = cos(p.rotation);
        float sr = sin(p.rotation);
        vec2 rotated = vec2(corner.x * cr - corner.y * sr, corner.x * sr + corner.y * cr);

        switch (e.renderMode) {
            case RENDER_AXIS_Y: {
                vec3 up = vec3(0.0, 1.0, 0.0);
                vec3 right = safeNormalize(cross(up, toCamera), camRight);
                worldPos = center + (right * corner.x + up * corner.y) * size;
                break;
            }

            case RENDER_VELOCITY: {
                float speed = length(velocity);
                vec3 axis = speed > 1e-4 ? velocity / speed : camUp;
                vec3 right = safeNormalize(cross(axis, toCamera), camRight);
                float stretch = 1.0 + speed * e.stretchFactor;
                worldPos = center + (right * corner.x + axis * corner.y * stretch) * size;
                break;
            }

            case RENDER_HORIZONTAL: {
                worldPos = center + vec3(rotated.x, 0.0, rotated.y) * size;
                break;
            }

            case RENDER_ORIENTED: {
                vec3 right = rotateAxisAngle(safeNormalize(mat3(e.transform)[0], vec3(1.0, 0.0, 0.0)), p.axis, p.rotation);
                vec3 up = rotateAxisAngle(safeNormalize(mat3(e.transform)[1], vec3(0.0, 1.0, 0.0)), p.axis, p.rotation);
                worldPos = center + (right * corner.x + up * corner.y) * size;
                break;
            }

            default: {
                worldPos = center + (camRight * rotated.x + camUp * rotated.y) * size;
                break;
            }
        }
    }

    // flipbook, rows are counted from the top of the sheet like SpriteComponent
    int cols = max(e.flipbookCols, 1);
    int rows = max(e.flipbookRows, 1);
    int frames = cols * rows;
    if(frames > 1) {
        int frame = e.flipbookFPS > 0.0 ? int(p.age * e.flipbookFPS) % frames : min(int(t * float(frames)), frames - 1);
        vec2 cell = 1.0 / vec2(cols, rows);
        vec2 cellOffset = vec2(frame % cols, (rows - 1) - frame / cols) * cell;
        uv = cellOffset + clamp(uv, 0.0, 1.0) * cell;
    }

    fragTexCoord = uv;
    fragColor = color;
    fragTextureID = e.textureID;
    fragShading = e.meshShading;
    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);
}
