#pragma once
#include "Texture.h"

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

	virtual void Init(const char* path, const char* texType, bool flipUV) override;
	virtual void TexUnit(ShaderOpenGL& shader, const char* uniform, unsigned int unit) override;
	virtual void Bind() override;
	virtual void Unbind() override;
	virtual void Delete() override;

protected:
	virtual void loadTexture(const char* path, bool flip);

};

