#pragma once

struct VkBuffer_T;
struct VkDeviceMemory_T;
struct VkDevice_T;

struct VkDescriptorSetLayout_T;
struct VkDescriptorPool_T;
struct VkDescriptorSet_T;

struct VkImage_T;
struct VkDeviceMemory_T;
struct VkImageView_T;
struct VkSampler_T;

struct VkFormat_T;
struct VkImageAspectFlags_T;

struct VkCommandBuffer_T;

class VkWrap {
protected:
	typedef VkBuffer_T* VkBuffer;
	typedef VkDeviceMemory_T* VkDeviceMemory;
	typedef VkDevice_T* VkDevice;

	typedef VkDescriptorSetLayout_T* VkDescriptorSetLayout;
	typedef VkDescriptorPool_T* VkDescriptorPool;
	typedef VkDescriptorSet_T* VkDescriptorSet;

	typedef VkImage_T* VkImage;
	typedef VkDeviceMemory_T* VkDeviceMemory;
	typedef VkImageView_T* VkImageView;
	typedef VkSampler_T* VkSampler;

	typedef VkFormat_T* VkFormat;
	typedef VkImageAspectFlags_T* VkImageAspectFlags;

	typedef VkCommandBuffer_T* VkCommandBuffer;
};

