/**************************************************************************/
/*  cef_texture.h                                                         */
/**************************************************************************/

#pragma once

#include "cef_texture_2d.h"

#include "core/variant/array.h"
#include "scene/gui/texture_rect.h"
#include "servers/audio/effects/audio_stream_generator.h"

// A Control that displays a browser and forwards input, focus, cursor and IME to it.
class CefTexture : public TextureRect {
	GDCLASS(CefTexture, TextureRect);

	Ref<CefTexture2D> browser_texture;
	Vector2i ime_position;
	bool ime_position_overridden = false;
	bool ime_active = false;
	bool ime_composing = false;
	String last_tooltip;
	float pixel_scale = 1.0f;
	float device_scale = 1.0f;

	GodotCefBrowserHost *_get_host() const;
	void _update_scale();
	void _update_view();
	void _update_cursor_and_tooltip();
	void _update_ime();
	void _set_ime_active(bool p_active);
	void _apply_premultiplied_material();
	void _on_files_dropped(const PackedStringArray &p_files);
	void _finish_browser_drag(const Vector2 &p_local_position);
	Vector2 _local_to_texture(const Vector2 &p_local) const;

protected:
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual String get_tooltip(const Point2 &p_pos) const override;
	virtual CursorShape get_cursor_shape(const Point2 &p_pos = Point2()) const override;

	Ref<CefTexture2D> get_browser_texture() const { return browser_texture; }

	void set_url(const String &p_url);
	String get_url() const;
	String get_current_url() const;
	String get_title() const;
	void set_enable_accelerated_osr(bool p_enable);
	bool get_enable_accelerated_osr() const;
	bool is_accelerated() const;
	void set_background_color(const Color &p_color);
	Color get_background_color() const;
	void set_popup_policy(int p_policy);
	int get_popup_policy() const;
	void set_preload_script(const String &p_script);
	String get_preload_script() const;
	void set_preload_script_path(const String &p_path);
	String get_preload_script_path() const;
	void set_ime_position(const Vector2i &p_position);
	Vector2i get_ime_position() const;
	bool is_browser_ready() const;

	void eval(const String &p_code);
	void go_back();
	void go_forward();
	bool can_go_back() const;
	bool can_go_forward() const;
	void reload();
	void reload_ignore_cache();
	void stop_loading();
	bool is_loading() const;
	void set_zoom_level(double p_zoom_level);
	double get_zoom_level() const;
	void set_audio_muted(bool p_muted);
	bool is_audio_muted() const;
	void send_ipc_message(const String &p_message);
	void send_ipc_binary_message(const PackedByteArray &p_data);
	void send_ipc_data(const Variant &p_data);
	void find_text(const String &p_query, bool p_forward, bool p_match_case);
	void find_next();
	void find_previous();
	void stop_finding();
	void shutdown();
	Ref<AudioStreamGenerator> create_audio_stream() const;
	int push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback);
	bool has_audio_data() const;
	int get_audio_buffer_size() const;
	bool is_audio_capture_enabled() const;
	// Positions are local to the control.
	void drag_enter(const Array &p_file_paths, const Vector2 &p_position, int p_allowed_ops);
	void drag_over(const Vector2 &p_position, int p_allowed_ops);
	void drag_leave();
	void drag_drop(const Vector2 &p_position);
	void drag_source_ended(const Vector2 &p_position, int p_operation);
	void drag_source_system_ended();
	bool is_dragging_from_browser() const;
	bool is_drag_over() const;
	bool grant_permission(int64_t p_request_id) const;
	bool deny_permission(int64_t p_request_id) const;
	bool respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input) const;
	bool get_all_cookies() const;
	bool get_cookies(const String &p_url, bool p_include_http_only) const;
	bool set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly) const;
	bool delete_cookies(const String &p_url, const String &p_cookie_name) const;
	bool clear_cookies() const;
	bool flush_cookies() const;

	static void cleanup_shared_resources();

	CefTexture();
	~CefTexture();
};
