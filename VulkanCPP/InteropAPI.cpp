

#include "pch.h"
#include "InteropAPI.h"

#include "VulkanCpp.h"

class VulkanInterop;

INTEROP_API void* CreateVulkanInteropInstance()
{
	VulkanInterop* instance = new VulkanInterop();
	return static_cast<void*>(instance);
}

INTEROP_API void VulkanInteropInitialize(
	void* instance,
	HANDLE directTextureHandle,
	uint64_t targetDeviceLuid,
	uint32_t width,
	uint32_t height,
	uint32_t format,
	uint32_t handleType,
	const char* modelFilePath)
{
	VulkanInterop* vulkanInstance = static_cast<VulkanInterop*>(instance);
	vulkanInstance->Initialize(
		directTextureHandle,
		targetDeviceLuid,
		width,
		height,
		static_cast<VkFormat>(format),
		static_cast<VkExternalMemoryHandleTypeFlagBits>(handleType),
		modelFilePath
	);
}

INTEROP_API void VulkanInteropDraw(void* instance, float time)
{
	VulkanInterop* vulkanInstance = static_cast<VulkanInterop*>(instance);
	vulkanInstance->Draw(time);
}
INTEROP_API void VulkanInteropResize(void* instance, HANDLE sharedTexture, uint32_t w, uint32_t h)
{
	VulkanInterop* vulkanInstance = static_cast<VulkanInterop*>(instance);
	vulkanInstance->Resize(sharedTexture, w, h);
}

