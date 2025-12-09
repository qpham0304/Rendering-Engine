#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>
#include<glm/glm.hpp>
#include<unordered_map>
#include<cstring>

struct UniformData {
	std::string type;
	std::string name;
};

class ShaderOpenGL
{
public:
	// Reference ID of the Shader Program
	unsigned int ID;
	std::string type;

public:
	ShaderOpenGL();
	ShaderOpenGL(const char* vertexFile, const char* fragmentFile);
	ShaderOpenGL(const char* vertexFile, const char* fragmentFile, const char* geometryFile);
	//Shader(const Shader& other);
	//Shader& operator=(const Shader& other);
	//Shader(Shader&& other) noexcept;
	//Shader& operator=(Shader&& other) noexcept;
	~ShaderOpenGL();

	void Init(const char* vertexFile, const char* fragmentFile);
	void Init(const char* vertexFile, const char* fragmentFile, const char* geometryFile);
	void Activate();
	void Delete();
	void compileErrors(unsigned int shader, const char* type);
	void reloadShader();

	// utility uniform functions
	void setBool(const std::string& name, bool value);
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat2(const std::string& name, const glm::mat2& mat);
    void setMat3(const std::string& name, const glm::mat3& mat);
    void setMat4(const std::string& name, const glm::mat4& mat);

private:
	static std::string get_file_contents(const char* filename);
	static std::vector<std::string> split(const std::string& str);
	static std::vector<UniformData> parseShaderUniforms(const std::string& content);

	unsigned int getUniformLocation(const std::string& name) const;
	unsigned int createShader(const char* vertexFile, const char* fragmentFile);
	unsigned int createShader(const char* vertexFile, const char* fragmentFile, const char* geometryFile);

private:
	mutable std::unordered_map<std::string, unsigned int> cache;
	std::vector<UniformData> uniforms;

	std::string vertPath;
	std::string fragPath;
	std::string geomPath;
	std::string tessPath;
};

#endif