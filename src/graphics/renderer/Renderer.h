#pragma once

#include <memory>
#include <Shader.h>

class Renderer
{
private:
	

public:
	Renderer() = default;
	~Renderer() = default;

	//void registerShader();
	virtual void render() = 0;
	virtual void init() = 0;
};

