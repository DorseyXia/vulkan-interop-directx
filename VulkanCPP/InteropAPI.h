#pragma once

#define INTEROP_API __declspec(dllexport)
#include <cstdint>

extern "C" {

	INTEROP_API void* CreateVulkanInteropInstance();

	INTEROP_API void VulkanInteropInitialize(
		void* instance,
		HANDLE directTextureHandle,
		uint64_t targetDeviceLuid,
		uint32_t width,
		uint32_t height,
		uint32_t format,
		uint32_t handleType,
		const char* modelFilePath);

	INTEROP_API void VulkanInteropDraw(void* instance, float time);
	INTEROP_API void VulkanInteropResize(void* instance, HANDLE sharedTexture, uint32_t w, uint32_t h);
}
