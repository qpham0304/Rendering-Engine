#include"TextureOpenGL.h"
#include <stb/stb_image.h>
#include<glad/glad.h>
#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"


void TextureOpenGL::loadTexture(const char* path, bool flip)
{
	glGenTextures(1, &m_id);

	int width, height, nrComponents;
	stbi_set_flip_vertically_on_load(flip);
	//TODO: stb loading task can be done in a separate thread, create the texture and populate later
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (!data) {
		std::cout << "Texture load failed: " << path << std::endl;
		stbi_image_free(data);
	}

	else {
		GLenum format = 0;
		if (nrComponents == 1) {
			format = GL_RED;
		}

		else if (nrComponents == 3) {
			format = GL_RGB;
		}

		else if (nrComponents == 4) {
			format = GL_RGBA;
		}

		if (format != 0) {
			glBindTexture(GL_TEXTURE_2D, m_id);
			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		std::cout << "Texture load success: " << path << "\n";

		stbi_image_free(data);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
}

TextureOpenGL::TextureOpenGL(const char* path, const char* texType)
{
	this->m_type = texType;
	this->m_path = path;

	loadTexture(path, false);
}

TextureOpenGL::TextureOpenGL(const char* fileName, const char* texType, const std::string& directory)
{
	std::string finalPath = std::string(fileName);
	finalPath = directory + '/' + finalPath;
	this->m_type = texType;
	this->m_path = fileName;

	loadTexture(finalPath.c_str(), false);
}

TextureOpenGL::~TextureOpenGL() {

}


void TextureOpenGL::Init(const char* path, const char* texType, bool flipUV)
{
	this->m_type = texType;
	this->m_path = path;

	loadTexture(path, flipUV);
}

void TextureOpenGL::TexUnit(ShaderOpenGL& shader, const char* uniform, unsigned int unit)
{
	this->unit = unit;
	shader.Activate();
	shader.setInt(uniform, unit);
}

void TextureOpenGL::Bind()
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, m_id);
}

void TextureOpenGL::Unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureOpenGL::Delete()
{
	glDeleteTextures(1, &m_id);
}
