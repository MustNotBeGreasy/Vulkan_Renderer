#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


#include "VulkanRenderer/Model/Model.h"
#include "VulkanRenderer/Model/Attributes.h"
#include "VulkanRenderer/Textures/Texture.h"
#include "VulkanRenderer/GraphicsPipeline/GraphicsPipeline.h"
#include "VulkanRenderer/Descriptors/DescriptorPool.h"
#include "VulkanRenderer/Descriptors/DescriptorSets.h"
#include "VulkanRenderer/Descriptors/Types/UBO/UBO.h"

class Skybox : public Model
{
public:

	Skybox(const std::string& name, const std::string& textureFolderName);

	~Skybox() override;

	void destroy(const VkDevice& logicalDevice) override;

    void createTextures(
        const VkPhysicalDevice& physicalDevice,
        const VkDevice& logicalDevice,
        CommandPool& commandPool,
        VkQueue& graphicsQueue
    ) override;

    void createDescriptorSets(
        const VkDevice& logicalDevice,
        const VkDescriptorSetLayout& descriptorSetLayout,
        DescriptorPool& descriptorPool
    ) override;

    void createUniformBuffers(
        const VkPhysicalDevice& physicalDevice,
        const VkDevice& logicalDevice,
        const uint32_t& uboCount
    ) override;

    void updateUBO(
        const VkDevice& logicalDevice,
        const glm::vec4& cameraPos,
        const VkExtent2D& extent,
        const uint32_t& currentFrame
    );

    void uploadVertexData(
        const VkPhysicalDevice& physicalDevice,
        const VkDevice& logicalDevice,
        VkQueue& graphicsQueue,
        CommandPool& commandPool
    );

    // Info to update UBO.
    float extremeX[2];
    float extremeY[2];
    float extremeZ[2];
    glm::fvec4 actualPos;
    glm::fvec3 actualSize;
    glm::fvec3 actualRot;

    std::vector<Mesh<Attributes::SKYBOX::Vertex>> m_meshes;

private:

    void processMesh(aiMesh* mesh, const aiScene* scene) override;
};