#include <iostream>
#include <imgui/imgui.h>
#include "Mesh.h"
#include "model.h"
#include <FrameBuffer.h>
#include "../../core/components/s/PlaneComponent.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processProgramInput(GLFWwindow* window);
void Rotate(glm::mat4 matrix, Shader& shader);
static const std::string DIR = "";
static const unsigned width = 1024;
static const unsigned height = 728;
float lastFrame = 0;
float rotationAngle = 0;
Camera* cameraController = nullptr;

float rectangleVertices[] =
{
	// Coords    // texCoords
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f,

	 1.0f,  1.0f,  1.0f, 1.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	-1.0f,  1.0f,  0.0f, 1.0f
};

int main() {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(width, height, "Graphic Engine", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	gladLoadGL();
	glViewport(0, 0, width, height);


	Camera camera(width, height, glm::vec3(0.0f, 0.5f, 3.0f));
	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	glm::vec3 lightPos = glm::vec3(0.5f, 4.5f, 0.5f);
	Light light = Light(lightPos, lightColor, 0.5f);

	Shader modelShader("Shaders/model.vert", "Shaders/model.frag");
	glm::mat4 objMatrix = glm::mat4(1.0f);
	objMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, 0.25f, 0.25f));
	objMatrix = glm::translate(objMatrix, glm::vec3(12.0f, 0.0f, 0.0f));
	modelShader.Activate();
	glUniformMatrix4fv(glGetUniformLocation(modelShader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(objMatrix));
	glUniform4f(glGetUniformLocation(modelShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(modelShader.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
	Model ourModel("Models/reimu/reimu.obj");

	//cube
	Shader cubeShader("Shaders/model.vert", "Shaders/model.frag");
	Model cube("Models/cube/cube.obj");
	glm::mat4 cubeMatrix = glm::mat4(1.0f);
	cubeMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, 0.25f, 0.25f));
	cubeMatrix = glm::translate(cubeMatrix, glm::vec3(2.0f, 2.0f, -1.0f));
	cubeShader.Activate();
	glUniformMatrix4fv(glGetUniformLocation(cubeShader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(cubeMatrix));
	glUniform4f(glGetUniformLocation(cubeShader.ID, "lightColor"), lightColor.x, lightColor.y, lightColor.z, lightColor.w);
	glUniform3f(glGetUniformLocation(cubeShader.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

	PlaneComponent plane;

	glEnable(GL_DEPTH_TEST);

	std::vector<std::tuple<Model, Shader>> models = {
		std::make_tuple(ourModel, modelShader),
		std::make_tuple(cube, cubeShader),
	};

	unsigned int rectVAO, rectVBO;
	glGenVertexArrays(1, &rectVAO);
	glGenBuffers(1, &rectVBO);
	glBindVertexArray(rectVAO);
	glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


	FrameBuffer framebuffer(width, height);

	Shader frameShaderProgram("src/apps/frame-buffer/framebuffer.vert", "src/apps/frame-buffer/framebuffer.frag");
	frameShaderProgram.Activate();
	frameShaderProgram.setFloat("screenTexture", 0);
	




	// Main while loop
	while (!glfwWindowShouldClose(window))
	{	
		// camera inputs
		camera.processInput(window);
		camera.cameraViewUpdate();
		framebuffer_size_callback(window, width, height);

		framebuffer.Bind();
		glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);

		for (const std::tuple<Model, Shader>& tuple : models) {
			Model m = std::get<0>(tuple);
			Shader s = std::get<1>(tuple);
			s.Activate();
			glUniform3f(glGetUniformLocation(s.ID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
			m.Draw(s, camera);
		}

		plane.render(camera, light);

		framebuffer.Unbind();
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		frameShaderProgram.Activate();
		glBindVertexArray(rectVAO);
		glDisable(GL_DEPTH_TEST); // prevents framebuffer rectangle from being discarded
		glBindTexture(GL_TEXTURE_2D, framebuffer.texture);
		glDrawArrays(GL_TRIANGLES, 0, 6);

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

	glm::mat4 rotationMatrix = glm::rotate(matrix, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, "matrix"), 1, GL_FALSE, glm::value_ptr(rotationMatrix));

}


