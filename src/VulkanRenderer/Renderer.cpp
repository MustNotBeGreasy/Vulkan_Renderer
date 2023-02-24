#include "VulkanRenderer/Renderer.h"

#include <iostream>
#include <vector>
#include <set>
#include <cstring>
#include <limits>
#include <algorithm>
#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VulkanRenderer/Settings/config.h"
#include "VulkanRenderer/Settings/VkLayersConfig.h"
#include "VulkanRenderer/ValidationLayersManager/vlManager.h"
#include "VulkanRenderer/Window/Window.h"
#include "VulkanRenderer/QueueFamily/QueueFamilyIndices.h"
#include "VulkanRenderer/QueueFamily/QueueFamilyHandles.h"
#include "VulkanRenderer/Swapchain/Swapchain.h"
#include "VulkanRenderer/ShaderManager/ShaderManager.h"
#include "VulkanRenderer/GraphicsPipeline/GraphicsPipelineManager.h"
#include "VulkanRenderer/Commands/CommandPool.h"
#include "VulkanRenderer/Commands/CommandUtils.h"
#include "VulkanRenderer/Extensions/ExtensionsUtils.h"
#include "VulkanRenderer/Buffers/BufferManager.h"
#include "VulkanRenderer/Buffers/BufferUtils.h"
#include "VulkanRenderer/Model/Vertex.h"
#include "VulkanRenderer/Descriptors/DescriptorPool.h"
#include "VulkanRenderer/Descriptors/DescriptorSetLayoutUtils.h"
#include "VulkanRenderer/Descriptors/DescriptorTypes/UBO.h"
#include "VulkanRenderer/Descriptors/DescriptorTypes/DescriptorTypes.h"
#include "VulkanRenderer/Descriptors/DescriptorSets.h"
#include "VulkanRenderer/Textures/Texture.h"
#include "VulkanRenderer/DepthBuffer/DepthBuffer.h"


void Renderer::run()
{
    if (m_models.size() == 0)
    {
        std::cout << "Nothing to render..\n";
        return;
    }

    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void Renderer::initWindow()
{
    m_window.createWindow(Config::RESOLUTION_W,Config::RESOLUTION_H,Config::TITLE);
}

void Renderer::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(Config::MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(Config::MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(Config::MAX_FRAMES_IN_FLIGHT);


    //---------------------------Sync. Objects Info----------------------------
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Creates the fence in the signaled state, so that the first call to
    // vkWaitForFences() returns immediately since the fence is already signaled.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;



    //---------------------Creation of Sync. Objects---------------------------

    for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(m_device.getLogicalDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create semaphore!");
        
        if (vkCreateSemaphore(m_device.getLogicalDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create semaphore!");
        
        if (vkCreateFence(m_device.getLogicalDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create fence!");
    }
}

void Renderer::createVkInstance()
{
    if (VkLayersConfig::VALIDATION_LAYERS_ENABLED &&!vlManager::AllRequestedLayersAvailable()) {
        throw std::runtime_error("Validation layers requested, but not available!"
        );
    }

    // This data is optional, but it may provide some useful information
    // to the driver in order to optimize our specific application(e.g because
    // it uses a well-known graphics engine with certain special behavior).
    //
    // Example: Our game uses UE and nvidia launched a new driver that optimizes
    // a certain thing. So in that case, nvidia will know it can apply that
    // optimization verifying this info.
    VkApplicationInfo appInfo{};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = Config::TITLE;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // This data is not optional and tells the Vulkan driver which global
    // extensions and validation layers we want to use.
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions = extensionsUtils::getRequiredExtensions();

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // This variable is placed outside the if statement to ensure that it is
    // not destroyed before the vkCreateInstance call.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    if (VkLayersConfig::VALIDATION_LAYERS_ENABLED)
    {

        createInfo.enabledLayerCount = static_cast<uint32_t> (
            VkLayersConfig::VALIDATION_LAYERS.size()
            );
        createInfo.ppEnabledLayerNames = VkLayersConfig::VALIDATION_LAYERS.data();

        debugCreateInfo = vlManager::getDebugMessengerCreateInfo();

        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

    }
    else
    {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }


    // -------------------------------------------------------------
    // Paramet. 1 -> Pointer to struct with creation info.
    // Paramet. 2 -> Pointer to custom allocator callbacks.
    // Paramet. 3 -> Pointer to the variable that stores the handle to the
    //               new object.
    // -------------------------------------------------------------
    if (vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan's instance!");
}


void Renderer::initVulkan()
{
    createVkInstance();

    vlManager::createDebugMessenger(m_vkInstance, m_debugMessenger);
    m_window.createSurface(m_vkInstance);

    m_device.pickPhysicalDevice(m_vkInstance,m_qfIndices,m_window.getSurface(),m_swapchain);
    m_device.createLogicalDevice(m_qfIndices);

    m_qfHandles.setQueueHandles(m_device.getLogicalDevice(), m_qfIndices);

    m_swapchain.createSwapchain(m_device.getPhysicalDevice(), m_device.getLogicalDevice(), m_window);

    m_swapchain.createAllImageViews(m_device.getLogicalDevice());

    m_renderPass.createRenderPass(m_device.getPhysicalDevice(),m_device.getLogicalDevice(),m_swapchain.getImageFormat());

    DescriptorSetLayoutUtils::createDescriptorSetLayout(
        m_device.getLogicalDevice(),
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        { 0,1 },
        { VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_FRAGMENT_BIT },
        m_descriptorSetLayout
    );

    m_graphicsPipelineM.createGraphicsPipeline(m_device.getLogicalDevice(),m_swapchain.getExtent(), m_renderPass.getRenderPass(), m_descriptorSetLayout);

    m_depthBuffer.createDepthBuffer(m_device.getPhysicalDevice(),m_device.getLogicalDevice(),m_swapchain.getExtent());

    m_swapchain.createFramebuffers(m_device.getLogicalDevice(), m_renderPass.getRenderPass(), m_depthBuffer);


    // Command Pool #1
    const uint32_t cmdPoolIndex = 0;
    CommandPool newCommandPool(m_device.getLogicalDevice(), m_qfIndices);
    m_commandPools.push_back(newCommandPool);

    for (auto& model : m_models)
    {
        // Vertex buffer(with staging buffer)
        BufferManager::createBufferAndTransferToDevice(
            m_commandPools[cmdPoolIndex],
            m_device.getPhysicalDevice(),
            m_device.getLogicalDevice(),
            model->vertices,
            m_qfHandles.graphicsQueue,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            model->vertexMemory,
            model->vertexBuffer
        );

        // Index Buffer(with staging buffer)
        BufferManager::createBufferAndTransferToDevice(
            m_commandPools[cmdPoolIndex],
            m_device.getPhysicalDevice(),
            m_device.getLogicalDevice(),
            model->indices,
            m_qfHandles.graphicsQueue,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            model->indexMemory,
            model->indexBuffer
        );

        // Create Textures
        model->createTexture(
            m_device.getPhysicalDevice(),
            m_device.getLogicalDevice(),
            VK_FORMAT_R8G8B8A8_SRGB,
            m_commandPools[0],
            m_qfHandles.graphicsQueue
        );


        // Uniform Buffers
        model->ubo.createUniformBuffers(m_device.getPhysicalDevice(), m_device.getLogicalDevice(), Config::MAX_FRAMES_IN_FLIGHT);
    }

    // Descriptor Pool
    m_descriptorPool.createDescriptorPool(
        m_device.getLogicalDevice(),
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
        { static_cast<uint32_t>(m_models.size() * Config::MAX_FRAMES_IN_FLIGHT),static_cast<uint32_t>(m_models.size() * Config::MAX_FRAMES_IN_FLIGHT) },
        m_models.size() * Config::MAX_FRAMES_IN_FLIGHT
    );

    // Descriptor Sets(of each type of model)
    for (auto& model : m_models)
    {
        model->descriptorSets.createDescriptorSets(
            m_device.getLogicalDevice(),
            model->texture.getTextureImageView(),
            model->texture.getTextureSampler(),
            model->ubo.getUniformBuffers(),
            m_descriptorSetLayout,
            m_descriptorPool
        );
    }
    // Allocates all the command buffers in the command Pool #1
    m_commandPools[cmdPoolIndex].allocAllCommandBuffers();

    createSyncObjects();
}

void Renderer::recordCommandBuffer(
    const VkFramebuffer& framebuffer,
    const VkRenderPass& renderPass,
    const VkExtent2D& extent,
    const VkPipeline& graphicsPipeline,
    const VkPipelineLayout& pipelineLayout,
    const uint32_t currentFrame,
    VkCommandBuffer& commandBuffer,
    CommandPool& commandPool
) {

    // Specifies some details about the usage of this specific command
    // buffer.
    commandPool.beginCommandBuffer(0, commandBuffer);

    // NUMBER OF VK_ATTACHMENT_LOAD_OP_CLEAR == CLEAR_VALUES
    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = { {0.3f, 0.3f, 0.3f, 1.0f} };
    clearValues[1].color = { 1.0f, 0.0f };

    VkRenderPassBeginInfo renderPassInfo{};
    commandPool.createRenderPassBeginInfo(renderPass, framebuffer, extent, clearValues, renderPassInfo);

    //--------------------------------RenderPass-----------------------------

    // The final parameter controls how the drawing commands between the
    // render pass will be provided:
    //    -VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be
    //    embedded in the primary command buffer itself and no secondary
    //    command buffers will be executed.
    //    -VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass
    //    commands will be executed from secondary command buffers.
    vkCmdBeginRenderPass(commandBuffer,&renderPassInfo,VK_SUBPASS_CONTENTS_INLINE);

    CommandUtils::STATE::bindPipeline(graphicsPipeline, commandBuffer);

    // Set Dynamic States
    CommandUtils::STATE::setViewport(0.0f, 0.0f, extent, 0.0f, 1.0f, 0, 1, commandBuffer);
    CommandUtils::STATE::setScissor({ 0, 0 }, extent, 0, 1, commandBuffer);

    for (const auto& model : m_models)
    {
        CommandUtils::STATE::bindVertexBuffers({ model->vertexBuffer }, { 0 }, 0, 1, commandBuffer);
        CommandUtils::STATE::bindIndexBuffer(model->indexBuffer, 0, VK_INDEX_TYPE_UINT32, commandBuffer);

        CommandUtils::STATE::bindDescriptorSets(pipelineLayout, 0, { model->getDescriptorSet(currentFrame) }, {}, commandBuffer);

        CommandUtils::ACTION::drawIndexed(model->indices.size(), 1, 0, 0, 0, commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    commandPool.endCommandBuffer(commandBuffer);
}


void Renderer::drawFrame(uint8_t& currentFrame)
{
    // Waits until the previous frame has finished.
   //    - 2 param. -> FenceCount.
   //    - 4 param. -> waitAll.
   //    - 5 param. -> timeOut.
    vkWaitForFences(m_device.getLogicalDevice(), 1, &m_inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // After waiting, we need to manually reset the fence.
    vkResetFences(m_device.getLogicalDevice(), 1, &m_inFlightFences[currentFrame]);


    //------------------------Updates uniform buffer----------------------------
    updateUniformBuffer1(m_device.getLogicalDevice(), currentFrame, m_swapchain.getExtent(), m_models[0]->ubo.getUniformBufferMemories());

    updateUniformBuffer2(m_device.getLogicalDevice(), currentFrame, m_swapchain.getExtent(), m_models[1]->ubo.getUniformBufferMemories());
    //--------------------Acquires an image from the swapchain------------------
    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_device.getLogicalDevice(), m_swapchain.getSwapchain(), UINT64_MAX, m_imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);


    //---------------------Records all the command buffer-----------------------    
    // Resets the command buffer to be able to be recorded/written.
    m_commandPools[0].resetCommandBuffer(currentFrame);
    recordCommandBuffer(
        m_swapchain.getFramebuffer(imageIndex),
        m_renderPass.getRenderPass(),
        m_swapchain.getExtent(),
        m_graphicsPipelineM.getGraphicsPipeline(),
        m_graphicsPipelineM.getPipelineLayout(),
        currentFrame,
        m_commandPools[0].getCommandBuffer(currentFrame),
        m_commandPools[0]
    );


    //----------------------Submits the command buffer -------------------------

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    // Specifies which semaphores to wait on before execution begins.
    submitInfo.pWaitSemaphores = waitSemaphores;
    // Specifies which stage/s of the pipeline to wait.
    submitInfo.pWaitDstStageMask = waitStages;
    // These two commands specify which command buffers to actualy submit for
   // execution.
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &(m_commandPools[0].getCommandBuffer(currentFrame));
    // Specifies which semaphores to signal once the command buffer/s have
   // finished execution.
    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if(vkQueueSubmit(m_qfHandles.graphicsQueue, 1, &submitInfo, m_inFlightFences[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer!");


    //-------------------Presentation of the swapchain image--------------------
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { m_swapchain.getSwapchain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    presentInfo.pResults = nullptr;

    vkQueuePresentKHR(m_qfHandles.presentQueue, &presentInfo);

    // Updates the frame
    currentFrame = (currentFrame + 1) % Config::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::mainLoop()
{
    // Tells us in which frame we are,
    // between 1 <= frame <= MAX_FRAMES_IN_FLIGHT
    uint8_t currentFrame = 0;
    while (m_window.isWindowClosed() == false)
    {
        m_window.pollEvents();
        drawFrame(currentFrame);
    }
    vkDeviceWaitIdle(m_device.getLogicalDevice());
}

void Renderer::destroySyncObjects()
{
    for (size_t i = 0; i < Config::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(m_device.getLogicalDevice(), m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device.getLogicalDevice(), m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device.getLogicalDevice(), m_inFlightFences[i], nullptr);
    }
}

void Renderer::cleanup()
{
    // DepthBuffer
    m_depthBuffer.destroyDepthBuffer(m_device.getLogicalDevice());

    // Framebuffers
    m_swapchain.destroyFramebuffers(m_device.getLogicalDevice());

    // ViewImages of the images from the Swapchain
    m_swapchain.destroyImageViews(m_device.getLogicalDevice());

    // Swapchain
    m_swapchain.destroySwapchain(m_device.getLogicalDevice());

    // Graphics Pipeline
    m_graphicsPipelineM.destroyGraphicsPipeline(m_device.getLogicalDevice());

    // Graphics Pipeline Layout
    m_graphicsPipelineM.destroyPipelineLayout(m_device.getLogicalDevice());

    // Render pass
    m_renderPass.destroyRenderPass(m_device.getLogicalDevice());

    // UBO(with their uniform buffers and uniform memories)
    for (auto& model : m_models)
        model->ubo.destroyUniformBuffersAndMemories(m_device.getLogicalDevice());

    // Descriptor Pool
    m_descriptorPool.destroyDescriptorPool(m_device.getLogicalDevice());

    // Textures
    for (auto& model : m_models)
        model->texture.destroyTexture(m_device.getLogicalDevice());

    // Descriptor Set Layout
    DescriptorSetLayoutUtils::destroyDescriptorSetLayout(
        m_device.getLogicalDevice(),
        m_descriptorSetLayout
    );

    // Bufferss
    for (auto& model : m_models)
    {
        BufferManager::destroyBuffer(m_device.getLogicalDevice(), model->vertexBuffer);
        BufferManager::destroyBuffer(m_device.getLogicalDevice(), model->indexBuffer);

        // Buffer Memories
        BufferManager::freeMemory(m_device.getLogicalDevice(), model->vertexMemory);
        BufferManager::freeMemory(m_device.getLogicalDevice(), model->indexMemory);
    }

    // Sync objects
    destroySyncObjects();

    // Command Pool
    for (auto& commandPool : m_commandPools)
        commandPool.destroyCommandPool();

    // Logical Device
    vkDestroyDevice(m_device.getLogicalDevice(), nullptr);

    // Validation Layers
    if (VkLayersConfig::VALIDATION_LAYERS_ENABLED)
    {
        vlManager::destroyDebugUtilsMessengerEXT(
            m_vkInstance,
            m_debugMessenger,
            nullptr
        );
    }

    // Window Surface
    m_window.destroySurface(m_vkInstance);

    // Vulkan's instance
    vkDestroyInstance(m_vkInstance, nullptr);

    // GLFW
    m_window.destroyWindow();
}


void Renderer::addModel(const std::string& meshFile, const std::string& textureFile) 
{
    std::unique_ptr<Model> newModel = std::make_unique<Model>((std::string(MODEL_DIR) + meshFile).c_str(), textureFile);

    m_models.push_back(std::move(newModel));
}


void Renderer::updateUniformBuffer1(
    const VkDevice& logicalDevice,
    const uint8_t currentFrame,
    const VkExtent2D extent,
    std::vector<VkDeviceMemory>& uniformBufferMemories
) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    DescriptorTypes::UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(time * 90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), (extent.width / (float)extent.height), 0.1f, 10.0f);

    // GLM was designed for OpenGl, where the Y coordinate of the clip coord. is
    // inverted. To compensate for that, we have to flip the sign on the scaling
    // factor of the Y axis.
    ubo.proj[1][1] *= -1;

    void* data;
    vkMapMemory(logicalDevice, uniformBufferMemories[currentFrame], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(logicalDevice, uniformBufferMemories[currentFrame]);
}

void Renderer::updateUniformBuffer2(
    const VkDevice& logicalDevice,
    const uint8_t currentFrame,
    const VkExtent2D extent,
    std::vector<VkDeviceMemory>& uniformBufferMemories
) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    DescriptorTypes::UniformBufferObject ubo{};
    //ubo.model = glm::rotate(
    //      glm::mat4(1.0f),
    //      glm::radians(time * 90.0f),
    //      glm::vec3(0.0f, 0.0f, 1.0f)
    //);
    ubo.model = glm::mat4(1.0f);
    ubo.model = glm::scale(ubo.model,glm::vec3(0.003f));
    ubo.model = glm::translate(ubo.model, glm::vec3(-700.0f, 0.0f, 0.0f));
    ubo.model = glm::rotate(ubo.model, glm::radians(time * 90.0f), glm::vec3(0.0f, 1.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), (extent.width / (float)extent.height), 0.1f, 10.0f);

    // GLM was designed for OpenGl, where the Y coordinate of the clip coord. is
    // inverted. To compensate for that, we have to flip the sign on the scaling
    // factor of the Y axis.
    ubo.proj[1][1] *= -1;

    void* data;
    vkMapMemory(logicalDevice, uniformBufferMemories[currentFrame], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(logicalDevice, uniformBufferMemories[currentFrame]);
}