#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include "shader.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "texture.h"
#include "camera.h"
#include "Triangle.h"
#include "imgui/imgui.h"
#include <vector>
#include <Mesh.h>
#include "model.h"
#include <skybox.h>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processProgramInput(GLFWwindow* window);
void Rotate(glm::mat4 matrix, Shader& shader);
static const unsigned width = 1024;
static const unsigned height = 728;
float lastFrame = 0;
float rotationAngle = 0;
float deltaTime = 0.0f;

glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
Camera* cameraController = nullptr;

// Indices for vertices order
std::vector<GLuint> indices =
{
	0, 1, 2, // Bottom side
	0, 2, 3, // Bottom side
	4, 6, 5, // Left side
	7, 9, 8, // Non-facing side
	10, 12, 11, // Right side
	13, 15, 14 // Facing side
};

GLfloat lightVertices[] =
{ //     COORDINATES     //
	-0.1f, -0.1f,  0.1f,
	-0.1f, -0.1f, -0.1f,
	 0.1f, -0.1f, -0.1f,
	 0.1f, -0.1f,  0.1f,
	-0.1f,  0.1f,  0.1f,
	-0.1f,  0.1f, -0.1f,
	 0.1f,  0.1f, -0.1f,
	 0.1f,  0.1f,  0.1f
};

GLuint lightIndices[] =
{
	0, 1, 2,
	0, 2, 3,
	0, 4, 7,
	0, 7, 3,
	3, 7, 6,
	3, 6, 2,
	2, 6, 5,
	2, 5, 1,
	1, 5, 4,
	1, 4, 0,
	4, 5, 6,
	4, 6, 7
};

float cubeVertices[] = {
	// positions		color					UV 			 normals
	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
	 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
	 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
	 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
	-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f,
	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,

	-0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
	 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,

	-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
	-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
	-0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
	-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,

	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
	 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
	 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
	 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,

	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
	 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f,
	 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
	 0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
	-0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
	-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,

	-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f,  1.0f, 0.0f,
	-0.5f,  0.5f,  0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.0f,  1.0f, 0.0f,
	-0.5f,  0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f,  1.0f, 0.0f
};

std::vector<Vertex> cubeMeshVertices = {
	// Front face
	Vertex{glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3(0.5f,-0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	// Back face
	Vertex{glm::vec3(-0.5f,-0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3(0.5f,-0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
};

std::vector<unsigned int> cubeMeshIndices = {
	// Front face
	0, 1, 2,
	2, 3, 0,
	// Right face
	1, 5, 6,
	6, 2, 1,
	// Back face
	7, 6, 5,
	5, 4, 7,
	// Left face
	4, 0, 3,
	3, 7, 4,
	// Bottom face
	4, 5, 1,
	1, 0, 4,
	// Top face
	3, 2, 6,
	6, 7, 3
};

float skyboxVertices[] = {
	// positions          
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f,  1.0f
};


struct Material {
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	float shininess;
};

struct Light {
	glm::vec3 position;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
};

unsigned int loadCubemap(std::vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	int width, height, nrChannels;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
			stbi_image_free(data);
		}
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}


int main() {
	glfwInit();

	// Tell GLFW what version of OpenGL we are using 
	// In this case we are using OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	// Tell GLFW we are using the CORE profile
	// So that means we only have the modern functions
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "Graphic Engine", NULL, NULL);
	// Error check if the window fails to create
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	// Introduce the window into the current context
	glfwMakeContextCurrent(window);

	gladLoadGL();
	glViewport(0, 0, width, height);

	glEnable(GL_DEPTH_TEST);

	Camera camera(width, height, glm::vec3(0.0f, 0.5f, 3.0f));
	
	glm::vec3 lightPos = glm::vec3(1.0f, 0.5f, 1.5f);
	glm::vec3 lightAmbient = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::vec3 lightDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
	glm::vec4 lightSpecular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	Light light{
		lightPos, 
		lightAmbient,
		lightDiffuse,
		lightSpecular,
	};


	//cube
	Shader cubeShader("Shaders/material.vert", "Shaders/material.frag");
    // cube VAO
	unsigned int cubeVAO, cubeVBO;
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

	glm::mat4 cubeModel = glm::mat4(1.0f);
	glm::mat4 normalMatrix = glm::transpose(glm::inverse(cubeModel));
	cubeShader.Activate();
	cubeShader.setMat4("matrix", glm::scale(cubeModel, glm::vec3(0.5f, 0.5f, 0.5f)));
	cubeShader.setVec3("lightColor", lightColor);
	cubeShader.setVec3("light.position", lightPos);
	cubeShader.setVec3("viewPos", camera.position);
	cubeShader.setMat3("normalMatrix", normalMatrix);

	// light properties
	lightColor.x = static_cast<float>(sin(glfwGetTime() * 2.0));
	lightColor.y = static_cast<float>(sin(glfwGetTime() * 0.7));
	lightColor.z = static_cast<float>(sin(glfwGetTime() * 1.3));
	glm::vec3 diffuseColor = lightColor * glm::vec3(0.5f); // decrease the influence
	glm::vec3 ambientColor = diffuseColor * glm::vec3(0.5f); // low influence
	cubeShader.setVec3("light.ambient", ambientColor);
	cubeShader.setVec3("light.diffuse", diffuseColor);
	cubeShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
	cubeShader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
	cubeShader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
	cubeShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
	cubeShader.setFloat("material.shininess", 32.0f);

	Shader cubeMeshShader("Shaders/material.vert", "Shaders/material.frag");
	Mesh cubeMesh(cubeMeshVertices, cubeMeshIndices, { Texture("Textures/metal.png", "diffuse", GL_TEXTURE0, GL_RGBA, GL_UNSIGNED_BYTE) });
	glm::mat4 cubeMeshModel = glm::scale(cubeModel, glm::vec3(0.5f, 0.5f, 0.5f));
	cubeMeshModel = glm::translate(cubeMeshModel, glm::vec3(2.0f, 1.0f, 0.0f));
	cubeMeshShader.Activate();
	cubeMeshShader.setMat4("matrix", cubeMeshModel);
	cubeMeshShader.setVec3("lightColor", lightColor);
	cubeMeshShader.setVec3("light.position", lightPos);
	cubeMeshShader.setVec3("viewPos", camera.position);
	cubeMeshShader.setMat3("normalMatrix", normalMatrix);

	// light properties for cubeMesh
	cubeMeshShader.setVec3("light.ambient", ambientColor);
	cubeMeshShader.setVec3("light.diffuse", diffuseColor);
	cubeMeshShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
	cubeMeshShader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
	cubeMeshShader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
	cubeMeshShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
	cubeMeshShader.setFloat("material.shininess", 32.0f);

	//cube2
	Shader cube2Shader("Shaders/material.vert", "Shaders/material.frag");
	// cube VAO
	unsigned int cube2VAO, cube2VBO;
	glGenVertexArrays(1, &cube2VAO);
	glGenBuffers(1, &cube2VBO);
	glBindVertexArray(cube2VAO);
	glBindBuffer(GL_ARRAY_BUFFER, cube2VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));

	glm::mat4 cube2Model = glm::mat4(1.0f);
	glm::mat4 normalMatrix2 = glm::transpose(glm::inverse(cube2Model));
	cube2Shader.Activate();
	cube2Model = glm::translate(cube2Model, glm::vec3(2.0f, 0.5f, 2.0f));
	cube2Model = glm::scale(cube2Model, glm::vec3(0.6f, 0.6f, 0.6f));
	cube2Shader.setMat4("matrix", cube2Model);
	cube2Shader.setVec3("lightColor", lightColor);
	cube2Shader.setVec3("light.position", lightPos);
	cube2Shader.setVec3("viewPos", camera.position);
	cube2Shader.setMat3("normalMatrix", normalMatrix2);

	// light properties for cubeMesh2
	cube2Shader.setVec3("light.ambient", ambientColor);
	cube2Shader.setVec3("light.diffuse", diffuseColor);
	cube2Shader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
	cube2Shader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
	cube2Shader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
	cube2Shader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
	cube2Shader.setFloat("material.shininess", 32.0f);


	//light
	Shader lightShader("Shaders/light.vert", "Shaders/light.frag");
	VAO lightVAO;
	lightVAO.Bind();
	VBO lightVBO(lightVertices, sizeof(lightVertices));
	EBO lightEBO(lightIndices, sizeof(lightIndices));
	lightVAO.LinkAttrib(lightVBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void*)0);
	lightVAO.Unbind();
	lightVBO.Unbind();
	lightEBO.Unbind();

	glm::mat4 lightModel = glm::mat4(1.0f);
	lightModel = glm::translate(lightModel, lightPos);
	lightShader.Activate();
	glUniformMatrix4fv(glGetUniformLocation(lightShader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(lightModel));
	glUniform3f(glGetUniformLocation(lightShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z);

	//skybox
	std::vector<std::string> faces = {
		"Textures/night-skybox/right.jpg",
		"Textures/night-skybox/left.jpg",
		"Textures/night-skybox/top.jpg",
		"Textures/night-skybox/bottom.jpg",
		"Textures/night-skybox/front.jpg",
		"Textures/night-skybox/back.jpg"
	};


	Shader skyboxShader("Shaders/skybox.vert", "Shaders/skybox.frag");
	Skybox skybox(faces);


	GLuint skyboxVAO, skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	Texture text("Textures/squish.png");
	cubeShader.Activate();
	cubeShader.setInt("diffuse0", 0);

	text.Bind();
	text.texUnit(cubeShader, "diffuse0", 0);

	Texture text2("Textures/planks.png");
	cube2Shader.Activate();
	cube2Shader.setInt("diffuse0", 0);

	text2.Bind();
	text2.texUnit(cube2Shader, "diffuse0", 0);

	
	skyboxShader.Activate();
	glUniform1i(glGetUniformLocation(skyboxShader.ID, "skybox"), 0);

	std::vector<std::tuple<Mesh, Shader>> meshes = {
		std::make_tuple(cubeMesh, cubeMeshShader),
	};

	std::vector<std::tuple<Model, Shader>> models = {
		
	};

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		framebuffer_size_callback(window, width, height);
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// camera inputs
		camera.processInput(window);
		

		//mesh
		for (const std::tuple<Mesh, Shader>& tuple : meshes) {
			Mesh m = std::get<0>(tuple);
			Shader s = std::get<1>(tuple);
			m.Draw(s, camera);
			Rotate(cubeMeshModel, s);
		}

		//model
		for (const std::tuple<Model, Shader>& tuple : models) {
			Model m = std::get<0>(tuple);
			Shader s = std::get<1>(tuple);
			m.Draw(s, camera);
		}

		//cube
		cubeShader.Activate();
		camera.cameraViewObject(cubeShader.ID, "mvp");
		glBindVertexArray(cubeVAO);
		glActiveTexture(GL_TEXTURE0);
		text.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		Rotate(cubeModel, cubeShader);

		//cube2
		cube2Shader.Activate();
		camera.cameraViewObject(cube2Shader.ID, "mvp");
		glBindVertexArray(cube2VAO);
		glActiveTexture(GL_TEXTURE0);
		text2.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		//Rotate(cube2Model, cube2Shader);

		//light
		lightShader.Activate();
		camera.cameraViewObject(lightShader.ID, "mvp");
		lightVAO.Bind();
		glDrawElements(GL_TRIANGLES, sizeof(lightIndices) / sizeof(int), GL_UNSIGNED_INT, 0);
		// light properties updates
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		lightColor.x = static_cast<float>(sin(glfwGetTime() * 2.0));
		lightColor.y = static_cast<float>(sin(glfwGetTime() * 0.7));
		lightColor.z = static_cast<float>(sin(glfwGetTime() * 1.3));
		diffuseColor = lightColor * glm::vec3(0.5f); // decrease the influence
		ambientColor = diffuseColor * glm::vec3(0.5f); // low influence
		
		// update properties for cubeShader
		cube2Shader.setVec3("light.ambient", ambientColor);
		cube2Shader.setVec3("light.diffuse", diffuseColor);
		cube2Shader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
		cube2Shader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
		cube2Shader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
		cube2Shader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
		cube2Shader.setFloat("material.shininess", 32.0f);

		//skybox
		glDepthFunc(GL_LEQUAL);				// change depth function so depth test passes when values are equal to depth buffer's content
		skyboxShader.Activate();
		glm::mat4 view = camera.view;
		glm::mat4 projection = camera.projection;
		view = glm::mat4(glm::mat3(view));
		skyboxShader.setMat4("view", view);
		skyboxShader.setMat4("projection", projection);
		
		glBindVertexArray(skyboxVAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.ID);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glDepthMask(GL_TRUE);
		glBindVertexArray(0);
		glDepthFunc(GL_LESS); // set depth function back to default

		processProgramInput(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// Delete window before ending the program
	glfwDestroyWindow(window);
	// Terminate GLFW before ending the program
	glfwTerminate();
	return 0;
}

void processProgramInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

void Rotate(glm::mat4 matrix, Shader& shader) {
	float currentFrame = glfwGetTime();
	float deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	float rotationSpeed = 1;
	rotationAngle += deltaTime * rotationSpeed;

	glm::mat4 rotationMatrix = glm::rotate(matrix, rotationAngle, glm::vec3(0.0f, 1.0f, 1.0f));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(rotationMatrix));
	shader.setMat3("normalMatrix", glm::transpose(glm::inverse(rotationMatrix)));

}



