/**************************************************************************/
/*  godot_cef_osr_metal.mm                                                */
/**************************************************************************/

#include "godot_cef_osr.h"

#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#import <IOKit/IOKitLib.h>
#import <IOSurface/IOSurface.h>
#import <Metal/Metal.h>

class GodotCefMetalBackend : public GodotCefAcceleratedBackend {
	id<MTLDevice> device = nil;
	// A dedicated queue: the copy is waited on synchronously without stalling Godot's own queue.
	id<MTLCommandQueue> queue = nil;
	bool reported_format_error = false;

public:
	GodotCefMetalBackend(id<MTLDevice> p_device, id<MTLCommandQueue> p_queue) :
			device(p_device), queue(p_queue) {}

	const char *get_name() const override { return "Metal/IOSurface"; }

	Size2i get_shared_texture_size(const CefAcceleratedPaintInfo &p_info) override {
		IOSurfaceRef surface = (IOSurfaceRef)p_info.shared_texture_io_surface;
		if (!surface) {
			return Size2i();
		}
		return Size2i(int(IOSurfaceGetWidth(surface)), int(IOSurfaceGetHeight(surface)));
	}

	bool copy_shared_texture(const CefAcceleratedPaintInfo &p_info, RID p_dst, const Size2i &p_size) override {
		IOSurfaceRef surface = (IOSurfaceRef)p_info.shared_texture_io_surface;
		if (!surface || p_size.x <= 0 || p_size.y <= 0) {
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

		@autoreleasepool {
			id<MTLTexture> dst = (__bridge id<MTLTexture>)(void *)(uintptr_t)dst_handle;

			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB
																							 width:IOSurfaceGetWidth(surface)
																							height:IOSurfaceGetHeight(surface)
																						 mipmapped:NO];
			desc.usage = MTLTextureUsageShaderRead;
			desc.storageMode = device.hasUnifiedMemory ? MTLStorageModeShared : MTLStorageModeManaged;
			id<MTLTexture> src = [device newTextureWithDescriptor:desc iosurface:surface plane:0];
			if (!src) {
				return false;
			}

			const NSUInteger width = MIN(NSUInteger(p_size.x), MIN(src.width, dst.width));
			const NSUInteger height = MIN(NSUInteger(p_size.y), MIN(src.height, dst.height));

			id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
			id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
			[blit copyFromTexture:src
						  sourceSlice:0
						  sourceLevel:0
						 sourceOrigin:MTLOriginMake(0, 0, 0)
						   sourceSize:MTLSizeMake(width, height, 1)
							toTexture:dst
					 destinationSlice:0
					 destinationLevel:0
					destinationOrigin:MTLOriginMake(0, 0, 0)];
			[blit endEncoding];
			[command_buffer commit];
			[command_buffer waitUntilCompleted];
			return command_buffer.status == MTLCommandBufferStatusCompleted;
		}
	}
};

static id<MTLDevice> _get_godot_metal_device() {
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return nil;
	}
	const uint64_t handle = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE);
	if (handle == 0) {
		return nil;
	}
	return (__bridge id<MTLDevice>)(void *)(uintptr_t)handle;
}

GodotCefAcceleratedBackend *godot_cef_create_metal_backend() {
	id<MTLDevice> device = _get_godot_metal_device();
	if (!device) {
		return nullptr;
	}
	id<MTLCommandQueue> queue = [device newCommandQueue];
	if (!queue) {
		return nullptr;
	}
	queue.label = @"Godot CEF OSR copy";
	return memnew(GodotCefMetalBackend(device, queue));
}

static bool _read_registry_u32(io_registry_entry_t p_entry, CFStringRef p_key, uint32_t &r_value) {
	CFTypeRef property = IORegistryEntrySearchCFProperty(p_entry, kIOServicePlane, p_key, kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
	if (!property) {
		return false;
	}
	bool ok = false;
	if (CFGetTypeID(property) == CFDataGetTypeID() && CFDataGetLength((CFDataRef)property) >= 4) {
		const UInt8 *bytes = CFDataGetBytePtr((CFDataRef)property);
		r_value = uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
		ok = true;
	}
	CFRelease(property);
	return ok;
}

bool godot_cef_metal_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id) {
	id<MTLDevice> device = _get_godot_metal_device();
	if (!device || device.registryID == 0) {
		return false;
	}
	io_registry_entry_t service = IOServiceGetMatchingService(MACH_PORT_NULL, IORegistryEntryIDMatching(device.registryID));
	if (service == IO_OBJECT_NULL) {
		return false;
	}
	// Apple silicon GPUs have no PCI ids; CEF picks the only GPU anyway.
	const bool ok = _read_registry_u32(service, CFSTR("vendor-id"), r_vendor_id) && _read_registry_u32(service, CFSTR("device-id"), r_device_id);
	IOObjectRelease(service);
	return ok;
}
