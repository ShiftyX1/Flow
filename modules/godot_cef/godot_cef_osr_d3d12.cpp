/**************************************************************************/
/*  godot_cef_osr_d3d12.cpp                                               */
/**************************************************************************/

#include "godot_cef_osr.h"

#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi.h>
#include <utility>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class GodotCefD3D12Backend : public GodotCefAcceleratedBackend {
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12Fence> fence;
	ComPtr<ID3D11Device> d3d11;
	ComPtr<ID3D11DeviceContext> d3d11_context;
	ComPtr<ID3D11On12Device> d3d11on12;
	HANDLE fence_event = nullptr;
	UINT64 fence_value = 0;
	bool reported_format_error = false;

	void _wait_for_gpu() {
		if (!fence || !queue || !fence_event) {
			return;
		}
		fence_value++;
		if (FAILED(queue->Signal(fence.Get(), fence_value))) {
			return;
		}
		if (fence->GetCompletedValue() < fence_value) {
			if (SUCCEEDED(fence->SetEventOnCompletion(fence_value, fence_event))) {
				WaitForSingleObject(fence_event, INFINITE);
			}
		}
	}

public:
	GodotCefD3D12Backend(ComPtr<ID3D12Device> p_device, ComPtr<ID3D12CommandQueue> p_queue, ComPtr<ID3D12Fence> p_fence, ComPtr<ID3D11Device> p_d3d11, ComPtr<ID3D11DeviceContext> p_context, ComPtr<ID3D11On12Device> p_d3d11on12, HANDLE p_event) :
			device(std::move(p_device)),
			queue(std::move(p_queue)),
			fence(std::move(p_fence)),
			d3d11(std::move(p_d3d11)),
			d3d11_context(std::move(p_context)),
			d3d11on12(std::move(p_d3d11on12)),
			fence_event(p_event) {}

	~GodotCefD3D12Backend() override {
		_wait_for_gpu();
		if (fence_event) {
			CloseHandle(fence_event);
		}
	}

	const char *get_name() const override { return "D3D12/D3D11On12"; }

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

		HANDLE duplicated = nullptr;
		if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
			return false;
		}

		ComPtr<ID3D11Device1> d3d11_1;
		ComPtr<ID3D11Texture2D> src;
		if (FAILED(d3d11.As(&d3d11_1)) || FAILED(d3d11_1->OpenSharedResource1(duplicated, IID_PPV_ARGS(&src)))) {
			CloseHandle(duplicated);
			return false;
		}

		ID3D12Resource *dst = reinterpret_cast<ID3D12Resource *>(dst_handle);
		D3D11_RESOURCE_FLAGS flags = {};
		flags.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		ComPtr<ID3D11Resource> wrapped;
		const HRESULT wrap = d3d11on12->CreateWrappedResource(dst, &flags, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON, IID_PPV_ARGS(&wrapped));
		if (FAILED(wrap) || !wrapped) {
			CloseHandle(duplicated);
			return false;
		}

		ID3D11Resource *resources[] = { wrapped.Get() };
		d3d11on12->AcquireWrappedResources(resources, 1);
		d3d11_context->CopyResource(wrapped.Get(), src.Get());
		d3d11on12->ReleaseWrappedResources(resources, 1);
		d3d11_context->Flush();
		_wait_for_gpu();
		CloseHandle(duplicated);
		return true;
	}
};

static ID3D12Device *_get_godot_d3d12_device() {
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return nullptr;
	}
	const uint64_t handle = rd->get_driver_resource(RenderingDevice::DRIVER_RESOURCE_LOGICAL_DEVICE);
	return handle ? reinterpret_cast<ID3D12Device *>(handle) : nullptr;
}

GodotCefAcceleratedBackend *godot_cef_create_d3d12_backend() {
	ID3D12Device *godot_device = _get_godot_d3d12_device();
	if (!godot_device) {
		return nullptr;
	}

	ComPtr<ID3D12Device> device = godot_device;
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue> queue;
	if (FAILED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)))) {
		return nullptr;
	}

	ComPtr<ID3D12Fence> fence;
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
		return nullptr;
	}
	HANDLE fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!fence_event) {
		return nullptr;
	}

	IUnknown *queues[] = { queue.Get() };
	ComPtr<ID3D11Device> d3d11;
	ComPtr<ID3D11DeviceContext> context;
	if (FAILED(D3D11On12CreateDevice(device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0, &d3d11, &context, nullptr))) {
		CloseHandle(fence_event);
		return nullptr;
	}

	ComPtr<ID3D11On12Device> d3d11on12;
	if (FAILED(d3d11.As(&d3d11on12))) {
		CloseHandle(fence_event);
		return nullptr;
	}

	return memnew(GodotCefD3D12Backend(device, queue, fence, d3d11, context, d3d11on12, fence_event));
}

bool godot_cef_d3d12_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id) {
	ID3D12Device *device = _get_godot_d3d12_device();
	if (!device) {
		return false;
	}
	const LUID target = device->GetAdapterLuid();
	ComPtr<IDXGIFactory> factory;
	if (FAILED(CreateDXGIFactory(IID_PPV_ARGS(&factory)))) {
		return false;
	}
	for (UINT i = 0;; i++) {
		ComPtr<IDXGIAdapter> adapter;
		if (FAILED(factory->EnumAdapters(i, &adapter))) {
			break;
		}
		DXGI_ADAPTER_DESC desc = {};
		if (FAILED(adapter->GetDesc(&desc))) {
			continue;
		}
		if (desc.AdapterLuid.HighPart == target.HighPart && desc.AdapterLuid.LowPart == target.LowPart) {
			r_vendor_id = desc.VendorId;
			r_device_id = desc.DeviceId;
			return true;
		}
	}
	return false;
}
