/**************************************************************************/
/*  cef_texture.h                                                         */
/**************************************************************************/

#pragma once

#include "cef_texture_2d.h"
#include "core/variant/array.h"
#include "scene/gui/texture_rect.h"
#include "servers/audio/effects/audio_stream_generator.h"

class CefTexture : public TextureRect {
	GDCLASS(CefTexture, TextureRect);

	Ref<CefTexture2D> browser_texture;
	Vector2i ime_position;
	bool dragging_from_browser = false;
	bool drag_over_browser = false;

protected:
	static void _bind_methods();

public:
	void set_url(const String &p_url);
	String get_url() const;
	void set_enable_accelerated_osr(bool p_enable);
	bool get_enable_accelerated_osr() const;
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
	Ref<AudioStreamGenerator> create_audio_stream() const;
	int push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback);
	bool has_audio_data() const;
	int get_audio_buffer_size() const;
	bool is_audio_capture_enabled() const;
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
	bool get_all_cookies() const;
	bool get_cookies(const String &p_url, bool p_include_http_only) const;
	bool set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly) const;
	bool delete_cookies(const String &p_url, const String &p_cookie_name) const;
	bool clear_cookies() const;
	bool flush_cookies() const;

	CefTexture();
};
