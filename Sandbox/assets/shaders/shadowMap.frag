#version 460

layout(location = 0) out vec4 outMoment;

void main() {
	float d = gl_FragCoord.z;
    // d = d * 2.0 - 1.0;
	float d2 = d * d;
	float d3 = d2 * d;
	float d4 = d3 * d;

	outMoment = vec4(d, d2, d3, d4);

	// float warpedD = exp(2.0 * d) / exp(2.0);
	// outMoment = vec4(warpedD, warpedD*warpedD, warpedD*warpedD*warpedD, warpedD*warpedD*warpedD*warpedD);

}