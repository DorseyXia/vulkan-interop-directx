

#include "pch.h"
#include "InteropAPI.h"

#include "VulkanCpp.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class VulkanInterop;

struct RenderThreadContext {
	std::thread thread;
	std::mutex mutex;
	std::condition_variable cv;
	std::atomic<bool> running{ false };
	std::atomic<bool> frameRequested{ false };
	float time = 0.0f;
	VulkanInterop* vulkanInstance = nullptr;
	RenderCallback callback = nullptr;
};

static RenderThreadContext g_renderCtx;

static void RenderThreadFunc() {
	while (g_renderCtx.running) {
		std::unique_lock<std::mutex> lock(g_renderCtx.mutex);
		g_renderCtx.cv.wait(lock, [] {
			return g_renderCtx.frameRequested.load() || !g_renderCtx.running.load();
		});

		if (!g_renderCtx.running)
			break;

		float time = g_renderCtx.time;
		g_renderCtx.frameRequested = false;
		lock.unlock();

		g_renderCtx.vulkanInstance->Draw(time);

		if (g_renderCtx.callback) {
			g_renderCtx.callback();
		}
	}
}

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

INTEROP_API void VulkanInteropStartRenderLoop(void* instance, RenderCallback callback)
{
	if (g_renderCtx.running)
		return;

	g_renderCtx.vulkanInstance = static_cast<VulkanInterop*>(instance);
	g_renderCtx.callback = callback;
	g_renderCtx.running = true;
	g_renderCtx.frameRequested = false;
	g_renderCtx.thread = std::thread(RenderThreadFunc);
}

INTEROP_API void VulkanInteropStopRenderLoop(void* instance)
{
	g_renderCtx.running = false;
	g_renderCtx.cv.notify_one();
	if (g_renderCtx.thread.joinable())
		g_renderCtx.thread.join();
}

INTEROP_API void VulkanInteropRequestFrame(void* instance, float time)
{
	{
		std::lock_guard<std::mutex> lock(g_renderCtx.mutex);
		g_renderCtx.time = time;
		g_renderCtx.frameRequested = true;
	}
	g_renderCtx.cv.notify_one();
}

INTEROP_API void VulkanInteropResize(void* instance, HANDLE sharedTexture, uint32_t w, uint32_t h)
{
	VulkanInterop* vulkanInstance = static_cast<VulkanInterop*>(instance);
	vulkanInstance->Resize(sharedTexture, w, h);
}
