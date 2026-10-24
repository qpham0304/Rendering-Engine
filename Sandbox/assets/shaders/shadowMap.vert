#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(push_constant) uniform MeshData {
    mat4 lightMVP;
    ObjectsBuffer objRef;
    BoneMatrixArrayTable boneTableRef;
} push;

void main() {
    Object obj = push.objRef.objects[gl_InstanceIndex];

    // light probes don't cast shadows, push them outside the clip volume so the triangles get clipped
    if((obj.flags & OBJECT_LIGHT_PROBE) != 0u) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    Vertex vert = fetchVertex(obj, gl_VertexIndex);
    mat4 skinMatrix = getSkinMatrix(push.boneTableRef, obj.boneMatricesIdx, vert);

    gl_Position = push.lightMVP * obj.model * skinMatrix * vec4(vert.pos, 1.0);
}
