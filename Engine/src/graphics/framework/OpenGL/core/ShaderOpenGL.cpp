#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"

#include <glad/glad.h>

static bool validateFormat(const char* str) {
	size_t len = std::strlen(str);
	if (len >= 5)	// .frag and .vert length
		return std::strcmp(str + len - 5, ".vert") == 0 || std::strcmp(str + len - 5, ".frag") == 0;
	return false;
}

ShaderOpenGL::ShaderOpenGL(const char* vertexFile, const char* fragmentFile) : ID(0)
{
	try {
		ID = createShader(vertexFile, fragmentFile);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

ShaderOpenGL::ShaderOpenGL(const char* vertexFile, const char* fragmentFile, const char* geometryFile)  : ID(0)
{
	try {
		ID = createShader(vertexFile, fragmentFile, geometryFile);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

ShaderOpenGL::ShaderOpenGL() : ID(0)
{
	type = "";
}

ShaderOpenGL::~ShaderOpenGL()
{
	Delete();
}

void ShaderOpenGL::Init(const char* vertexFile, const char* fragmentFile)
{
	try {
		ID = createShader(vertexFile, fragmentFile);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

void ShaderOpenGL::Init(const char* vertexFile, const char* fragmentFile, const char* geometryFile)
{
	try {
		ID = createShader(vertexFile, fragmentFile, geometryFile);
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

// Activates the Shader Program
void ShaderOpenGL::Activate()
{
	glUseProgram(ID);
}

// Deletes the Shader Program
void ShaderOpenGL::Delete()
{
	glDeleteProgram(ID);
}

std::vector<std::string> ShaderOpenGL::split(const std::string& str) {
	std::istringstream iss(str);
	std::vector<std::string> tokens;
	std::string token;

	while (iss >> token) {
		tokens.push_back(token);
	}
	return tokens;
}

std::vector<UniformData> ShaderOpenGL::parseShaderUniforms(const std::string& content) {
	std::vector<std::string> tokens = split(content);
	std::vector<UniformData> uniforms;

	// Iterate through tokens to find uniform declarations
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i] == "uniform" && i + 2 < tokens.size()) {
			UniformData uniform;
			uniform.type = tokens[i + 1];
			uniform.name = tokens[i + 2];

			// Remove trailing semicolon from the name if present
			if (!uniform.name.empty() && uniform.name.back() == ';') {
				uniform.name.pop_back();
			}

			uniforms.push_back(uniform);
			i += 2; // Move past the type and name tokens
		}
	}

	return uniforms;
}

// Reads a text file and outputs a string with everything in the text file
std::string ShaderOpenGL::get_file_contents(const char* filepath)
{
	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "Error opening file: " << filepath << std::endl;
		return "";
	}

	//typedef std::istreambuf_iterator<char> charStreamIterator;
	using charStreamIterator = std::istreambuf_iterator<char>;
	std::string content((charStreamIterator(file)), (charStreamIterator()));
	file.close();

	return content;
}

// Checks if the different Shaders have compiled properly
void ShaderOpenGL::compileErrors(unsigned int shader, const char* type)
{
#define DEBUG
#ifdef DEBUG
	// Stores status of compilation
	int hasCompiled;
	// Character array to store error message in
	char infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::string msg = "SHADER_COMPILATION_ERROR for " + std::string(type) + ": " + infoLog;
			throw std::runtime_error(msg.c_str());
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &hasCompiled);
		if (hasCompiled == GL_FALSE)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::string msg = "SHADER_LINKING_ERROR for " + std::string(type) + ": " + infoLog;
			throw std::runtime_error(msg.c_str());
		}
	}
#else
	std::cout << "Ignore shader compile error message\n";
#endif
}

void ShaderOpenGL::reloadShader()
{
	try {
		unsigned int newID;
		if(!geomPath.empty())
			newID = createShader(vertPath.c_str(), fragPath.c_str(), geomPath.c_str());
		else
			newID = createShader(vertPath.c_str(), fragPath.c_str());
		glDeleteProgram(ID);
		this->ID = newID;
	}
	catch (const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
	}
}

unsigned int ShaderOpenGL::getUniformLocation(const std::string& name) const
{
	if (cache.find(name) != cache.end())
		return cache[name];
	unsigned int location = glGetUniformLocation(ID, name.c_str());
	cache[name] = location;
	return location;

}

unsigned int ShaderOpenGL::createShader(const char* vertexFile, const char* fragmentFile)
{
	printf("loading files: %s and %s\n", vertexFile, fragmentFile);
	
	//TODO properly validate format 
	//if false, use a default shader or something to prevent crash maybe?
	if (!(validateFormat(vertexFile) && validateFormat(fragmentFile))) {
		std::cerr << "invalid file format, must be .frag or .vert" << std::endl;
	}

	const char* fileName = std::strrchr(fragmentFile, '/');
	if (fileName != nullptr)
		type = fileName + 1;
	else
		type = fragmentFile;
	type = type.substr(0, type.size() - 5);

	vertPath = vertexFile;
	fragPath = fragmentFile;

	std::string vertexCode = get_file_contents(vertPath.c_str());
	std::string fragmentCode = get_file_contents(fragPath.c_str());

	uniforms = parseShaderUniforms(vertexCode + '\t' + fragmentCode);

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	compileErrors(vertexShader, "VERTEX");

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	compileErrors(fragmentShader, "FRAGMENT");

	unsigned int id = glCreateProgram();
	glAttachShader(id, vertexShader);
	glAttachShader(id, fragmentShader);
	glLinkProgram(id);
	compileErrors(fragmentShader, "PROGRAM");

	glDetachShader(ID, vertexShader);
	glDetachShader(ID, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return id;
}

unsigned int ShaderOpenGL::createShader(const char* vertexFile, const char* fragmentFile, const char* geometryFile)
{
	//TODO properly validate format 
	//if false, use a default shader or something to prevent crash maybe?
	if (!(validateFormat(vertexFile) && validateFormat(fragmentFile))) {
		std::cerr << "invalid file format, must be .frag or .vert" << std::endl;
	}

	const char* fileName = std::strrchr(fragmentFile, '/');
	if (fileName != nullptr)
		type = fileName + 1;
	else
		type = fragmentFile;
	type = type.substr(0, type.size() - 5);

	vertPath = vertexFile;
	fragPath = fragmentFile;
	geomPath = geometryFile;

	std::string vertexCode = get_file_contents(vertPath.c_str());
	std::string fragmentCode = get_file_contents(fragPath.c_str());
	std::string geometryCode = get_file_contents(geomPath.c_str());

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();
	const char* geometrySource = geometryCode.c_str();

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	compileErrors(vertexShader, "VERTEX");

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);
	compileErrors(fragmentShader, "FRAGMENT");

	unsigned int geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
	glShaderSource(geometryShader, 1, &geometrySource, NULL);
	glCompileShader(geometryShader);
	compileErrors(geometryShader, "GEOMETRY");

	unsigned int id = glCreateProgram();
	glAttachShader(id, vertexShader);
	glAttachShader(id, fragmentShader);
	glAttachShader(id, geometryShader);
	glLinkProgram(id);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glDeleteShader(geometryShader);

	return id;
}

void ShaderOpenGL::setBool(const std::string& name, bool value)
{
	glUniform1i(getUniformLocation(name), (int)value);
}

void ShaderOpenGL::setInt(const std::string& name, int value)
{
	glUniform1i(getUniformLocation(name), value);
}

void ShaderOpenGL::setFloat(const std::string& name, float value)
{
	glUniform1f(getUniformLocation(name), value);
}

void ShaderOpenGL::setVec2(const std::string& name, const glm::vec2& value)
{
	glUniform2fv(getUniformLocation(name), 1, &value[0]);
}
void ShaderOpenGL::setVec2(const std::string& name, float x, float y)
{
	glUniform2f(getUniformLocation(name), x, y);
}

void ShaderOpenGL::setVec3(const std::string& name, const glm::vec3& value)
{
	glUniform3fv(getUniformLocation(name), 1, &value[0]);
}
void ShaderOpenGL::setVec3(const std::string& name, float x, float y, float z)
{
	glUniform3f(getUniformLocation(name), x, y, z);
}

void ShaderOpenGL::setVec4(const std::string& name, const glm::vec4& value)
{
	glUniform4fv(getUniformLocation(name), 1, &value[0]);
}
void ShaderOpenGL::setVec4(const std::string& name, float x, float y, float z, float w)
{
	glUniform4f(getUniformLocation(name), x, y, z, w);
}

void ShaderOpenGL::setMat2(const std::string& name, const glm::mat2& mat)
{
	glUniformMatrix2fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void ShaderOpenGL::setMat3(const std::string& name, const glm::mat3& mat)
{
	glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}

void ShaderOpenGL::setMat4(const std::string& name, const glm::mat4& mat)
{
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}