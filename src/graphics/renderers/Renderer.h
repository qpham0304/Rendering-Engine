#pragma once

#include <memory>

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

