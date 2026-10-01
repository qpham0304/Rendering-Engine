#ifndef DEFINITIONS_GLSL
#define DEFINITIONS_GLSL

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_explicit_uniform_location : enable

const float PI = 3.14159265359;
const float GRAVITY = 9.18;
const vec3 G_FORCE = vec3(0.0, -GRAVITY, 0.0); 

#endif