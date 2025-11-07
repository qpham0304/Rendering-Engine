#include "ParticleGeometry.h"
#include "../src/graphics/utils/Utils.h"
#include "../../src/core/features/Timer.h"
#include "camera.h"
#include <glad/glad.h>

static const float g = 9.8f; // Gravity
static const float rho = 1.225f; // Air density
static const float Cd = 1.0f; // Drag coefficient
static const float cross_sectional_area = 0.5f; // Cross-sectional area
static const float dt = 0.016f; // Time step (assuming 60 FPS)

static glm::vec3 generateRandomDirection() {
    float theta = glm::radians(static_cast<float>(rand() % 360)); // Random angle in the xy-plane
    float phi = glm::radians(static_cast<float>(rand() % 180)); // Random angle for z-axis
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    return glm::normalize(glm::vec3(x, y, z));
}

static glm::vec3 generateRandomCircularDirection() {
    float angle = glm::radians(static_cast<float>(rand() % 360)); // Random angle in the XY plane
    float x = cos(angle);
    float y = sin(angle);
    return glm::normalize(glm::vec3(x, y, 0.0f)); // Z is 0 to keep it in the XY plane
}

float calculate_terminal_velocity(float mass, float cross_sectional_area) {
    float combined_constant = 2.0f * g / rho;
    return sqrt((combined_constant * mass) / (Cd * cross_sectional_area));
}

// Function to update velocity
float update_velocity(float current_velocity, float mass, float cross_sectional_area) {
    float terminal_velocity = calculate_terminal_velocity(mass, cross_sectional_area);

    if (current_velocity < terminal_velocity) {
        current_velocity += g * dt;

        float drag_force = 0.5f * rho * current_velocity * current_velocity * Cd * cross_sectional_area;
        float drag_acceleration = drag_force / mass;

        current_velocity -= drag_acceleration * dt;

        if (current_velocity > terminal_velocity) {
            current_velocity = terminal_velocity;
        }
    }

    return current_velocity;
}

ParticleGeometry::ParticleGeometry() {
    direction = glm::vec3(0.0, 0.0, 0.0);
    scale = glm::vec3(1.0);
    model = glm::mat4(1.0);
    upperBound = 0.0;
    lowerBound = 0.0;
    cubeVAO = 0;
    cubeVBO = 0;
    instanceVBO = 0;

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &instanceVBO);
}

void ParticleGeometry::init(const ParticleControl& control) {
    Timer timer("particle reinit time", false);
    scale = control.size;
    upperBound = control.upperBound;
    lowerBound = control.lowerBound;

    if (!matrixModels.empty()) {
        Console::println("Please clear particle before reinitialize");
    }
    else {
        upperBound = control.upperBound;
        lowerBound = control.lowerBound;
        // can preallocate an array given num Instances instead to avoid vector resize
        for (int i = 0; i < control.numInstances; i++) {    
            matrixModels.push_back(Utils::Random::createRandomTransform(control.spawnArea, scale));
            weights.push_back(Utils::Random::randomFloat(control.randomRange.x, control.randomRange.y));
            flyDirections.push_back(glm::vec3(0.0, 0.0, 0.0));
            glm::vec3 rand = generateRandomDirection();
            //glm::vec3 rand = generateRandomCircularDirection();
            randomDirs.push_back(rand);
        }
        if (firstInit) {
            glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_DYNAMIC_DRAW);

            glBindVertexArray(cubeVAO);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferData(GL_ARRAY_BUFFER, matrixModels.size() * sizeof(glm::mat4), &matrixModels[0], GL_DYNAMIC_DRAW);

            for (int i = 0; i < 4; i++) {
                glEnableVertexAttribArray(3 + i);
                glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
                glVertexAttribDivisor(3 + i, 1); // Set attribute divisor to 1 for instanced rendering
            }
            firstInit = false;
        }
        tempMatricies = matrixModels;
        glBindVertexArray(0);
    }
}

void ParticleGeometry::clear() {
    matrixModels.clear();
}

void ParticleGeometry::reset() {
    std::fill(flyDirections.begin(), flyDirections.end(), glm::vec3(0.0));
}

void ParticleGeometry::render(Shader& shader, Camera* camera, int& numRender, float& speed, bool& pause) {
    shader.Activate();
    shader.setMat4("mvp", camera->getMVP());
    shader.setVec3("lightColor", glm::vec3(0.7, 0.8, 1.0));

    if (!matrixModels.empty()) {
        glm::mat4 viewMatrix = camera->getViewMatrix();
        glm::vec3 camRight = glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
        glm::vec3 camUp = glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);

        glm::mat4 billboardMatrix;
        billboardMatrix[0] = glm::vec4(camRight, 0.0f);
        billboardMatrix[1] = glm::vec4(camUp, 0.0f);
        billboardMatrix[2] = glm::vec4(-glm::normalize(glm::cross(camRight, camUp)), 0.0f); // Forward vector
        billboardMatrix[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);                             // No translation

        //if (numRender >= numInstances) {
        //    Console::println("Cannot render more than the number of instances")
        //}

        for (unsigned int i = 0; i < numRender; i++) {
            if (!pause) {
                float velocity = speed * camera->getDeltaTime();
                //flyDirections[i].x -= weights[i] * velocity;
                flyDirections[i].y -= weights[i] * velocity;
                //flyDirections[i].z -= weights[i] * velocity;

                //if(!randomDirs.empty()) // randomdirs to generate pattern
                //    flyDirections[i] += randomDirs[i] * velocity;
            }
            if (flyDirections[i].y <= lowerBound || flyDirections[i].y >= upperBound) {
                flyDirections[i].x = 0.0;
                flyDirections[i].y = 0.0;
                flyDirections[i].z = 0.0;
            }
            direction = flyDirections[i];

            translateMatrix = glm::translate(glm::mat4(1.0), direction);
            tempMatricies[i] = translateMatrix * matrixModels[i];
            //shader.setMat4("matrix", model * billboardMatrix);
        }
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, numRender * sizeof(glm::mat4), &tempMatricies[0]);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glBindVertexArray(cubeVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, numRender);
        glBindVertexArray(0);
    }
    else {
        Console::println("No particle to render");
    }
}
