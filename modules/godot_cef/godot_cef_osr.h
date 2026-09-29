/**************************************************************************/
/*  godot_cef_osr.h                                                       */
/**************************************************************************/

#pragma once

#include "godot_cef_include.h"

#include "core/math/rect2i.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"

// Copies CEF shared textures (IOSurface / D3D11 shared handle) into RenderingDevice textures.
// CefAcceleratedPaintInfo is only valid during OnAcceleratedPaint, so copies are synchronous.
class GodotCefAcceleratedBackend {
public:
	virtual ~GodotCefAcceleratedBackend() = default;

	virtual const char *get_name() const = 0;
	virtual Size2i get_shared_texture_size(const CefAcceleratedPaintInfo &p_info) = 0;
	// Copies the top-left p_size region of the shared texture into p_dst.
	virtual bool copy_shared_texture(const CefAcceleratedPaintInfo &p_info, RID p_dst, const Size2i &p_size) = 0;

	// Returns a backend for the active rendering driver, or nullptr when accelerated OSR is unavailable.
	static GodotCefAcceleratedBackend *create();
	static bool is_supported();
	// PCI ids of the GPU Godot renders with, so CEF's GPU process can use the same adapter.
	static bool get_gpu_ids(uint32_t &r_vendor_id, uint32_t &r_device_id);
};

class GodotCefRenderTarget {
	RID texture; // Stable RenderingServer texture owned by CefTexture2D.
	Size2i texture_size;

	enum TextureKind {
		TEXTURE_KIND_PLACEHOLDER,
		TEXTURE_KIND_SOFTWARE,
		TEXTURE_KIND_ACCELERATED,
	};
	TextureKind texture_kind = TEXTURE_KIND_PLACEHOLDER;

	bool popup_visible = false;
	Rect2i popup_rect;

	// Software path.
	PackedByteArray view_pixels; // RGBA8.
	Size2i view_size;
	PackedByteArray popup_pixels; // RGBA8.
	Size2i popup_size;

	// Accelerated path.
	GodotCefAcceleratedBackend *backend = nullptr;
	RID view_rd;
	RID popup_rd;
	Size2i popup_rd_size;
	bool popup_has_content = false;
	bool accelerated_failed = false;

	void _replace_texture(RID p_new_texture, const Size2i &p_size, TextureKind p_kind);
	void _upload_software_frame();
	RID _create_rd_texture(const Size2i &p_size) const;
	void _free_rd_textures();
	void _composite_popup_accelerated();

public:
	void set_texture(RID p_texture) { texture = p_texture; }
	RID get_texture() const { return texture; }
	Size2i get_size() const { return texture_size; }

	// Returns true when the accelerated path is ready.
	bool enable_accelerated();
	bool is_accelerated() const { return backend != nullptr && !accelerated_failed; }
	bool has_accelerated_failed() const { return accelerated_failed; }

	// Return true when the texture was recreated with a new size.
	bool on_paint(bool p_popup, const void *p_buffer, int p_width, int p_height);
	bool on_accelerated_paint(bool p_popup, const CefAcceleratedPaintInfo &p_info);

	void set_popup_visible(bool p_visible);
	void set_popup_rect(const Rect2i &p_rect);
	bool is_popup_visible() const { return popup_visible; }

	void clear();
	~GodotCefRenderTarget();
};

void godot_cef_bgra_to_rgba(const uint8_t *p_src, uint8_t *p_dst, int64_t p_pixel_count);
