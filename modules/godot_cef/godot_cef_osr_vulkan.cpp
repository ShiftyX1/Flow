/**************************************************************************/
/*  godot_cef_osr_vulkan.cpp                                              */
/**************************************************************************/

#include "godot_cef_osr.h"

#include "core/templates/local_vector.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <windows.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

class GodotCefVulkanBackend : public GodotCefAcceleratedBackend {
	HMODULE vulkan_lib = nullptr;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queue_family = 0;
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool reported_format_error = false;

	PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = nullptr;
	PFN_vkCreateImage vkCreateImage = nullptr;
	PFN_vkDestroyImage vkDestroyImage = nullptr;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
	PFN_vkAllocateMemory vkAllocateMemory = nullptr;
	PFN_vkFreeMemory vkFreeMemory = nullptr;
	PFN_vkBindImageMemory vkBindImageMemory = nullptr;
	PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
	PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
	PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
	PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
	PFN_vkCmdCopyImage vkCmdCopyImage = nullptr;
	PFN_vkQueueSubmit vkQueueSubmit = nullptr;
	PFN_vkWaitForFences vkWaitForFences = nullptr;
	PFN_vkResetFences vkResetFences = nullptr;
	PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
	PFN_vkCreateFence vkCreateFence = nullptr;
	PFN_vkDestroyFence vkDestroyFence = nullptr;
	PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
	PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR = nullptr;

	template <typename T>
	T _load(const char *p_name) const {
		return reinterpret_cast<T>(vkGetDeviceProcAddr(device, p_name));
	}

	bool _load_functions() {
		vkCreateImage = _load<PFN_vkCreateImage>("vkCreateImage");
		vkDestroyImage = _load<PFN_vkDestroyImage>("vkDestroyImage");
		vkGetImageMemoryRequirements = _load<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements");
		vkAllocateMemory = _load<PFN_vkAllocateMemory>("vkAllocateMemory");
		vkFreeMemory = _load<PFN_vkFreeMemory>("vkFreeMemory");
		vkBindImageMemory = _load<PFN_vkBindImageMemory>("vkBindImageMemory");
		vkCreateCommandPool = _load<PFN_vkCreateCommandPool>("vkCreateCommandPool");
		vkDestroyCommandPool = _load<PFN_vkDestroyCommandPool>("vkDestroyCommandPool");
		vkAllocateCommandBuffers = _load<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers");
		vkBeginCommandBuffer = _load<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
		vkEndCommandBuffer = _load<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
		vkCmdPipelineBarrier = _load<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
		vkCmdCopyImage = _load<PFN_vkCmdCopyImage>("vkCmdCopyImage");
		vkQueueSubmit = _load<PFN_vkQueueSubmit>("vkQueueSubmit");
		vkWaitForFences = _load<PFN_vkWaitForFences>("vkWaitForFences");
		vkResetFences = _load<PFN_vkResetFences>("vkResetFences");
		vkResetCommandBuffer = _load<PFN_vkResetCommandBuffer>("vkResetCommandBuffer");
		vkCreateFence = _load<PFN_vkCreateFence>("vkCreateFence");
		vkDestroyFence = _load<PFN_vkDestroyFence>("vkDestroyFence");
		vkGetDeviceQueue = _load<PFN_vkGetDeviceQueue>("vkGetDeviceQueue");
		vkGetMemoryWin32HandlePropertiesKHR = _load<PFN_vkGetMemoryWin32HandlePropertiesKHR>("vkGetMemoryWin32HandlePropertiesKHR");
		return vkCreateImage && vkDestroyImage && vkGetImageMemoryRequirements && vkAllocateMemory && vkFreeMemory &&
				vkBindImageMemory && vkCreateCommandPool && vkDestroyCommandPool && vkAllocateCommandBuffers &&
				vkBeginCommandBuffer && vkEndCommandBuffer && vkCmdPipelineBarrier && vkCmdCopyImage &&
				vkQueueSubmit && vkWaitForFences && vkResetFences && vkResetCommandBuffer && vkCreateFence &&
				vkDestroyFence && vkGetDeviceQueue && vkGetMemoryWin32HandlePropertiesKHR;
	}

	void _destroy_pool() {
		if (device == VK_NULL_HANDLE) {
			return;
		}
		if (fence != VK_NULL_HANDLE && vkDestroyFence) {
			vkDestroyFence(device, fence, nullptr);
			fence = VK_NULL_HANDLE;
		}
		if (command_pool != VK_NULL_HANDLE && vkDestroyCommandPool) {
			vkDestroyCommandPool(device, command_pool, nullptr);
			command_pool = VK_NULL_HANDLE;
			command_buffer = VK_NULL_HANDLE;
		}
	}

public:
	explicit GodotCefVulkanBackend(HMODULE p_lib, VkDevice p_device, PFN_vkGetDeviceProcAddr p_get_proc) :
			vulkan_lib(p_lib), device(p_device), vkGetDeviceProcAddr(p_get_proc) {}

	~GodotCefVulkanBackend() override {
		_destroy_pool();
		if (vulkan_lib) {
			FreeLibrary(vulkan_lib);
		}
	}

	bool initialize(uint32_t p_queue_family, uint32_t p_queue_index) {
		if (!_load_functions()) {
			return false;
		}
		queue_family = p_queue_family;
		vkGetDeviceQueue(device, p_queue_family, p_queue_index, &queue);
		if (queue == VK_NULL_HANDLE) {
			return false;
		}

		VkCommandPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = p_queue_family;
		if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
			return false;
		}

		VkCommandBufferAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
			return false;
		}

		VkFenceCreateInfo fence_info = {};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		return vkCreateFence(device, &fence_info, nullptr, &fence) == VK_SUCCESS;
	}

	const char *get_name() const override { return "Vulkan/Win32"; }

	Size2i get_shared_texture_size(const CefAcceleratedPaintInfo &p_info) override {
		return Size2i(int(p_info.extra.coded_size.width), int(p_info.extra.coded_size.height));
	}

	bool copy_shared_texture(const CefAcceleratedPaintInfo &p_info, RID p_dst, const Size2i &p_size) override {
		const HANDLE handle = p_info.shared_texture_handle;
		if (!handle || handle == INVALID_HANDLE_VALUE || p_size.x <= 0 || p_size.y <= 0) {
			return false;
		}
		if (p_info.format != CEF_COLOR_TYPE_BGRA_8888) {
			if (!reported_format_error) {
				reported_format_error = true;
				ERR_PRINT(vformat("Godot CEF: unsupported shared texture format %d.", int(p_info.format)));
			}
			return false;
		}

		RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
		const uint64_t dst_handle = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_TEXTURE, p_dst);
		if (dst_handle == 0) {
			return false;
		}
		const VkImage dst_image = reinterpret_cast<VkImage>(dst_handle);

		HANDLE duplicated = nullptr;
		if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			return false;
		}

		VkExternalMemoryImageCreateInfo external_image = {};
		external_image.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
		external_image.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;

		VkImageCreateInfo image_info = {};
		image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_info.pNext = &external_image;
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.format = VK_FORMAT_B8G8R8A8_SRGB;
		image_info.extent = { uint32_t(p_size.x), uint32_t(p_size.y), 1 };
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImage src_image = VK_NULL_HANDLE;
		if (vkCreateImage(device, &image_info, nullptr, &src_image) != VK_SUCCESS) {
			CloseHandle(duplicated);
			return false;
		}

		VkMemoryWin32HandlePropertiesKHR handle_props = {};
		handle_props.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
		if (vkGetMemoryWin32HandlePropertiesKHR(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT, duplicated, &handle_props) != VK_SUCCESS) {
			vkDestroyImage(device, src_image, nullptr);
			CloseHandle(duplicated);
			return false;
		}

		VkMemoryRequirements requirements = {};
		vkGetImageMemoryRequirements(device, src_image, &requirements);
		const uint32_t type_bits = requirements.memoryTypeBits & handle_props.memoryTypeBits;
		if (type_bits == 0) {
			vkDestroyImage(device, src_image, nullptr);
			CloseHandle(duplicated);
			return false;
		}

		VkImportMemoryWin32HandleInfoKHR import_info = {};
		import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
		import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
		import_info.handle = duplicated;

		VkMemoryDedicatedAllocateInfo dedicated = {};
		dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
		dedicated.pNext = &import_info;
		dedicated.image = src_image;

		VkMemoryAllocateInfo alloc_info = {};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = &dedicated;
		alloc_info.allocationSize = requirements.size;
		uint32_t memory_type = 0;
		for (uint32_t bits = type_bits; (bits & 1u) == 0; bits >>= 1) {
			memory_type++;
		}
		alloc_info.memoryTypeIndex = memory_type;

		VkDeviceMemory memory = VK_NULL_HANDLE;
		if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
			vkDestroyImage(device, src_image, nullptr);
			CloseHandle(duplicated);
			return false;
		}
		if (vkBindImageMemory(device, src_image, memory, 0) != VK_SUCCESS) {
			vkFreeMemory(device, memory, nullptr);
			vkDestroyImage(device, src_image, nullptr);
			CloseHandle(duplicated);
			return false;
		}

		vkResetFences(device, 1, &fence);
		vkResetCommandBuffer(command_buffer, 0);

		VkCommandBufferBeginInfo begin_info = {};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
			vkFreeMemory(device, memory, nullptr);
			vkDestroyImage(device, src_image, nullptr);
			CloseHandle(duplicated);
			return false;
		}

		const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkImageMemoryBarrier barriers[2] = {};
		barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].image = src_image;
		barriers[0].subresourceRange = range;
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[1].image = dst_image;
		barriers[1].subresourceRange = range;
		barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

		VkImageCopy region = {};
		region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.extent = { uint32_t(p_size.x), uint32_t(p_size.y), 1 };
		vkCmdCopyImage(command_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier final_barrier = {};
		final_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		final_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		final_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		final_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		final_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		final_barrier.image = dst_image;
		final_barrier.subresourceRange = range;
		final_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		final_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &final_barrier);
		vkEndCommandBuffer(command_buffer);

		VkSubmitInfo submit = {};
		submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &command_buffer;
		const bool ok = vkQueueSubmit(queue, 1, &submit, fence) == VK_SUCCESS &&
				vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;

		vkDestroyImage(device, src_image, nullptr);
		vkFreeMemory(device, memory, nullptr);
		CloseHandle(duplicated);
		return ok;
	}
};

static void _choose_copy_queue(HMODULE p_lib, VkPhysicalDevice p_physical, uint32_t &r_family, uint32_t &r_index) {
	r_family = 0;
	r_index = 0;
	if (p_physical == VK_NULL_HANDLE) {
		return;
	}
	const auto get_props = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(GetProcAddress(p_lib, "vkGetPhysicalDeviceQueueFamilyProperties"));
	if (!get_props) {
		return;
	}
	uint32_t count = 0;
	get_props(p_physical, &count, nullptr);
	if (count == 0) {
		return;
	}
	LocalVector<VkQueueFamilyProperties> props;
	props.resize(count);
	get_props(p_physical, &count, props.ptr());
	if (props[0].queueCount > 1) {
		r_index = 1;
		return;
	}
	for (uint32_t i = 0; i < count; i++) {
		const bool transfer = props[i].queueFlags & VK_QUEUE_TRANSFER_BIT;
		const bool graphics = props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		if (transfer && !graphics && props[i].queueCount > 0) {
			r_family = i;
			r_index = 0;
			return;
		}
	}
}

GodotCefAcceleratedBackend *godot_cef_create_vulkan_backend() {
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return nullptr;
	}
	const uint64_t device_handle = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE);
	if (device_handle == 0) {
		return nullptr;
	}

	HMODULE lib = LoadLibraryW(L"vulkan-1.dll");
	if (!lib) {
		return nullptr;
	}
	const auto get_device_proc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(lib, "vkGetDeviceProcAddr"));
	if (!get_device_proc) {
		FreeLibrary(lib);
		return nullptr;
	}

	GodotCefVulkanBackend *backend = memnew(GodotCefVulkanBackend(lib, reinterpret_cast<VkDevice>(device_handle), get_device_proc));
	uint32_t family = 0;
	uint32_t index = 0;
	_choose_copy_queue(lib, reinterpret_cast<VkPhysicalDevice>(rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE)), family, index);
	if (!backend->initialize(family, index)) {
		memdelete(backend);
		return nullptr;
	}
	return backend;
}

bool godot_cef_vulkan_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id) {
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return false;
	}
	const uint64_t physical_handle = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_PHYSICAL_DEVICE);
	if (physical_handle == 0) {
		return false;
	}
	HMODULE lib = LoadLibraryW(L"vulkan-1.dll");
	if (!lib) {
		return false;
	}
	const auto get_props = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(GetProcAddress(lib, "vkGetPhysicalDeviceProperties"));
	if (!get_props) {
		FreeLibrary(lib);
		return false;
	}
	VkPhysicalDeviceProperties props = {};
	get_props(reinterpret_cast<VkPhysicalDevice>(physical_handle), &props);
	r_vendor_id = props.vendorID;
	r_device_id = props.deviceID;
	FreeLibrary(lib);
	return r_vendor_id != 0;
}
