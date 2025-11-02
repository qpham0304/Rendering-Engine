#version 460 core

#define FLT_MAX 3.402823466e+38
#define PI  3.14159265359
#define PI2 6.28318530717
#define MAX_WEIGHT 200
#define LAMB 0
#define METAL 1
#define DIEL 2
#define EMISSIVE 3

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float materialIndex;
    float emissive;
};

struct Sphere {
    vec3 position;
    // vec3 color;
    float radius;
    Material material;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Hit {
    float hitDistance;
    vec3 worldPos;
    vec3 worldNormal;
    int index;
};

const int NUM_SPHERES = 100;

out vec4 FragColor;
in vec2 uv;

uniform sampler2D colorScene;
uniform samplerCube envMap;
uniform sampler2D prevScene;

uniform vec3 camPos = vec3(0.0, 0.0, 1.5);
uniform mat4 inViewMat;
uniform mat4 inProjMat;
uniform vec2 iResolution;
uniform Sphere spheres[NUM_SPHERES];
uniform float iTime;
uniform int numBounces = 2;
uniform int frameIndex;
uniform bool skyboxEnabled = false;


vec3 lightPos = vec3(3.0, 3.0, 3.0);
vec3 lightDir = normalize(vec3(-1.0, -1.0, -1.0));
// vec4 sphereCenter = vec4(0.0, 0.0, 0.0, 0.5); // radius at w channel

float seed = iTime;

// https://www.shadertoy.com/view/ldtSR2
float random() {
	return fract(sin(dot(uv, vec2(12.9898, 78.233)) + seed++) * 43758.5453);
}

vec3 randomUnitVector() {
	float theta = random() * PI2;
    float z = random() * 2.0 - 1.0;
    float a = sqrt(1.0 - z * z);
    vec3 vector = vec3(a * cos(theta), a * sin(theta), z);
    vector *= sqrt(random());
    // vector *= random(); // modified to reduce the frequency(less spread)
    return vector;
}

Hit miss(Ray ray) {
    Hit hit;
    hit.hitDistance = -1.0;
    return hit;
}

Hit closestHit(Ray ray, float hitDistance, int index) {
    Hit hit;
    hit.hitDistance = hitDistance;
    hit.index = index;
    
    vec3 origin = ray.origin - spheres[index].position;
    hit.worldPos = origin + ray.direction * hitDistance;
    hit.worldNormal = normalize(hit.worldPos);
    hit.worldPos += spheres[index].position; // shifted for to display sphere so bring back ths original pos

    return hit;
}

Hit traceRay(Ray ray) {
    // (bx^2 + by^2 + bz^2)t^2 + (2(axbx + ayby + azbz))t + (ax^2 + ay^2 + az^2 - r^2) = 0
    float hitDistance = FLT_MAX;
    int closestSphereIndex = -1;

    for(int i = 0; i < NUM_SPHERES; i++) {
        vec3 origin = ray.origin - spheres[i].position;
        float a = dot(ray.direction, ray.direction);
        float b = 2.0 * dot(origin, ray.direction);
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
    if(closestSphereIndex < 0) {
        return miss(ray);
    }
    
    return closestHit(ray, hitDistance, closestSphereIndex);
}

vec3 schlick() {
    return vec3(0.0);
}

vec3 GGX() {
    return vec3(0.0);
}



void main() {
    vec2 localUV = uv * 2.0 - 1.0;
    vec3 rayOrigin = camPos;
    vec4 target = inProjMat * vec4(localUV.x, localUV.y, 1.0 ,1.0);
    vec3 rayDir = vec3(inViewMat * vec4(normalize(target.xyz/target.w), 0.0));

    Ray ray;
    ray.origin = rayOrigin;
    ray.direction = rayDir;
    vec4 finalColor = vec4(0.0);
    vec3 contribution = vec3(1.0);

    // an extra run from outside to blend the sphere background
    // otherwise skybox texture will overwrite the reflection miss on the spheres
    Hit payload = traceRay(ray);
    if(payload.hitDistance < 0.0) {
        // finalColor = texture(envMap, ray.direction);
        // finalColor = texture(colorScene, uv);
        // finalColor += texture(envMap, ray.direction) * vec4(contribution, 1.0);
    } else {
        vec3 albedo = spheres[payload.index].material.albedo;
        // vec3 ambient = albedo * 0.3;
        float diffuse = max(dot(payload.worldNormal, -lightDir), 0.0);

        // vec3 lighting = albedo * diffuse;// + ambient;
        // finalColor += vec4(lighting, 1.0);
        contribution *= albedo;
    }

    for(int i = 1; i <= numBounces; i++) {
        Hit payload = traceRay(ray);

        if(payload.hitDistance < 0.0) {
            // finalColor = texture(envMap, ray.direction);
            if(skyboxEnabled)
                finalColor += texture(envMap, ray.direction) * vec4(contribution, 1.0);
            // finalColor += vec4(1.0);
            break;
        } 

        vec3 albedo = spheres[payload.index].material.albedo;
        float diffuse = max(dot(payload.worldNormal, -lightDir), 0.0);
        // finalColor += vec4(albedo * diffuse, 1.0);
        contribution *= albedo;
        finalColor += vec4(spheres[payload.index].material.albedo, 1.0) * (spheres[payload.index].material.emissive);


        vec3 offset = spheres[payload.index].material.roughness * randomUnitVector();
        ray.origin = payload.worldPos + payload.worldNormal * 0.001f;
        ray.direction = reflect(ray.direction, payload.worldNormal + offset);
    }
    
    
    FragColor = clamp(finalColor, 0.0, 1.0);
}