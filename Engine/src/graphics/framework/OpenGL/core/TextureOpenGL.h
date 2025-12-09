#pragma once
#include "Texture.h"

class ShaderOpenGL;
class TextureOpenGL : public Texture
{
public:
	TextureOpenGL(const char* path, const char* texType);
	TextureOpenGL(const char* fileName, const char* texType, const std::string& directory);
	TextureOpenGL() = default;

	//TextureOpenGL(const TextureOpenGL& other) = default;
	//TextureOpenGL& operator=(const TextureOpenGL& other) = default;
	//TextureOpenGL(const TextureOpenGL&& other) = default;
	//TextureOpenGL& operator=(const TextureOpenGL&& other) = default;
	
	virtual ~TextureOpenGL() override;

	void Init(const char* path, const char* texType, bool flipUV);
	void TexUnit(ShaderOpenGL& shader, const char* uniform, unsigned int unit);
	virtual void Bind() override;
	virtual void Unbind() override;
	virtual void Delete() override;

protected:
	virtual void loadTexture(const char* path, bool flip);

};

