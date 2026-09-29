/**************************************************************************/
/*  godot_cef_osr.cpp                                                     */
/**************************************************************************/

#include "godot_cef_osr.h"

#include "core/io/image.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_default.h"

#ifdef MACOS_ENABLED
GodotCefAcceleratedBackend *godot_cef_create_metal_backend();
bool godot_cef_metal_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id);
#endif
#ifdef WINDOWS_ENABLED
GodotCefAcceleratedBackend *godot_cef_create_d3d12_backend();
GodotCefAcceleratedBackend *godot_cef_create_vulkan_backend();
bool godot_cef_d3d12_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id);
bool godot_cef_vulkan_get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id);
#endif

static bool _can_use_rendering_device() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!rs || !rs->get_rendering_device()) {
		return false;
	}
	// Native copies and RD calls happen from CEF callbacks on the main thread.
	return rs->is_on_render_thread();
}

GodotCefAcceleratedBackend *GodotCefAcceleratedBackend::create() {
	if (!_can_use_rendering_device()) {
		return nullptr;
	}
	const String driver = RenderingServer::get_singleton()->get_current_rendering_driver_name();
#ifdef MACOS_ENABLED
	if (driver == "metal") {
		return godot_cef_create_metal_backend();
	}
#endif
#ifdef WINDOWS_ENABLED
	if (driver == "d3d12") {
		return godot_cef_create_d3d12_backend();
	}
	if (driver == "vulkan") {
		return godot_cef_create_vulkan_backend();
	}
#endif
	return nullptr;
}

bool GodotCefAcceleratedBackend::is_supported() {
	GodotCefAcceleratedBackend *backend = create();
	if (!backend) {
		return false;
	}
	memdelete(backend);
	return true;
}

bool GodotCefAcceleratedBackend::get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id) {
	if (!_can_use_rendering_device()) {
		return false;
	}
	const String driver = RenderingServer::get_singleton()->get_current_rendering_driver_name();
#ifdef MACOS_ENABLED
	if (driver == "metal") {
		return godot_cef_metal_get_gpu_ids(r_vendor_id, r_device_id);
	}
#endif
#ifdef WINDOWS_ENABLED
	if (driver == "d3d12") {
		return godot_cef_d3d12_get_gpu_ids(r_vendor_id, r_device_id);
	}
	if (driver == "vulkan") {
		return godot_cef_vulkan_get_gpu_ids(r_vendor_id, r_device_id);
	}
#endif
	(void)driver;
	return false;
}

void godot_cef_bgra_to_rgba(const uint8_t *p_src, uint8_t *p_dst, int64_t p_pixel_count) {
	const uint32_t *src = reinterpret_cast<const uint32_t *>(p_src);
	uint32_t *dst = reinterpret_cast<uint32_t *>(p_dst);
	for (int64_t i = 0; i < p_pixel_count; i++) {
		const uint32_t p = src[i];
		// Little endian: bytes B,G,R,A -> R,G,B,A.
		dst[i] = (p & 0xFF00FF00u) | ((p & 0x000000FFu) << 16) | ((p >> 16) & 0x000000FFu);
	}
}

void GodotCefRenderTarget::_replace_texture(RID p_new_texture, const Size2i &p_size, TextureKind p_kind) {
	ERR_FAIL_COND(p_new_texture.is_null());
	RenderingServer *rs = RenderingServer::get_singleton();
	if (texture.is_valid()) {
		rs->texture_replace(texture, p_new_texture);
	} else {
		texture = p_new_texture;
	}
	texture_size = p_size;
	texture_kind = p_kind;
}

bool GodotCefRenderTarget::enable_accelerated() {
	if (backend) {
		return !accelerated_failed;
	}
	if (accelerated_failed) {
		return false;
	}
	backend = GodotCefAcceleratedBackend::create();
	if (!backend) {
		accelerated_failed = true;
		return false;
	}
	print_verbose(vformat("Godot CEF: accelerated OSR enabled (%s).", backend->get_name()));
	return true;
}

void GodotCefRenderTarget::_upload_software_frame() {
	if (view_size.x <= 0 || view_size.y <= 0 || view_pixels.is_empty()) {
		return;
	}

	PackedByteArray frame = view_pixels;
	if (popup_visible && !popup_pixels.is_empty()) {
		// Composite the <select> popup over the view.
		const Rect2i target = Rect2i(popup_rect.position, popup_size).intersection(Rect2i(Point2i(), view_size));
		if (target.has_area()) {
			uint8_t *dst = frame.ptrw();
			const uint8_t *src = popup_pixels.ptr();
			const int src_x = target.position.x - popup_rect.position.x;
			const int src_y = target.position.y - popup_rect.position.y;
			for (int y = 0; y < target.size.y; y++) {
				memcpy(dst + (int64_t(target.position.y + y) * view_size.x + target.position.x) * 4,
						src + (int64_t(src_y + y) * popup_size.x + src_x) * 4,
						size_t(target.size.x) * 4);
			}
		}
	}

	Ref<Image> image = Image::create_from_data(view_size.x, view_size.y, false, Image::FORMAT_RGBA8, frame);
	RenderingServer *rs = RenderingServer::get_singleton();
	if (texture_kind == TEXTURE_KIND_SOFTWARE && texture_size == view_size && texture.is_valid()) {
		rs->texture_2d_update(texture, image);
	} else {
		_replace_texture(rs->texture_2d_create(image), view_size, TEXTURE_KIND_SOFTWARE);
		_free_rd_textures();
	}
}

bool GodotCefRenderTarget::on_paint(bool p_popup, const void *p_buffer, int p_width, int p_height) {
	if (!p_buffer || p_width <= 0 || p_height <= 0) {
		return false;
	}
	const Size2i old_size = texture_size;
	const int64_t pixel_count = int64_t(p_width) * p_height;

	if (p_popup) {
		popup_size = Size2i(p_width, p_height);
		popup_pixels.resize(pixel_count * 4);
		godot_cef_bgra_to_rgba(static_cast<const uint8_t *>(p_buffer), popup_pixels.ptrw(), pixel_count);
	} else {
		view_size = Size2i(p_width, p_height);
		view_pixels.resize(pixel_count * 4);
		godot_cef_bgra_to_rgba(static_cast<const uint8_t *>(p_buffer), view_pixels.ptrw(), pixel_count);
	}
	_upload_software_frame();
	return texture_size != old_size;
}

RID GodotCefRenderTarget::_create_rd_texture(const Size2i &p_size) const {
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	ERR_FAIL_NULL_V(rd, RID());

	RenderingDevice::TextureFormat tf;
	tf.format = RenderingDevice::DATA_FORMAT_B8G8R8A8_SRGB;
	tf.width = uint32_t(MAX(1, p_size.x));
	tf.height = uint32_t(MAX(1, p_size.y));
	tf.usage_bits = RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_TO_BIT | RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	// Declared up front so RenderingServer never has to recreate the texture (and its native handle).
	tf.shareable_formats.push_back(RenderingDevice::DATA_FORMAT_B8G8R8A8_UNORM);
	tf.shareable_formats.push_back(RenderingDevice::DATA_FORMAT_B8G8R8A8_SRGB);
	return rd->texture_create(tf, RenderingDevice::TextureView());
}

void GodotCefRenderTarget::_free_rd_textures() {
	RenderingServer *rs = RenderingServer::get_singleton();
	RenderingDevice *rd = rs ? rs->get_rendering_device() : nullptr;
	if (rd) {
		if (view_rd.is_valid()) {
			rd->free_rid(view_rd);
		}
		if (popup_rd.is_valid()) {
			rd->free_rid(popup_rd);
		}
	}
	view_rd = RID();
	popup_rd = RID();
	popup_rd_size = Size2i();
	popup_has_content = false;
}

void GodotCefRenderTarget::_composite_popup_accelerated() {
	if (!popup_visible || !popup_has_content || view_rd.is_null() || popup_rd.is_null()) {
		return;
	}
	const Rect2i target = Rect2i(popup_rect.position, popup_rd_size).intersection(Rect2i(Point2i(), texture_size));
	if (!target.has_area()) {
		return;
	}
	const Vector3 from(target.position.x - popup_rect.position.x, target.position.y - popup_rect.position.y, 0);
	const Vector3 to(target.position.x, target.position.y, 0);
	RenderingServer::get_singleton()->get_rendering_device()->texture_copy(popup_rd, view_rd, from, to, Vector3(target.size.x, target.size.y, 1), 0, 0, 0, 0);
}

bool GodotCefRenderTarget::on_accelerated_paint(bool p_popup, const CefAcceleratedPaintInfo &p_info) {
	if (!is_accelerated()) {
		return false;
	}
	const Size2i size = backend->get_shared_texture_size(p_info);
	if (size.x <= 0 || size.y <= 0) {
		return false;
	}
	const Size2i old_size = texture_size;
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();

	if (p_popup) {
		if (popup_rd.is_null() || popup_rd_size != size) {
			if (popup_rd.is_valid()) {
				rd->free_rid(popup_rd);
			}
			popup_rd = _create_rd_texture(size);
			popup_rd_size = size;
		}
		if (popup_rd.is_null() || !backend->copy_shared_texture(p_info, popup_rd, size)) {
			return false;
		}
		popup_has_content = true;
		_composite_popup_accelerated();
		// Native copies bypass RenderingServer, so low-processor mode would not redraw.
		RenderingServerDefault::redraw_request();
		return false;
	}

	if (view_rd.is_null() || texture_kind != TEXTURE_KIND_ACCELERATED || texture_size != size) {
		const RID new_rd = _create_rd_texture(size);
		if (new_rd.is_null()) {
			ERR_PRINT("Godot CEF: failed to create the accelerated OSR texture, falling back to software rendering.");
			accelerated_failed = true;
			return false;
		}
		const RID new_texture = RenderingServer::get_singleton()->texture_rd_create(new_rd);
		if (new_texture.is_null()) {
			rd->free_rid(new_rd);
			accelerated_failed = true;
			return false;
		}
		// The replaced texture releases its views before the old RD texture is freed.
		_replace_texture(new_texture, size, TEXTURE_KIND_ACCELERATED);
		if (view_rd.is_valid()) {
			rd->free_rid(view_rd);
		}
		view_rd = new_rd;
	}

	if (!backend->copy_shared_texture(p_info, view_rd, size)) {
		return texture_size != old_size;
	}
	_composite_popup_accelerated();
	RenderingServerDefault::redraw_request();
	return texture_size != old_size;
}

void GodotCefRenderTarget::set_popup_visible(bool p_visible) {
	popup_visible = p_visible;
	if (!p_visible) {
		popup_pixels.clear();
		popup_size = Size2i();
		popup_has_content = false;
		if (texture_kind == TEXTURE_KIND_SOFTWARE) {
			_upload_software_frame();
		}
	}
}

void GodotCefRenderTarget::set_popup_rect(const Rect2i &p_rect) {
	popup_rect = p_rect;
}

void GodotCefRenderTarget::clear() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs && texture.is_valid() && texture_kind == TEXTURE_KIND_ACCELERATED) {
		// Detach the RD texture from the stable RID before freeing it.
		rs->texture_replace(texture, rs->texture_2d_placeholder_create());
		texture_kind = TEXTURE_KIND_PLACEHOLDER;
		texture_size = Size2i();
	}
	_free_rd_textures();
	view_pixels.clear();
	popup_pixels.clear();
	view_size = Size2i();
	popup_size = Size2i();
	if (backend) {
		memdelete(backend);
		backend = nullptr;
	}
	accelerated_failed = false;
}

GodotCefRenderTarget::~GodotCefRenderTarget() {
	clear();
}
