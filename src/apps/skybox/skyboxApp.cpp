#include <iostream>
#include "shader.h"
#include "camera.h"
#include "imgui/imgui.h"
#include <vector>
#include "model.h"
#include <skybox.h>
#include "../../graphics/renderer/SkyboxRenderer.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processProgramInput(GLFWwindow* window);
void Rotate(glm::mat4 matrix, Shader& shader);
static const unsigned width = 1024;
static const unsigned height = 728;
glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);

float lastFrame = 0;
float rotationAngle = 0;
float deltaTime = 0;


// camera
Camera camera(width, height, glm::vec3(0.0f, 0.0f, 3.0f));
glm::vec3 camPos = camera.getPosition();

Camera* cameraController = nullptr;

std::vector<Vertex> cv = {
	// Front face
	Vertex{glm::vec3(-0.5f,-0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3( 0.5f,-0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3( 0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	Vertex{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f)},
	// Back face
	Vertex{glm::vec3(-0.5f,-0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3( 0.5f,-0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3( 0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
	Vertex{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)},
};

std::vector<unsigned int> cubeIndices = {
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


// set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
float cubeVertices[] = {
	// positions          // normals
	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
	-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
	-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
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

	// build and compile shaders
	// -------------------------
	Shader shader("Shaders/cubemap.vert", "Shaders/cubemap.frag");
	Shader skyboxShader("Shaders/skybox.vert", "Shaders/skybox.frag");

	//cube
	Shader cubeShader("Shaders/default.vert", "Shaders/default.frag");
	Mesh cube(cv, cubeIndices, { Texture("squish.png", "diffuse", "Textures") });
	glm::mat4 cubeMatrix = glm::mat4(1.0f);
	cubeMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, 0.25f, 0.25f));
	cubeMatrix = glm::translate(cubeMatrix, glm::vec3(3.0f, 2.0f, -1.0f));
	cubeShader.Activate();
	glUniformMatrix4fv(glGetUniformLocation(cubeShader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(cubeMatrix));
	glUniform4f(glGetUniformLocation(cubeShader.ID, "lightColor"), lightColor.x, lightColor.x, lightColor.x, lightColor.w);
	glUniform3f(glGetUniformLocation(cubeShader.ID, "lightPos"), lightPos.x, lightPos.x, lightPos.x);
	glUniform3f(glGetUniformLocation(cubeShader.ID, "camPos"), camPos.x, camPos.x, camPos.x);


	Shader modelShader("Shaders/cubemap.vert", "Shaders/cubemap.frag");
	glm::mat4 objMatrix = glm::mat4(1.0f);
	objMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, 0.25f, 0.25f));
	objMatrix = glm::translate(objMatrix, glm::vec3(-3.0f, 10.0f, -3.0f));
	objMatrix = glm::rotate(objMatrix, 90.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	modelShader.Activate();
	glUniformMatrix4fv(glGetUniformLocation(modelShader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(objMatrix));
	// glUniform4f(glGetUniformLocation(modelShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	modelShader.setInt("texture1", 0);
	Model ourModel("Models/planet/planet.obj");

	// cube VAO
	Texture texture("Textures/squish.png");
	unsigned int cubeVAO, cubeVBO;
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

	// shader configuration
	// --------------------
	shader.Activate();
	shader.setInt("texture1", 0);

	SkyboxRenderer skybox;
	skybox.setUniform();


	std::vector<std::tuple<Mesh, Shader>> meshes = {
		std::make_tuple(cube, cubeShader),
	};
	std::vector<std::tuple<Model, Shader>> models = {
		std::make_tuple(ourModel, modelShader),
	};

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		framebuffer_size_callback(window, width, height);
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// camera inputs
		camera.processInput(window);
		camera.cameraViewUpdate();
		glm::mat4 projection = camera.projectionMatrix();
		glm::mat4 viewMatrix = camera.getViewMatrix();
		glm::mat4 model = glm::mat4(1.0f);
		glm::mat4 mvp = projection * viewMatrix;

		for (const std::tuple<Mesh, Shader>& tuple : meshes) {
			Mesh m = std::get<0>(tuple);
			Shader s = std::get<1>(tuple);
			m.Draw(s, camera);
		}
		//Rotate(objMatrix, modelShader);

		for (const std::tuple<Model, Shader>& tuple : models) {
			Model m = std::get<0>(tuple);
			Shader s = std::get<1>(tuple);
			m.Draw(s, camera);
		}

		modelShader.Activate();
		modelShader.setMat4("model", objMatrix);
		modelShader.setMat4("mvp", mvp);
		modelShader.setVec3("camPos", camPos);
		ourModel.Draw(modelShader, camera);

		// cubes
		shader.Activate();
		shader.setMat4("model", model);
		shader.setMat4("mvp", mvp);
		shader.setVec3("camPos", camPos);
		glBindVertexArray(cubeVAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture.ID);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);


		skybox.render(camera);

		processProgramInput(window);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);	// Delete window before ending the program
	glfwTerminate();			// Terminate GLFW before ending the program
	return 0;
}

void processProgramInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
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

	glm::mat4 rotationMatrix = glm::rotate(matrix, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(rotationMatrix));

}