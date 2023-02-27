#include "VulkanRenderer/Model/Types/NormalPBR.h"

#include <iostream>

#include "VulkanRenderer/Textures/Texture.h"
#include "VulkanRenderer/Settings/Config.h"
#include "VulkanRenderer/Descriptors/Types/DescriptorTypes.h"
#include "VulkanRenderer/Descriptors/Types/UBO/UBOutils.h"
#include "VulkanRenderer/Settings/GraphicsPipelineConfig.h"
#include "VulkanRenderer/Buffers/BufferManager.h"
#include "VulkanRenderer/Model/Types/DirectionalLight.h"


NormalPBR::NormalPBR(const std::string& name, const std::string& modelFileName)
	: Model(name, ModelType::NORMAL_PBR)
{
	loadModel((std::string(MODEL_DIR) + modelFileName).c_str());

	for (auto& mesh : m_meshes)
	{
		mesh.m_textures.resize(GRAPHICS_PIPELINE::PBR::TEXTURES_PER_MESH_COUNT);
	}

	// TODO: Load scene settings.
	actualPos = glm::fvec4(0.0f);
	actualSize = glm::fvec3(1.0f);
	actualRot = glm::fvec3(0.0f);
}

NormalPBR::~NormalPBR() {}

void NormalPBR::destroy(const VkDevice& logicalDevice)
{
	m_ubo.destroyUniformBuffersAndMemories(logicalDevice);

	for (auto& mesh : m_meshes)
	{
		for(auto & texture : mesh.m_textures)
			texture.destroyTexture(logicalDevice);

		BufferManager::destroyBuffer(logicalDevice, mesh.m_vertexBuffer);
		BufferManager::destroyBuffer(logicalDevice, mesh.m_indexBuffer);

		BufferManager::freeMemory(logicalDevice, mesh.m_vertexMemory);
		BufferManager::freeMemory(logicalDevice, mesh.m_indexMemory);
	}
}

std::string NormalPBR::getMaterialTextureName(aiMaterial* material,const aiTextureType& type,const std::string& typeName) 
{
	if (material->GetTextureCount(type) > 0)
	{
		// TODO: do something when there are many textures of the same type.

		//for (size_t i = 0; i < material->GetTextureCount(type); i++)
		//{
		//   aiString str;
		//   material->GetTexture(type, i, &str);
		//}
		aiString str;
		material->GetTexture(type, 0, &str);

		return str.C_Str();

	}
	else {
		std::cout << "LEYENDO DEFAULT PORQ NO EXISTE LA TEXTURE\n";
		std::cout << "NO SE PUDO " << typeName << std::endl;
		// TODO!
		return "textures/default.jpg";
	}
}

void NormalPBR::processMesh(aiMesh* mesh, const aiScene* scene)
{
	Mesh<Attributes::PBR::Vertex> newMesh;

	for (size_t i = 0; i < mesh->mNumVertices; i++)
	{
		Attributes::PBR::Vertex vertex{};

		vertex.pos = {mesh->mVertices[i].x,mesh->mVertices[i].y,mesh->mVertices[i].z};
		vertex.normal = {mesh->mNormals[i].x,mesh->mNormals[i].y,mesh->mNormals[i].z};
		vertex.normal = glm::normalize(vertex.normal);

		vertex.texCoord = {mesh->mTextureCoords[0][i].x,mesh->mTextureCoords[0][i].y,};
		glm::vec3 tangent = {mesh->mTangents[i].x,mesh->mTangents[i].y,mesh->mTangents[i].z};
		tangent = glm::normalize(tangent);
		
		//glm::vec3 bitangent = glm::cross(vertex.normal, tangent);

		vertex.tangent = tangent;

		newMesh.m_vertices.push_back(vertex);
	}

	for (size_t i = 0; i < mesh->mNumFaces; i++)
	{
		auto face = mesh->mFaces[i];
		for (size_t j = 0; j < face.mNumIndices; j++)
			newMesh.m_indices.push_back(face.mIndices[j]);
	}


	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		TextureToLoadInfo info;
		info.name = getMaterialTextureName(material, aiTextureType_DIFFUSE, "DIFFUSE");
		info.format = VK_FORMAT_R8G8B8A8_SRGB;
		newMesh.m_texturesToLoadInfo.push_back(info);


		info.name = getMaterialTextureName(material,aiTextureType_UNKNOWN,"METALIC_ROUGHNESS");
		info.format = VK_FORMAT_R8G8B8A8_SRGB;
		newMesh.m_texturesToLoadInfo.push_back(info);

		info.name = getMaterialTextureName(material,aiTextureType_NORMALS,"NORMALS");
		info.format = VK_FORMAT_R8G8B8A8_UNORM;
		newMesh.m_texturesToLoadInfo.push_back(info);
	}
	m_meshes.push_back(newMesh);
}

void NormalPBR::createUniformBuffers(const VkPhysicalDevice& physicalDevice,const VkDevice& logicalDevice,const uint32_t& uboCount) 
{
	m_ubo.createUniformBuffers(physicalDevice, logicalDevice, uboCount, sizeof(DescriptorTypes::UniformBufferObject::NormalPBR));
}

void NormalPBR::createDescriptorSets(const VkDevice& logicalDevice,const VkDescriptorSetLayout& descriptorSetLayout,DescriptorPool& descriptorPool) 
{
	for (auto& mesh : m_meshes)
	{
		mesh.m_descriptorSets.createDescriptorSets(
			logicalDevice,
			GRAPHICS_PIPELINE::PBR::UBOS_INFO,
			GRAPHICS_PIPELINE::PBR::SAMPLERS_INFO,
			mesh.m_textures,
			m_ubo.getUniformBuffers(),
			descriptorSetLayout,
			descriptorPool
		);
	}
}

void NormalPBR::uploadVertexData(const VkPhysicalDevice& physicalDevice,const VkDevice& logicalDevice,VkQueue& graphicsQueue,CommandPool& commandPool) 
{

	for (auto& mesh : m_meshes)
	{
		// Vertex Buffer(with staging buffer)
		BufferManager::createBufferAndTransferToDevice(
			commandPool,
			physicalDevice,
			logicalDevice,
			mesh.m_vertices.data(),
			sizeof(mesh.m_vertices[0]) * mesh.m_vertices.size(),
			graphicsQueue,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			mesh.m_vertexMemory,
			mesh.m_vertexBuffer
		);

		// Index Buffer(with staging buffer)
		BufferManager::createBufferAndTransferToDevice(
			commandPool,
			physicalDevice,
			logicalDevice,
			mesh.m_indices.data(),
			sizeof(mesh.m_indices[0]) * mesh.m_indices.size(),
			graphicsQueue,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			mesh.m_indexMemory,
			mesh.m_indexBuffer
		);
	}
}

void NormalPBR::createTextures(const VkPhysicalDevice& physicalDevice,const VkDevice& logicalDevice,CommandPool& commandPool, VkQueue& graphicsQueue)
{
	for (auto& mesh : m_meshes)
	{
		for (size_t i = 0; i < mesh.m_textures.size(); i++)
		{
			mesh.m_textures[i] = Texture(
				physicalDevice,
				logicalDevice,
				mesh.m_texturesToLoadInfo[i],
				// isSkybox
				false,
				commandPool,
				graphicsQueue
			);
		}
	}
}

void NormalPBR::updateUBO(
	const VkDevice&				logicalDevice,
	const glm::vec4&			cameraPos,
	const glm::mat4&			proj,
	const std::vector<std::shared_ptr<Model>>& models,
	const std::vector<size_t>	directionalLightIndices,
	const uint32_t&				currentFrame
) {

	DescriptorTypes::UniformBufferObject::NormalPBR newUBO;

	newUBO.model = UBOutils::getUpdatedModelMatrix(actualPos,actualRot,actualSize);
	newUBO.view = UBOutils::getUpdatedViewMatrix(glm::vec3(cameraPos),glm::vec3(0.0f, 0.0f, 0.0f),glm::vec3(0.0f, 1.0f, 0.0f));
	newUBO.proj = proj;

	newUBO.cameraPos = cameraPos;

	updateLightData(newUBO, models, directionalLightIndices);

	UBOutils::updateUBO(m_ubo, logicalDevice, newUBO, currentFrame);
}

void NormalPBR::updateLightData(
	DescriptorTypes::UniformBufferObject::NormalPBR& ubo,
	const std::vector<std::shared_ptr<Model>>& models,
	const std::vector<size_t> directionalLightIndices
) {
	ubo.lightsCount = directionalLightIndices.size();

	for (int i = 0; i < ubo.lightsCount; i++)
	{
		auto& model = models[directionalLightIndices[i]];
		if (auto pModel = std::dynamic_pointer_cast<DirectionalLight>(model))
		{
			ubo.lightPositions[i] = (pModel->actualPos);
			ubo.lightColors[i] = pModel->color;
		}
	}
}