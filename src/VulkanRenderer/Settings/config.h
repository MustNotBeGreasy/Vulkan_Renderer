#pragma once 

#include <vector>
#include <string>

#include <vulkan/vulkan.h>
#include "VulkanRenderer/Descriptor/DescriptorInfo.h"

namespace Config
{
	inline const uint16_t RESOLUTION_W = 1920;
	inline const uint16_t RESOLUTION_H = 1080;

	inline const char* WINDOW_TITLE = "Hello Vulkan";

	// Graphic's settings
	inline const int MAX_FRAMES_IN_FLIGHT = 2;

	//Camera settings
	inline const float FOV = 45.0f;
	inline const float Z_NEAR = 0.01f;
	inline const float Z_FAR = 200.0f;


	// Textures - Naming Convention For Cubemaps
	inline const std::vector<std::string> TEXTURE_CUBEMAP_NAMING_CONV =
	{ "px","nx","py","ny","pz","nz"};

	inline const std::string TEXTURE_CUBEMAP_FORMAT = "png";

	// Scene
	inline const uint32_t LIGHTS_COUNT = 10;

	// BRDF
	inline const uint32_t BRDF_WIDTH = 256;
	inline const uint32_t BRDF_HEIGHT = 256;
}