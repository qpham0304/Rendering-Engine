#include "DepthMap.h"

void DepthMap::setup(unsigned int width, unsigned int height) {
	glGenFramebuffers(1, &FBO);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float clampColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, clampColor);

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

DepthMap::DepthMap()
{
	setup(DEFAULT_WIDTH, DEFAULT_HEIGHT);
	this->width = DEFAULT_WIDTH;
	this->height = DEFAULT_HEIGHT;
}

DepthMap::DepthMap(unsigned int width, unsigned int height)
{
	this->width = width;
	this->height = height;
	setup(width, height);
}

void DepthMap::Init(unsigned int width, unsigned int height)
{
	Delete();
	setup(width, height);
}

void DepthMap::Bind() {
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
}

void DepthMap::Unbind() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void DepthMap::Delete() {
	glDeleteFramebuffers(1, &FBO);
	glDeleteTextures(1, &texture);
}

unsigned int DepthMap::getWidth()
{
	return width;
}

unsigned int DepthMap::getHeight()
{
	return height;
}


