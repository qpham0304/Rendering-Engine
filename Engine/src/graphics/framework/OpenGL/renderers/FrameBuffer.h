#pragma once

#include <glad/glad.h>
#include <iostream>

class FrameBuffer
{
public:
	//NOTE: this framebuffer is not flexible with only one texture, only use it to render the general scene
	//for specific for specific implementation or multiple render target, create a manual one
	//or use the derrived classes for a specific feature
	unsigned int FBO;
	unsigned int RBO;
	unsigned int texture;

	FrameBuffer() = default;
	FrameBuffer(int width, int height);
	FrameBuffer(int width, int height, int size);
	~FrameBuffer();

	void Init(
		int width,
		int height,
		int internalFormat,
		int format,
		int type,
		const void* data
	);

	void Bind();
	void Unbind();
	void Delete();
};

