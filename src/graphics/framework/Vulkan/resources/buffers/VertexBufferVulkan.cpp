#include "VertexBufferVulkan.h"
#include "vulkan/vulkan.h"

//typedef VertexBuffer::VertexAttribute VertexAttribute;
//
//static uint32_T toGLType(VertexAttribute::Type type)
//{
//	switch (type)
//	{
//	case VertexAttribute::Type::Float: return GL_FLOAT;
//	case VertexAttribute::Type::Int:   return GL_INT;
//	case VertexAttribute::Type::UInt:  return GL_UNSIGNED_INT;
//	case VertexAttribute::Type::Byte:  return GL_BYTE;
//	case VertexAttribute::Type::UByte: return GL_UNSIGNED_BYTE;
//	}
//	return GL_FLOAT;
//}

VertexBufferVulkan::VertexBufferVulkan(uint32_t id, VkBuffer buffer, VkDeviceMemory bufferMemory)
	: BufferVulkan(id, buffer, bufferMemory)
{
}

VertexBufferVulkan::~VertexBufferVulkan()
{

}

void VertexBufferVulkan::bind(void* commandBuffer)
{
	VkBuffer vertexBuffers[] = { buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers((VkCommandBuffer)commandBuffer, 0, 1, vertexBuffers, offsets);
}

//void VertexBufferVulkan::setAttribute(VertexAttribute attribute)
//{
//
//}

