#pragma once

#include <vector>
#include <string>
#include <memory>

#include "VulkanRenderer/Image/Image.h"
#include "VulkanRenderer/Texture/Texture.h"
#include "VulkanRenderer/Descriptor/DescriptorSets.h"
#include "VulkanRenderer/Model/Attributes.h"

template<typename T>
struct Mesh
{
	// Vertex
	std::vector<T>                         vertices;
	std::vector<uint32_t>                  indices;

	VkBuffer                               vertexBuffer;
	VkBuffer                               indexBuffer;
	VkDeviceMemory                         vertexMemory;
	VkDeviceMemory                         indexMemory;

	std::vector<std::shared_ptr<Texture>>  textures;
	std::vector<TextureToLoadInfo>         texturesToLoadInfo;

	// (One descriptor set for all the ubo and samplers of a mesh)
	// (The same descriptor set for each frame in flight)
	DescriptorSets                         descriptorSets;
};