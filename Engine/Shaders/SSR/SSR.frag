#version 460 core

uniform sampler2D gNormal;      //scene normal
uniform sampler2D gAlbedo;  //scene color
uniform sampler2D gSpecular;
uniform sampler2D depthMap; 
uniform sampler2D colorBuffer; 

uniform mat4 view;
uniform mat4 projection;
uniform mat4 invView;
uniform mat4 invProjection;
uniform int width;
uniform int height;

in vec2 uv;

out vec4 FragColor;

const float step = 0.1;
const float minRayStep = 0.1;   // stepsize
const float maxSteps = 100;      // num steps
const int numBinarySearchSteps = 10;

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 hash(vec3 a) {
    vec3 Scale =  vec3(.8, .8, .8);
    float K = 19.19;
    a = fract(a * Scale);
    a += dot(a, a.yxz + K);
    return fract((a.xxy + a.yxx)*a.zyx);
}

vec3 generatePositionFromDepth(vec2 texturePos, float depth) {
	vec4 ndc = vec4((texturePos - 0.5) * 2, depth, 1.f);
	vec4 inversed = invProjection * ndc;// going back from projected
	inversed /= inversed.w;
	return inversed.xyz;
}

vec2 generateProjectedPosition(vec3 pos){
	vec4 samplePosition = projection * vec4(pos, 1.f);
	samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
	return samplePosition.xy;
}


vec3 getCurrentViewPos(float z, vec2 coord) {
    vec2 posCanonical  = coord * 2 - 1.0; //position in Canonical View Volume
	vec4 posView = invProjection * vec4(posCanonical, z , 1.0);
	posView /= posView.w;
	return posView.xyz;
}

vec3 getCurrentViewPos2(float curDepth, vec2 coord) {
    float depth = texture(depthMap, uv).r * 2 - 1;
    
    vec2 posCanonical  = uv * 2 - 1.0; //position in Canonical View Volume
	vec4 posView = invProjection * vec4(posCanonical, curDepth , 1.0);
	posView /= posView.w;

    return vec3(1.0);
}

vec3 BinarySearch(inout vec3 dir, inout vec3 hitCoord, inout float dDepth) {
    float depth;

    vec4 projectedCoord;
    for(int i = 0; i < numBinarySearchSteps; i++) {
        projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
 
        float depth = texture(depthMap, projectedCoord.xy).r * 2 - 1;
        vec3 viewSpacePosition = getCurrentViewPos(depth, projectedCoord.xy);
        depth = viewSpacePosition.z;
        dDepth = hitCoord.z - depth;

        dir *= 0.5;
        if(dDepth > 0.0)
            hitCoord += dir;
        else
            hitCoord -= dir;    
    }

    projectedCoord = projection * vec4(hitCoord, 1.0);
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
 
    return vec3(projectedCoord.xy, depth);
}

vec2 rayCast(vec3 dir, inout vec3 hitCoord, out float dDepth) {
    dir *= step;

    for (int i = 0; i < maxSteps; i++) {
        hitCoord += dir;

        vec4 projectedCoord = projection * vec4(hitCoord, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5; 

        float depth = texture(depthMap, projectedCoord.xy).r * 2 - 1;
        vec3 viewSpacePosition = getCurrentViewPos(depth, projectedCoord.xy);
        depth = viewSpacePosition.z;
        dDepth = hitCoord.z - depth;

        if((dir.z - dDepth) < 1.2 && dDepth <= 0.0) return BinarySearch(dir, hitCoord, dDepth).xy;
    }

    return vec2(-1.0);
}

vec4 RayMarch(vec3 dir, inout vec3 position, out float dDepth) {
    dir *= step;
 
    float depth;
    vec4 projectedCoord;
 
    for(int i = 0; i < maxSteps; i++) {
        position += dir;
        
        projectedCoord = projection * vec4(position, 1.0);
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
 
        float depth = texture(depthMap, projectedCoord.xy).r * 2 - 1;
        vec3 viewSpacePosition = getCurrentViewPos(depth, projectedCoord.xy);
        depth = viewSpacePosition.z;
        if(depth > 1000.0)
            continue;
 
        dDepth = position.z - depth;

        if((dir.z - dDepth) < 1.2) {
            if(dDepth <= 0.0) {   
                vec4 Result = vec4(BinarySearch(dir, position, dDepth), 1.0);
                return Result;
            }
        }
    }
    return vec4(projectedCoord.xy, depth, 0.0);
}

vec3 SSR(vec3 position, vec3 reflection) {
	float rayStep = 0.2f;
    float distanceBias = 0.05f;
    bool debugDraw = false;
    bool isBinarySearchEnabled = true;
    bool isAdaptiveStepEnabled = true;
    bool isExponentialStepEnabled = true;

    vec3 step = rayStep * reflection;
	vec3 marchingPosition = position + step;
	float delta;
	float depthFromScreen;
	vec2 screenPosition;
	
	for (int i = 0; i < maxSteps; i++) {
		screenPosition = generateProjectedPosition(marchingPosition);
		depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(depthMap, screenPosition).x).z);
		delta = abs(marchingPosition.z) - depthFromScreen;
		if (abs(delta) < distanceBias) {
			vec3 color = vec3(1);
			if(debugDraw)
				color = vec3( 0.5+ sign(delta)/2,0.3,0.5- sign(delta)/2);
			return texture(colorBuffer, screenPosition).xyz * color;
		}
		if (isBinarySearchEnabled && delta > 0) {
			break;
		}
		if (isAdaptiveStepEnabled){
			float directionSign = sign(abs(marchingPosition.z) - depthFromScreen);
			//this is sort of adapting step, should prevent lining reflection by doing sort of iterative converging
			//some implementation doing it by binary search, but I found this idea more cheaty and way easier to implement
			step = step * (1.0 - rayStep * max(directionSign, 0.0));
			marchingPosition += step * (-directionSign);
		}
		else {
			marchingPosition += step;
		}
        if (isExponentialStepEnabled){
			step *= 1.05;
		}
    }
    if(isBinarySearchEnabled){
		for(int i = 0; i < maxSteps; i++){
			step *= 0.5;
			marchingPosition = marchingPosition - step * sign(delta);
			
			screenPosition = generateProjectedPosition(marchingPosition);
			depthFromScreen = abs(generatePositionFromDepth(screenPosition, texture(depthMap, screenPosition).x).z);
			delta = abs(marchingPosition.z) - depthFromScreen;
			
			if (abs(delta) < distanceBias) {
                vec3 color = vec3(1);
                if(debugDraw)
                    color = vec3(0.5 + sign(delta)/2,0.3,0.5 - sign(delta)/2);
				return texture(colorBuffer, screenPosition).xyz * color;
			}
		}
	}
	
    return vec3(0.0);
}

float random (vec2 uv) {
	return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123); //simple random function
}

// /*
void main() {
    vec4 viewSpaceNormal = texture(gNormal, uv);
    // vec4 worldSpaceNormal = invView * viewSpaceNormal;
    
    // float depth = texture(depthMap, uv).r * 2 - 1;
	// vec3 viewSpacePosition = getCurrentViewPos(depth, uv);
	vec3 viewSpacePosition = generatePositionFromDepth(uv, texture(depthMap, uv).x);
    // vec3 worldSpacePosition = mat3(invView) * viewSpacePosition;

    float Metallic = normalize(viewSpaceNormal.a); //reflection mask in alpha channel
    if(Metallic == 0.0) {
        FragColor = texture(colorBuffer, uv);
        return;
    }

    // vec4 gSpec = texture(gSpecular, uv);
    // if(gSpec.a > 0.0) {
    //     return;
    // }

    vec3 albedo = texture(gAlbedo, uv).rgb;
    vec3 reflectedDir = normalize(reflect(viewSpacePosition, viewSpaceNormal.xyz)); // Reflection vector
    if(false) {
        float sampleCount = 4;
        vec3 firstBasis = normalize(cross(vec3(0.f, 0.f, 1.f), reflectedDir));
        vec3 secondBasis = normalize(cross(reflectedDir, firstBasis));
        vec4 resultingColor = vec4(0.f);
        for (int i = 0; i < sampleCount; i++) {
            vec2 coeffs = vec2(random(uv + vec2(0, i)) + random(uv + vec2(i, 0))) * 0.02;
            vec3 reflectionDirectionRandomized = reflectedDir + firstBasis * coeffs.x + secondBasis * coeffs.y;
            vec3 tempColor = SSR(viewSpacePosition, normalize(reflectionDirectionRandomized));
            if (tempColor != vec3(0.f)) {
                resultingColor += vec4(tempColor, 1.f);
            }
        }
        if (resultingColor.w == 0){
            FragColor = texture(colorBuffer, uv);
        } else {
            resultingColor /= resultingColor.w;
            FragColor = vec4(resultingColor.xyz, 1.f);
        }
    }

    else {
        FragColor = vec4(SSR(viewSpacePosition, normalize(reflectedDir)), 1.f);
        if (FragColor.xyz == vec3(0.f)){
            FragColor = texture(colorBuffer, uv);
        }
    }


    // vec3 hitPos = viewSpacePosition;
    // float dDepth;
    // vec2 coords = rayCast(reflectedDir * max(-viewSpacePosition.z, minRayStep), hitPos, dDepth);
    
    // float d = texture(depthMap, coords.xy).r * 2 - 1;
    // vec3 vsPos = getCurrentViewPos(d, coords.xy);
    // float L = length(vsPos - viewSpacePosition);
    // vec3 color = texture(colorBuffer, coords.xy).rgb;
    
    // if (coords.xy != vec2(-1.0)) {
    //     FragColor = mix(texture(colorBuffer, uv), vec4(color, 1.0), Metallic);
    //     return;
    // }
    // FragColor = mix(texture(colorBuffer, uv), vec4(1.0, 0.0, 0.0, 1.0), Metallic);

    // if(reflectedDir.z > 0){
    //     FragColor = vec4(0,0,0,1);
    //     return;
    // }
    // float dDepth = 0.0f;
    // vec3 viewPosition = viewSpacePosition;
    // vec4 coords = RayMarch(reflectedDir * max(minRayStep, -viewSpacePosition.z), viewPosition, dDepth);
    // vec2 dCoords = smoothstep(0.2, 0.6, abs(vec2(0.5) - coords.xy));
    
    // const float fallOffExponent = 3.0;
    // float screenEdgefactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);
    // float ReflectionMultiplier = pow(Metallic, fallOffExponent) * screenEdgefactor * -reflectedDir.z;
    // vec3 SSR = texture(colorBuffer, coords.xy).rgb;  

    // vec3 F0 = vec3(0.04); 
    // F0 = mix(F0, albedo, Metallic);
    // vec3 Fresnel = fresnelSchlick(max(dot(normalize(viewSpaceNormal.xyz), normalize(viewSpacePosition)), 0.0), F0);
    // SSR *= Fresnel;
    // FragColor = vec4(SSR, 1.0);
}

// */

// bool rayIsOutofScreen(vec2 ray){
// 	return (ray.x > 1 || ray.y > 1 || ray.x < 0 || ray.y < 0) ? true : false;
// }

// vec3 TraceRay(vec3 rayPos, vec3 dir, int iterationCount){
// 	float sampleDepth;
// 	vec3 hitColor = vec3(0);
// 	bool hit = false;

// 	for(int i = 0; i < iterationCount; i++){
// 		rayPos += dir;
// 		if(rayIsOutofScreen(rayPos.xy)){
// 			break;
// 		}

// 		sampleDepth = texture(depthMap, rayPos.xy).r;
// 		float depthDif = rayPos.z - sampleDepth;
//         //TODO: figure out how to perform binary search when we miss or to avoid holes
// 		if(depthDif >= 0 && depthDif < 0.00001){ //we have a hit
// 			hit = true;
// 			hitColor = texture(colorBuffer, rayPos.xy).rgb;
// 			break;
// 		}
// 	}
// 	return hitColor;
// }

// void main() {
// 	float maxRayDistance = 100.0f;

//     vec4 viewSpaceNormal = texture(gNormal, uv);
//     float depth = texture(depthMap, uv).r * 2 - 1;
// 	vec3 viewSpacePosition = getCurrentViewPos(depth, uv);

//     float Metallic = normalize(viewSpaceNormal.a); //reflection mask in alpha channel
//     if(Metallic == 0.0) {
//         FragColor = texture(colorBuffer, uv);
//         return;
//     }

//     vec4 gSpec = texture(gSpecular, uv);
//     if(gSpec.a > 0.0) {
//         discard;
//     }

// 	vec3 viewspaceNormal = texture(gNormal, uv).rgb;	
// 	float pixelDepth = texture(depthMap, uv).r;	// 0< <1
    	
// 	vec4 positionView = vec4(viewSpacePosition, 1.0);;
// 	positionView /= positionView.w;
// 	vec3 reflectedDir = normalize(reflect(positionView.xyz, viewspaceNormal));
// 	if(reflectedDir.z > 0){
// 		FragColor = vec4(0,0,0,1);
// 		return;
// 	}
// 	vec3 rayEndPositionView = positionView.xyz + reflectedDir * maxRayDistance;

// 	//Texture Space ray calculation
// 	vec4 rayEndPositionTexture = projection * vec4(rayEndPositionView,1);
// 	rayEndPositionTexture /= rayEndPositionTexture.w;
// 	rayEndPositionTexture.xyz = (rayEndPositionTexture.xyz + vec3(1)) / 2.0f;
	
//     vec3 pixelPositionTexture = vec3(uv, pixelDepth);
// 	vec2 screenSpaceStartPosition = vec2(pixelPositionTexture.x * width, pixelPositionTexture.y * height); 
// 	vec2 screenSpaceEndPosition = vec2(rayEndPositionTexture.x * width, rayEndPositionTexture.y * height); 
// 	vec2 screenSpaceDistance = screenSpaceEndPosition - screenSpaceStartPosition;
// 	int screenSpaceMaxDistance = int(max(abs(screenSpaceDistance.x), abs(screenSpaceDistance.y)) / 2);
//     vec3 rayDirectionTexture = rayEndPositionTexture.xyz - pixelPositionTexture;
//     rayDirectionTexture /= max(screenSpaceMaxDistance, 0.001f);

// 	vec3 outColor = TraceRay(pixelPositionTexture, rayDirectionTexture, screenSpaceMaxDistance);
// 	FragColor = vec4(outColor, 1.0f);
// }