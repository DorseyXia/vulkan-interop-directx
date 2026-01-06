// ============================================================
// VulkanInterop.cpp  (Converted from C# Silk.NET Vulkan)
// ============================================================

#include "pch.h"
#include "VulkanCpp.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <glm/glm.hpp>

Luid VulkanInterop::VulkanDeviceLuidToLuid(const uint8_t* vulkanDeviceLuidPtr)
{
    uint64_t vulkanLuidUlong = 0;

    // 等价于 C# 的 BitConverter.ToUInt64(byte[])
    memcpy(&vulkanLuidUlong, vulkanDeviceLuidPtr, VK_LUID_SIZE);

    return RtlConvertUlongToLuid(vulkanLuidUlong);
}

bool VulkanInterop::CheckGraphicsQueue(
    VkPhysicalDevice physicalDevice,
    uint32_t& index)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueFamilyCount,
        queueFamilies.data());

    index = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            return true;

        index++;
    }

    return false;
}

bool VulkanInterop::CheckExternalMemoryExtension(
    VkPhysicalDevice physicalDevice)
{
    uint32_t propertyCount = 0;
    vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &propertyCount,
        nullptr);

    std::vector<VkExtensionProperties> extensions(propertyCount);
    vkEnumerateDeviceExtensionProperties(
        physicalDevice,
        nullptr,
        &propertyCount,
        extensions.data());

    for (const auto& ext : extensions)
    {
        if (strcmp(ext.extensionName, interopExtensionName) == 0)
            return true;
    }

    return false;
}



bool VulkanInterop::CheckPhysicalDeviceLuid(
    const uint8_t* vulkanDeviceLuidPtr,
    const Luid& targetDeviceLuid,
    VkExternalMemoryHandleTypeFlagBits targetHandleType)
{
    switch (targetHandleType)
    {
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT:
    case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT:
    {
        // 必须是同一个物理 GPU
        Luid vulkanLuid = VulkanDeviceLuidToLuid(vulkanDeviceLuidPtr);
        return vulkanLuid == targetDeviceLuid;
    }

    default:
        // 不要求同一物理设备
        return true;
    }
}

bool VulkanInterop::CheckExternalImageHandleType(
    VkPhysicalDevice physicalDevice,
    VkFormat targetFormat,
    VkExternalMemoryHandleTypeFlagBits targetHandleType)
{
    // 对应 PhysicalDeviceExternalImageFormatInfo
    VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo{};
    externalFormatInfo.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalFormatInfo.handleType = targetHandleType;

    // 对应 PhysicalDeviceImageFormatInfo2
    VkPhysicalDeviceImageFormatInfo2 formatInfo{};
    formatInfo.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.pNext = &externalFormatInfo;
    formatInfo.format = targetFormat;
    formatInfo.type = VK_IMAGE_TYPE_2D;
    formatInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    formatInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // ExternalImageFormatProperties
    VkExternalImageFormatProperties externalFormatProperties{};
    externalFormatProperties.sType =
        VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

    // ImageFormatProperties2
    VkImageFormatProperties2 formatProperties{};
    formatProperties.sType =
        VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    formatProperties.pNext = &externalFormatProperties;

    VkResult result =
        vkGetPhysicalDeviceImageFormatProperties2(
            physicalDevice,
            &formatInfo,
            &formatProperties);

    if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
    {
        // handleType 或 format 不支持
        return false;
    }

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(
            "External handle type check failed");
    }

    return (externalFormatProperties
        .externalMemoryProperties
        .externalMemoryFeatures &
        VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) != 0;
}

uint32_t VulkanInterop::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

VkExternalMemoryFeatureFlags
VulkanInterop::GetImageFormatExternalMemoryFeatures(
    const VkImageCreateInfo& imageInfo,
    VkExternalMemoryHandleTypeFlagBits handleType)
{
    // 1. External image format info
    VkPhysicalDeviceExternalImageFormatInfo externalFormatInfo{};
    externalFormatInfo.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
    externalFormatInfo.pNext = nullptr;
    externalFormatInfo.handleType = handleType;

    // 2. Image format info 2
    VkPhysicalDeviceImageFormatInfo2 formatInfo{};
    formatInfo.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
    formatInfo.pNext = &externalFormatInfo;
    formatInfo.format = imageInfo.format;
    formatInfo.type = imageInfo.imageType;
    formatInfo.tiling = imageInfo.tiling;
    formatInfo.usage = imageInfo.usage;
    formatInfo.flags = imageInfo.flags;

    // 3. External image format properties
    VkExternalImageFormatProperties externalFormatProperties{};
    externalFormatProperties.sType =
        VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
    externalFormatProperties.pNext = nullptr;

    // 4. Image format properties 2
    VkImageFormatProperties2 formatProperties{};
    formatProperties.sType =
        VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
    formatProperties.pNext = &externalFormatProperties;

    // 5. Query
    VkResult res = vkGetPhysicalDeviceImageFormatProperties2(
        physicalDevice,
        &formatInfo,
        &formatProperties);

    if (res != VK_SUCCESS)
        throw std::runtime_error("vkGetPhysicalDeviceImageFormatProperties2 failed");

    return externalFormatProperties
        .externalMemoryProperties
        .externalMemoryFeatures;
}


VkFormat VulkanInterop::FindSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(
            physicalDevice,
            format,
            &properties);

        if ((tiling == VK_IMAGE_TILING_LINEAR &&
            (properties.linearTilingFeatures & features) == features) ||
            (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (properties.optimalTilingFeatures & features) == features))
        {
            return format;
        }
    }

    throw std::runtime_error("Supported format not found");
}

VkShaderModule VulkanInterop::CreateShaderModule(
    const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result =
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");

    return shaderModule;
}

// ============================================================
// Core
// ============================================================

// ---------------- Images (include external shared) ----------------



void VulkanInterop::CreateBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    // 1️⃣ 创建 Buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer!");

    // 2️⃣ 获取 Buffer 内存需求
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    // 3️⃣ 分配 Memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = GetMemoryTypeIndex(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory!");

    // 4️⃣ binding Buffer and Memory
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

ImageWithView VulkanInterop::CreateImageView(
    VkFormat imageFormat,
    VkSampleCountFlagBits sampleCount,
    VkImageUsageFlags imageUsage,
    VkImageAspectFlags viewAspect
) {
    ImageWithView result{};

    // ---------------- create Image ----------------
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = sampleCount;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = imageUsage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &result.image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image");
    }

    // ---------------- allocate memory ----------------
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, result.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = GetMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &result.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory");
    }

    if (vkBindImageMemory(device, result.image, result.memory, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind image memory");
    }

    // ---------------- create ImageView ----------------
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = result.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = viewAspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &result.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view");
    }

    return result;
}

void VulkanInterop::CreateImageViews(HANDLE directTextureHandle)
{
    // 1. create color image + view
    colorImageWithView = CreateImageView(
        targetFormat,
        sampleCount,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );


    // 2. create depth image + view
    depthImageWithView = CreateImageView(
        depthFormat,
        sampleCount,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

     //3. import DirectX texture's Vulkan image
    {
        VkExternalMemoryImageCreateInfo externalInfo{};
        externalInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        externalInfo.handleTypes = targetHandleType; // VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT etc.

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.pNext = &externalInfo;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = targetFormat;
        imageInfo.extent = { width, height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // Include TRANSFER_SRC so Readback/Copy barriers and vkCmdCopyImageToBuffer work.
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        //imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // 创建外部 image（带 pNext 的 external image create info 已在 imageInfo 中设置）
        VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &directImage));

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(device, directImage, &memReq);

        VkImportMemoryWin32HandleInfoKHR importInfo{};
        importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
        importInfo.pNext = nullptr;
        importInfo.handleType = targetHandleType;
        importInfo.handle = directTextureHandle;

        VkMemoryAllocateInfo memAlloc{};
        memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAlloc.pNext = &importInfo; // 链接 importInfo
        memAlloc.allocationSize = memReq.size;
        memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo{};
        dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
        dedicatedAllocateInfo.pNext = nullptr;
        dedicatedAllocateInfo.image = directImage;
        dedicatedAllocateInfo.buffer = VK_NULL_HANDLE;

        VkExternalMemoryFeatureFlags externalFeatures =
            GetImageFormatExternalMemoryFeatures(imageInfo, targetHandleType);
        if (externalFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT)
        {
            // 将 dedicatedAllocateInfo 插入到 importInfo 的 pNext 链上
            importInfo.pNext = &dedicatedAllocateInfo;
        }

        // 分配并绑定内存，使用检查宏确保调用成功
        VK_CHECK(vkAllocateMemory(device, &memAlloc, nullptr, &directMem));
        VK_CHECK(vkBindImageMemory(device, directImage, directMem, 0));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = directImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = targetFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &directView));

        // Debug 打印，以便在运行时确认导入的 image / memory / view 有效
        std::cout << "Imported Vulkan image: " << directImage
                  << " directMem: " << directMem
                  << " directView: " << directView << std::endl;
    }
}

// ---------------- Pipeline ----------------

void VulkanInterop::CreatePipeline()
{
    // 1. Shader stages
    VkPipelineShaderStageCreateInfo vertexShaderStage{};
    vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStage.pNext = nullptr;
    vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStage.module = vertexShaderModule;
    vertexShaderStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStage{};
    fragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStage.pNext = nullptr;
    fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStage.module = fragmentShaderModule;
    fragmentShaderStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStage, fragmentShaderStage };

    // 2. Vertex input
    VkVertexInputBindingDescription bindingDescription = VertexPositionColor::GetBindingDescription();
    auto attributeDescriptions = VertexPositionColor::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputState{};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &bindingDescription;
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputState.pVertexAttributeDescriptions = attributeDescriptions.data();

    // 3. Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    //inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    // 4. Viewport & scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    //scissor.offset = { 0, 0 };
    scissor.extent = { width, height };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 5. Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizationState{};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    // Match typical NDC vertex winding (CCW) for your simple triangle
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    // 6. Multisampling
    VkPipelineMultisampleStateCreateInfo multisampleState{};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = sampleCount;
    multisampleState.sampleShadingEnable = VK_FALSE;

    // 7. Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    //colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    //colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_COPY;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;

    // 8. Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
    //depthStencilState.depthBoundsTestEnable = VK_FALSE;
    //depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.maxDepthBounds = 1.0f;

    // 9. Graphics pipeline create info
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }

#ifdef ENABLE_VK_DEBUG
std::cout << "PIPELINE DEBUG: sampleCount=" << static_cast<int>(sampleCount)
          << " rasterCullMode=" << rasterizationState.cullMode << std::endl;

std::cout << "Vertex Binding: binding=" << bindingDescription.binding
          << " stride=" << bindingDescription.stride
          << " inputRate=" << bindingDescription.inputRate << std::endl;

for (size_t i = 0; i < attributeDescriptions.size(); ++i)
{
    const auto& a = attributeDescriptions[i];
    std::cout << "Attrib[" << i << "] location=" << a.location
              << " binding=" << a.binding
              << " format=" << a.format
              << " offset=" << a.offset << std::endl;
}


#endif
}

void VulkanInterop::CreateFramebuffer()
{
    VkImageView attachments[3] = { colorImageWithView.view, depthImageWithView.view, directView };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 3;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create framebuffer!");
    }
}

// ---------------- Command Buffer ----------------

void VulkanInterop::CreateCommandBuffer()
{
    // Allocate command buffer
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    if (res != VK_SUCCESS)
    {
        throw std::runtime_error("vkAllocateCommandBuffers failed");
    }

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (res != VK_SUCCESS)
    {
        throw std::runtime_error("vkBeginCommandBuffer failed");
    }

    // Clear values (exactly 3, same order)
    VkClearValue clearValues[3]{};

    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };
    clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = directImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // add explicit transition for color attachment (debug / safety)
    VkImageMemoryBarrier colorBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    colorBarrier.srcAccessMask = 0;
    colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = colorImageWithView.image;
    colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorBarrier.subresourceRange.baseMipLevel = 0;
    colorBarrier.subresourceRange.levelCount = 1;
    colorBarrier.subresourceRange.baseArrayLayer = 0;
    colorBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &colorBarrier
    );

    // Render pass begin info
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.clearValueCount = 3;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = { width, height };

    vkCmdBeginRenderPass(
        commandBuffer,
        &renderPassBeginInfo,
        VK_SUBPASS_CONTENTS_INLINE
    );

    // Bind pipeline
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline
    );

    // Bind vertex buffer
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
        commandBuffer,
        0,
        1,
        &vertexBuffer,
        offsets
    );

    // Bind index buffer
    vkCmdBindIndexBuffer(
        commandBuffer,
        indexBuffer,
        0,
        VK_INDEX_TYPE_UINT32
    );

    // Bind descriptor set
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &descriptorSet,
        0,
        nullptr
    );

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
			
    // End render pass
				vkCmdEndRenderPass(commandBuffer);

    // End command buffer
    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
    {
        throw std::runtime_error("vkEndCommandBuffer failed");
    }
}

bool VulkanInterop::LoadModelFromStream(const std::string& filename, std::vector<VertexPositionColor>& outVertices, std::vector<uint32_t>& outIndices)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) std::cout << "Warning: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Error: " << err << std::endl;
    if (!ret) return false;

    // 遍历场景中的网格
    for (const auto& scene : model.scenes)
    {
        for (int nodeIndex : scene.nodes)
        {
            const tinygltf::Node& node = model.nodes[nodeIndex];
            if (node.mesh >= 0)
            {
                const tinygltf::Mesh& mesh = model.meshes[node.mesh];

                for (const auto& primitive : mesh.primitives)
                {
                    // 读取 POSITION
                    const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
                    const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
                    const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

                    // 读取 NORMAL
                    const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
                    const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];
                    const float* normals = reinterpret_cast<const float*>(&normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);

                    // 添加顶点
                    for (size_t i = 0; i < posAccessor.count; i++)
                    {
                        VertexPositionColor v;
                        v.position = glm::vec3(
                            positions[i * 3 + 0],
                            positions[i * 3 + 1],
                            positions[i * 3 + 2]
                        );
                        v.color = glm::vec3(
                            normals[i * 3 + 0],
                            normals[i * 3 + 1],
                            normals[i * 3 + 2]
                        );
                        outVertices.push_back(v);
                    }

                    // 读取索引
                    if (primitive.indices >= 0)
                    {
                        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
                        const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

                        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        {
                            const uint32_t* indices = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                            outIndices.insert(outIndices.end(), indices, indices + indexAccessor.count);
                        }
                        else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        {
                            const uint16_t* indices = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                            outIndices.insert(outIndices.end(), indices, indices + indexAccessor.count);
                        }
                        else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        {
                            const uint8_t* indices = reinterpret_cast<const uint8_t*>(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset]);
                            outIndices.insert(outIndices.end(), indices, indices + indexAccessor.count);
                        }
                    }
                }
            }
        }
    }

    return true;
}

void VulkanInterop::CreateDebugUtilsMessenger()
{

    VkDebugUtilsMessengerCreateInfoEXT dbgCreate{};
    dbgCreate.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbgCreate.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT|
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbgCreate.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbgCreate.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* /*userData*/)->VkBool32
        {
            std::cerr << "VULKAN VALIDATION: " << (callbackData ? callbackData->pMessage : "null") << std::endl;
            return VK_FALSE;
        };

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        func(instance, &dbgCreate, nullptr, &debugMessenger);
    }
    else {
        std::cerr << "vkCreateDebugUtilsMessengerEXT not found" << std::endl;
    }
}

void VulkanInterop::Initialize(
    HANDLE directTextureHandle,
    uint64_t targetDeviceLuid,
    uint32_t w,
    uint32_t h,
    VkFormat format,
    VkExternalMemoryHandleTypeFlagBits handleType,
    const char* modelFilePath)
{
    width = w;
    height = h;
    targetFormat = format;
    targetHandleType = handleType;

    // =========================================================
    // Create instance
    // =========================================================
    // --- Instance + validation + debug utils ---
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    const char* debugUtilsExt = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    // Enable validation layers and debug utils when available (debug build)
    std::vector<const char*> instanceExtensions = { /* keep existing as needed */ };
    instanceExtensions.push_back(debugUtilsExt);

    // Instance create
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &appInfo;

    // enable validation layer
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = &validationLayer;
			
    // enable debug utils extension
    ici.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    ici.ppEnabledExtensionNames = instanceExtensions.data();

    VK_CHECK(vkCreateInstance(&ici, nullptr, &instance));

    CreateDebugUtilsMessenger();

    std::cout << "Vulkan instance: 0x"
        << std::hex << (uint64_t)instance << std::dec << std::endl;

    // =========================================================
    // Pick physical device
    // =========================================================
    uint32_t queueIndex = 0;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (auto dev : devices)
    {
        VkPhysicalDeviceIDProperties idProps{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };

        VkPhysicalDeviceProperties2 props2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        props2.pNext = &idProps;

        vkGetPhysicalDeviceProperties2(dev, &props2);

        if (CheckGraphicsQueue(dev, queueIndex)
            && CheckExternalMemoryExtension(dev)
            && CheckExternalImageHandleType(dev, targetFormat, handleType)
            && memcmp(idProps.deviceLUID, &targetDeviceLuid, sizeof(LUID)) == 0)
        {
            physicalDevice = dev;
            physicalDeviceProperties = props2.properties;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("Suitable device not found");

    // ---------------------------------------------------------
    // Depth format
    // ---------------------------------------------------------
    depthFormat = FindSupportedFormat(
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // ---------------------------------------------------------
    // Sample count
    // ---------------------------------------------------------
    VkSampleCountFlags sampleCounts =
        physicalDeviceProperties.limits.framebufferDepthSampleCounts &
        physicalDeviceProperties.limits.framebufferColorSampleCounts;

    if (sampleCounts & VK_SAMPLE_COUNT_8_BIT)      sampleCount = VK_SAMPLE_COUNT_8_BIT;
    else if (sampleCounts & VK_SAMPLE_COUNT_4_BIT) sampleCount = VK_SAMPLE_COUNT_4_BIT;
    else if (sampleCounts & VK_SAMPLE_COUNT_2_BIT) sampleCount = VK_SAMPLE_COUNT_2_BIT;
    else                                           sampleCount = VK_SAMPLE_COUNT_1_BIT;

    // DEBUG: force disable MSAA to rule out multisample/resolve issues
    //sampleCount = VK_SAMPLE_COUNT_1_BIT;

    std::cout << physicalDeviceProperties.deviceName
        << " having external memory extension: 0x"
        << std::hex << (uint64_t)physicalDevice << std::dec << std::endl;

    // =========================================================
    // Create device
    // =========================================================
    float priority = 1.0f;

    VkDeviceQueueCreateInfo qci{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr };
    qci.queueFamilyIndex = queueIndex;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    const char* extensions[] = {
        interopExtensionName  // VK_KHR_external_memory_win32
    };

    VkDeviceCreateInfo dci{
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO , nullptr};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = extensions;

    VK_CHECK(vkCreateDevice(physicalDevice, &dci, nullptr, &device));

    std::cout << "Vulkan device: 0x"
        << std::hex << (uint64_t)device << std::dec << std::endl;

    vkGetDeviceQueue(device, queueIndex, 0, &queue);

    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO , nullptr};
    VK_CHECK(vkCreateFence(device, &fci, nullptr, &fence));

    VkCommandPoolCreateInfo cpci{
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO , nullptr};
    cpci.queueFamilyIndex = queueIndex;
    VK_CHECK(vkCreateCommandPool(device, &cpci, nullptr, &commandPool));

    // =========================================================
    // Create shader modules
    // =========================================================
    auto vertCode = ReadFile("D:\\Repos\\vulkan-interop-directx\\VulkanCPP\\shaders\\shader.vert.spv");
    auto fragCode = ReadFile("D:\\Repos\\vulkan-interop-directx\\VulkanCPP\\shaders\\shader.frag.spv");

    vertexShaderModule = CreateShaderModule(vertCode);
    fragmentShaderModule = CreateShaderModule(fragCode);

    // =========================================================
    // Create render pass (MSAA + depth + resolve)
    // =========================================================
    VkAttachmentDescription attachments[3]{};

    // change in Initialize(...) where attachments[0] is set
    // Color
    attachments[0].format = targetFormat;
    attachments[0].samples = sampleCount;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // If we're single-sampled we must STORE the color attachment to preserve pixels
    attachments[0].storeOp = (sampleCount == VK_SAMPLE_COUNT_1_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth
    attachments[1].format = depthFormat;
    attachments[1].samples = sampleCount;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Resolve
    attachments[2].format = targetFormat;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // 必须 STORE 才能把 resolve 结果保留给外部消费者（DirectX）
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout =
        VK_IMAGE_LAYOUT_GENERAL;

    // Print renderpass attachment info used to create renderPass (debug)
    std::cout << "RenderPass attachments: color(0)=format(" << attachments[0].format << ") samples=" << attachments[0].samples
        << " depth(1)=format(" << attachments[1].format << ") samples=" << attachments[1].samples
        << " resolve(2)=format(" << attachments[2].format << ") samples=" << attachments[2].samples
        << std::endl;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    // Conditionally set resolveRef. If sampleCount == 1, mark as UNUSED to satisfy the spec.
    VkAttachmentReference resolveRef{};
    if (sampleCount == VK_SAMPLE_COUNT_1_BIT)
    {
        resolveRef.attachment = VK_ATTACHMENT_UNUSED;
        resolveRef.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    else
    {
        resolveRef.attachment = 2;
        resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    // pResolveAttachments may contain VK_ATTACHMENT_UNUSED entries — valid when MSAA disabled
    subpass.pResolveAttachments = &resolveRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 3;
    rpci.pAttachments = attachments;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dep;

    VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &renderPass));

    // =========================================================
    // Load model (SharpGLTF → C++ side assumes preloaded data)
    // =========================================================
    LoadModelFromStream(modelFilePath, vertices, indices);

    /*vertices = { {{-1.0,-1.0,0},{1,0,0}}, {{3.0,-1.0,0},{0,1,0}}, {{-1.0,3.0,0},{0,0,1}} };
    indices = { 0,1,2 };*/

    // ---------------- Vertex / Index / Uniform Buffers ----------------
    /*std::vector<uint32_t> indices;
    std::vector<VertexPositionColor> vertices;*/

    // 假设你已经从 glTF/GLB 模型里填充了 indices 和 vertices
    // 示例：indices = {...}, vertices = {...}

    // Buffer sizes
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    VkDeviceSize vertexBufferSize = sizeof(VertexPositionColor) * vertices.size();
    VkDeviceSize uniformBufferSize = sizeof(ModelViewProjection);

    // 创建 index buffer 并填充数据
    CreateBuffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer, indexMemory);

    void* data;
    vkMapMemory(device, indexMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(device, indexMemory);

    // 创建 vertex buffer 并填充数据
    CreateBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer, vertexMemory);

    vkMapMemory(device, vertexMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(device, vertexMemory);

    // Debug: print uploaded vertex buffer contents (add immediately after vkUnmapMemory for vertex buffer)
{
    //void* debugMapped = nullptr;
    //VK_CHECK(vkMapMemory(device, vertexMemory, 0, vertexBufferSize, 0, &debugMapped));
    //auto* v = reinterpret_cast<VertexPositionColor*>(debugMapped);
    /*size_t count = std::min<size_t>(3, vertices.size());
    for (size_t i = 0; i < count; ++i)
    {
        std::cout << "VTX[" << i << "] pos = (" << v[i].position.x << "," << v[i].position.y << "," << v[i].position.z
                  << ") color = (" << v[i].color.r << "," << v[i].color.g << "," << v[i].color.b << ")" << std::endl;
    }*/

    // Sanity: print struct size and attribute offsets used by the pipeline
    std::cout << "sizeof(VertexPositionColor) = " << sizeof(VertexPositionColor)
              << " posOffset = " << offsetof(VertexPositionColor, position)
              << " colorOffset = " << offsetof(VertexPositionColor, color)
              << std::endl;

    //vkUnmapMemory(device, vertexMemory);
}

    // 创建 uniform buffer
    CreateBuffer(uniformBufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uniformBuffer, uniformMemory);

    /*modelViewProjection.model = glm::mat4(1.0f);
    modelViewProjection.view = glm::mat4(1.0f);
    modelViewProjection.proj = glm::mat4(1.0f);*/

    VK_CHECK(vkMapMemory(device, uniformMemory, 0, sizeof(ModelViewProjection), 0, &data));
    std::memcpy(data, &modelViewProjection, sizeof(ModelViewProjection));
    vkUnmapMemory(device, uniformMemory);

    // ---------------- Descriptor Pool ----------------
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool!");

    // ---------------- Descriptor Set Layout ----------------
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &layoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout!");

    // ---------------- Descriptor Set ----------------
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set!");

    // 更新 descriptor set
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.range = sizeof(ModelViewProjection);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    // =========================================================
    // Pipeline layout
    // =========================================================
    VkPipelineLayoutCreateInfo plci{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &descriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout));

    // Debug: 打印关键句柄，便于确认 descriptor / pipeline layout / uniform buffer 等已创建
    std::cout << "DescriptorPool: " << descriptorPool
        << " DescriptorSetLayout: " << descriptorSetLayout
        << " DescriptorSet: " << descriptorSet
        << " UniformBuffer: " << uniformBuffer << std::endl;

    // =========================================================
    // External image + pipeline + framebuffer
    // =========================================================
    CreateImageViews(directTextureHandle);
    CreatePipeline();
    CreateFramebuffer();
    CreateCommandBuffer();

    // =========================================================
    // Initial MVP
    // =========================================================
    // Debug: force identity MVP for isolation (temporary)

    modelViewProjection.model = glm::mat4(1.0f);
    modelViewProjection.view =
        glm::lookAt(glm::vec3(5, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, 1));
    modelViewProjection.proj =
        glm::perspective(glm::radians(45.0f),
            float(width) / float(height),
            0.001f, 10000.0f);
}

void VulkanInterop::SubmitWork(const void* pNext)
{
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = pNext;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit queue!");

    vkQueueWaitIdle(queue);

    // DEBUG: verify direct/resolve image contents on CPU
    //ReadbackResolveImageAndPrint();

    vkResetFences(device, 1, &fence);
}

void VulkanInterop::UpdateModelViewProjection(float time)
{
    // 更新 Model 矩阵
    modelViewProjection.model= glm::rotate(glm::mat4(1.0f), time * RotationSpeed, glm::vec3(0.0f, 0.0f, 1.0f));

    void* data = nullptr;

    // 映射 Uniform Buffer 内存
    if (vkMapMemory(device, uniformMemory, 0, sizeof(ModelViewProjection), 0, &data) != VK_SUCCESS)
        throw std::runtime_error("Failed to map uniform buffer memory");

    // 复制数据到映射内存
    std::memcpy(data, &modelViewProjection, sizeof(ModelViewProjection));

    // 解除映射
    vkUnmapMemory(device, uniformMemory);
}


void VulkanInterop::Draw(float time)
{
    UpdateModelViewProjection(time);
    SubmitWork();
}

void VulkanInterop::ReleaseSizeDependentResources()
{
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr);

    vkDestroyImageView(device, colorImageWithView.view, nullptr);
    vkDestroyImageView(device, depthImageWithView.view, nullptr);
    vkDestroyImageView(device, directView, nullptr);

    vkDestroyImage(device, colorImageWithView.image, nullptr);
    vkDestroyImage(device, depthImageWithView.image, nullptr);
    vkDestroyImage(device, directImage, nullptr);

    /*vkFreeMemory(device, colorImageMemory, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);
    vkFreeMemory(device, directImageMemory, nullptr);*/

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanInterop::Resize(HANDLE directTextureMemory, uint32_t newWidth, uint32_t newHeight)
{
    width = newWidth;
    height = newHeight;

    ReleaseSizeDependentResources();

    CreateImageViews(directTextureMemory);
    CreatePipeline();
    CreateFramebuffer();
    CreateCommandBuffer();

    modelViewProjection.proj= glm::perspective(glm::radians(45.0f), float(width) / height, 0.001f, 10000.0f);
}

void VulkanInterop::Clear()
{
    ReleaseSizeDependentResources();

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkDestroyBuffer(device, uniformBuffer, nullptr);

    vkFreeMemory(device, vertexMemory, nullptr);
    vkFreeMemory(device, indexMemory, nullptr);
    vkFreeMemory(device, uniformMemory, nullptr);

    vkDestroyRenderPass(device, renderPass, nullptr);

    vkDestroyShaderModule(device, vertexShaderModule, nullptr);
    vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyFence(device, fence, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

// ============================================================
// END
// ============================================================
void VulkanInterop::ReadbackResolveImageAndPrint()
{
    if (targetFormat != VK_FORMAT_R8G8B8A8_UNORM && targetFormat != VK_FORMAT_B8G8R8A8_UNORM)
    {
        std::cout << "Readback: unsupported format for quick debug: " << targetFormat << std::endl;
        return;
    }

    const uint32_t bpp = 4;
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * bpp;

    // Choose the source image and its expected layout.
    // If MSAA disabled (sampleCount == 1) the color attachment (colorImageWithView.image)
    // contains the rendered pixels. If MSAA is enabled, the resolve image (directImage)
    // holds the resolved pixels.
    VkImage srcImage = (sampleCount == VK_SAMPLE_COUNT_1_BIT) ? colorImageWithView.image : directImage;
    VkImageLayout srcOldLayout = (sampleCount == VK_SAMPLE_COUNT_1_BIT)
        ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_GENERAL;

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = GetMemoryTypeIndex(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory));
    VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    // One-time command buffer
    VkCommandBufferAllocateInfo cbAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbAlloc.commandPool = commandPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cb;
    VK_CHECK(vkAllocateCommandBuffers(device, &cbAlloc, &cb));

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cb, &beginInfo));

    // Barrier: srcOldLayout -> TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = srcOldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    // Copy image -> buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cb, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    // Restore layout: TRANSFER_SRC_OPTIMAL -> srcOldLayout
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = srcOldLayout;
    vkCmdPipelineBarrier(cb,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cb));

    // Submit and wait
    VK_CHECK(vkResetFences(device, 1, &fence));
    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cb;
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &fence));

    // Map and print first pixels
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mapped));
    unsigned char* pixels = reinterpret_cast<unsigned char*>(mapped);

    const uint32_t printCount = std::min<uint32_t>(16, width * height);
    for (uint32_t i = 0; i < printCount; ++i)
    {
        uint32_t idx = i * bpp;
        printf("READBACK pixel %u: R=%u G=%u B=%u A=%u\n", i,
            pixels[idx + 0], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]);
    }

    vkUnmapMemory(device, stagingMemory);

    // Cleanup
    vkFreeCommandBuffers(device, commandPool, 1, &cb);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}
