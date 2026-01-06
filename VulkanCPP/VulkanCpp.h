#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#define ENABLE_VK_DEBUG
#include <vulkan/vulkan.h>
#include <windows.h>

#include <vector>
#include <array>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VK_CHECK(x) \
    do { VkResult err = x; if (err != VK_SUCCESS) throw std::runtime_error("Vulkan error"); } while (0)

static const char* interopExtensionName = "VK_KHR_external_memory_win32";

// ---------------------------------------------
// Structs
// ---------------------------------------------

struct VertexPositionColor
{
    glm::vec3 position;
    glm::vec3 color;

    // 获取顶点绑定描述
    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription;
        bindingDescription.binding = 0; // 默认绑定点 0
        bindingDescription.stride = sizeof(VertexPositionColor);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // 获取顶点属性描述
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
    {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(VertexPositionColor, position);

        // Color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(VertexPositionColor, color);

        return attributeDescriptions;
    }
};

struct ModelViewProjection
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// ---------------------------------------------
// Helpers
// ---------------------------------------------

static std::vector<char> ReadFile(const char* name)
{
    std::ifstream f(name, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(f), {});
}



struct Luid
{
    uint32_t Low;
    int32_t  High;

    bool operator==(const Luid& other) const
    {
        return Low == other.Low && High == other.High;
    }
};

static Luid RtlConvertUlongToLuid(uint64_t val)
{
    // 行为严格等价于 Windows ntddk.h 里的 RtlConvertUlongToLuid
    Luid luid;
    luid.Low = static_cast<uint32_t>(val);
    luid.High = 0;
    return luid;
}

struct ImageWithView {
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
};

// ---------------------------------------------
// VulkanInterop
// ---------------------------------------------

class VulkanInterop
{
public:
	
    void Initialize(
        HANDLE directTextureHandle,
        uint64_t targetDeviceLuid,
        uint32_t w,
        uint32_t h,
        VkFormat format,
        VkExternalMemoryHandleTypeFlagBits targetHandleType,
        const char* modelFilePath);

    void Draw(float time);
    void Resize(HANDLE sharedTexture, uint32_t w, uint32_t h);
    void Clear();

private:
    ImageWithView CreateImageView(
        VkFormat imageFormat,
        VkSampleCountFlagBits sampleCount,
        VkImageUsageFlags imageUsage,
        VkImageAspectFlags viewAspect
    );
    void CreateImageViews(HANDLE directTextureHandle);
    void CreatePipeline();
    void CreateFramebuffer();
    void CreateCommandBuffer();
    void SubmitWork(const void* pNext = nullptr);

    bool CheckGraphicsQueue(
        VkPhysicalDevice physicalDevice,
        uint32_t& index);
    bool CheckExternalMemoryExtension(
        VkPhysicalDevice physicalDevice);
    Luid VulkanDeviceLuidToLuid(const uint8_t* vulkanDeviceLuidPtr);
    bool CheckPhysicalDeviceLuid(
        const uint8_t* vulkanDeviceLuidPtr,
        const Luid& targetDeviceLuid,
        VkExternalMemoryHandleTypeFlagBits targetHandleType);
    bool CheckExternalImageHandleType(
        VkPhysicalDevice physicalDevice,
        VkFormat targetFormat,
        VkExternalMemoryHandleTypeFlagBits targetHandleType);
    VkFormat FindSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features);
    VkShaderModule CreateShaderModule(
        const std::vector<char>& code);
    void CreateBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);
    uint32_t GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties);
    
    void UpdateModelViewProjection(float time);

    void ReleaseSizeDependentResources();

    bool LoadModelFromStream(const std::string& filename, std::vector<VertexPositionColor>& outVertices, std::vector<uint32_t>& outIndices);

    VkExternalMemoryFeatureFlags
        GetImageFormatExternalMemoryFeatures(
            const VkImageCreateInfo& imageInfo,
            VkExternalMemoryHandleTypeFlagBits handleType);
    void CreateDebugUtilsMessenger();
    void ReadbackResolveImageAndPrint();


private:
    std::vector<VertexPositionColor> vertices;
    std::vector<uint32_t> indices;

    ModelViewProjection modelViewProjection{};


    uint32_t width{}, height{};

    VkInstance instance{};

    VkDevice device{};
    VkPhysicalDevice physicalDevice{};
    VkPhysicalDeviceProperties physicalDeviceProperties{};

    VkQueue queue{};

    VkFence fence{};

    ImageWithView colorImageWithView{};
    ImageWithView depthImageWithView{};
    VkImage directImage{};
    VkImageView directView{};
    VkDeviceMemory directMem{};

    VkFramebuffer framebuffer{};
    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};

    VkDescriptorPool descriptorPool{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkDescriptorSet descriptorSet{};

    VkRenderPass renderPass{};

    VkCommandPool commandPool{};
    VkCommandBuffer commandBuffer{};

    VkBuffer vertexBuffer{}, indexBuffer{}, uniformBuffer{};
    VkDeviceMemory vertexMemory{}, indexMemory{}, uniformMemory{};

    VkShaderModule vertexShaderModule{}, fragmentShaderModule{};
    
    VkFormat depthFormat{};
    VkFormat targetFormat{};
    VkExternalMemoryHandleTypeFlagBits targetHandleType{};
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_8_BIT;
    
    VkDebugUtilsMessengerEXT debugMessenger{};

    int RotationSpeed = 1;
};