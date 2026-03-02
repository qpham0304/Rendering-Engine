#version 460

layout(location = 0) out vec4 outMoment;

void main() {
	float d = gl_FragCoord.z;
	float d2 = d * d;
	float d3 = d2 * d;
	float d4 = d3 * d;

	outMoment = vec4(d, d2, d3, d4);
}