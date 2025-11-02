#version 460 core

#define FLT_MAX 3.402823466e+38
#define PI  3.14159265359
#define PI2 6.28318530717

struct Sphere {
    vec3 position;
    vec3 color;
    float radius;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};


const int NUM_SPHERES = 100;

out vec4 FragColor;
in vec2 uv;

uniform sampler2D colorScene;
uniform vec3 camPos = vec3(0.0, 0.0, 1.5);
uniform mat4 inViewMat;
uniform mat4 inProjMat;
uniform vec2 iResolution;
uniform Sphere spheres[NUM_SPHERES];
uniform float iTime;
uniform bool accumulateEnabled = true;
uniform int numBounces = 2;


vec3 lightPos = vec3(3.0, 3.0, 3.0);
vec3 lightDir = normalize(vec3(-1.0, -3.0, -1.0));
// vec4 sphereCenter = vec4(0.0, 0.0, 0.0, 0.5); // radius at w channel
vec2 localUV = uv * 2.0 - 1.0;
vec2 UV = vec2(0.0);

float seed = 0.0;
float random() {
	return fract(sin(dot(UV, vec2(12.9898, 78.233)) + seed++) * 43758.5453);
}


vec3 randomUnitVector() {
	float theta = random() * PI2;
    float z = random() * 2.0 - 1.0;
    float a = sqrt(1.0 - z * z);
    vec3 vector = vec3(a * cos(theta), a * sin(theta), z);
    return vector * sqrt(random());
}


float mySphereIntersect(vec3 rayOrigin, vec3 rayDir, out int index) {
    // (bx^2 + by^2 + bz^2)t^2 + (2(axbx + ayby + azbz))t + (ax^2 + ay^2 + az^2 - r^2) = 0
    float hitDistance = FLT_MAX;
    int closestSphereIndex = -1;

    for(int i = 0; i < NUM_SPHERES; i++) {
        vec3 origin = rayOrigin - spheres[i].position;
        float a = dot(rayDir, rayDir);
        float b = 2.0 * dot(origin, rayDir);
        float c = dot(origin, origin) - spheres[i].radius * spheres[i].radius;
        float discriminant = b*b - 4.0 * a * c;
        
        if(discriminant < 0.0) {
           continue;
        }

        // float t0 = (-b + sqrt(discriminant)) / (2.0 * a);   // back hit t
        float t1 = (-b - sqrt(discriminant)) / (2.0 * a);   // closest hit t

        if(t1 > 0.0 && t1 < hitDistance) {        
            hitDistance = t1;
            closestSphereIndex = i;
        }
    }
    
    index = closestSphereIndex;
    return hitDistance;
}

vec4 lighting(vec3 rayOrigin, vec3 rayDir) {
    vec3 ro = rayOrigin;
    vec3 rd = rayDir;

    vec4 finalColor = vec4(0.0);
    for(int i = 0; i < 2; i++) {
        int index;
        float closestT = mySphereIntersect(ro, rd, index);
        // if(closestT < 0.0) {
        //     return texture(colorScene, uv);
        // }
        
        if(index == -1) {
            // finalColor = texture(colorScene, uv);
            break;
        }

        vec3 origin = ro - spheres[index].position;
        vec3 worldPos = origin + rd * closestT;
        vec3 worldNormal = normalize(worldPos);

        worldPos += spheres[index].position;
        vec3 albedo = spheres[index].color;
        float diffuse = max(dot(worldNormal, -lightDir), 0.0);
        vec3 lighting = albedo * diffuse;
        finalColor += vec4(lighting, 1.0) * 0.7;

        ro = worldPos + worldNormal * 0.001;
        rd = reflect(rd, worldNormal);
    }
    return finalColor;
}

void main() {
    vec3 rayOrigin = camPos;
    vec4 target = inProjMat * vec4(localUV.x, localUV.y, 1.0 ,1.0);
    vec3 rayDir = vec3(inViewMat * vec4(normalize(target.xyz/target.w), 0.0));

    int index;
    float closestT = 0.0;//mySphereIntersect(rayOrigin, rayDir, index);


	FragColor = lighting(rayOrigin, rayDir);
}