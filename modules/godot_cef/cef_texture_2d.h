/**************************************************************************/
/*  cef_texture_2d.h                                                      */
/**************************************************************************/

#pragma once

#include "core/input/input_event.h"
#include "scene/resources/texture.h"

class AudioStreamGenerator;
class AudioStreamGeneratorPlayback;
class GodotCefBrowserHost;

// A Texture2D that renders a windowless Chromium browser. Usable directly as a resource
// (e.g. on 3D materials) or through the CefTexture control.
class CefTexture2D : public Texture2D {
	GDCLASS(CefTexture2D, Texture2D);

public:
	enum PopupPolicy {
		POPUP_POLICY_BLOCK = 0,
		POPUP_POLICY_REDIRECT = 1,
		POPUP_POLICY_SIGNAL_ONLY = 2,
	};

private:
	String url = "https://google.com";
	bool enable_accelerated_osr = true;
	Color background_color = Color(0, 0, 0, 0);
	int popup_policy = POPUP_POLICY_BLOCK;
	String preload_script;
	String preload_script_path;
	Vector2i texture_size = Vector2i(1024, 1024);
	float device_scale_factor = 1.0f;
	double zoom_level = 0.0;
	bool audio_muted = false;
	String last_find_query;
	bool last_find_match_case = false;

	mutable RID texture;
	GodotCefBrowserHost *host = nullptr;
	ObjectID signal_forward_target;

	// Browsers of standalone resources start on their own; CefTexture starts its browser
	// once the control has a size.
	bool auto_start = true;
	bool started = false;
	bool shutdown_requested = false;
	bool creation_failed = false;
	bool accelerated_disabled = false;
	bool needs_recreate = false;
	bool pending_changed = false;

	void _ensure_texture() const;
	void _create_browser();
	void _destroy_browser();
	String _get_effective_preload_script() const;
	bool _should_run() const;
	void _apply_view();
	void _emit_deferred_signal(const StringName &p_name, const Array &p_args);

protected:
	static void _bind_methods();

public:
	static void _bind_browser_signals(const StringName &p_class);

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
	void set_texture_size(const Vector2i &p_size);
	Vector2i get_texture_size() const;
	void set_device_scale_factor(float p_scale);
	float get_device_scale_factor() const;
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
	void set_focused(bool p_focused);
	void shutdown();

	bool grant_permission(int64_t p_request_id);
	bool deny_permission(int64_t p_request_id);
	bool respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input);

	bool get_all_cookies();
	bool get_cookies(const String &p_url, bool p_include_http_only);
	bool set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly);
	bool delete_cookies(const String &p_url, const String &p_cookie_name);
	bool clear_cookies();
	bool flush_cookies();

	Ref<AudioStreamGenerator> create_audio_stream();
	int push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback);
	bool has_audio_data() const;
	int get_audio_buffer_size() const;
	bool is_audio_capture_enabled() const;

	// Positions are in texture pixels.
	void drag_enter(const Array &p_file_paths, const Vector2 &p_position, int p_allowed_ops);
	void drag_over(const Vector2 &p_position, int p_allowed_ops);
	void drag_leave();
	void drag_drop(const Vector2 &p_position);
	void drag_source_ended(const Vector2 &p_position, int p_operation);
	void drag_source_system_ended();
	bool is_dragging_from_browser() const;
	bool is_drag_over() const;

	void forward_mouse_button_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_mouse_motion_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_mouse_exit(const Vector2 &p_position, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_pan_gesture_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_magnify_gesture_event(const Ref<InputEvent> &p_event);
	void forward_key_event(const Ref<InputEvent> &p_event, bool p_focus_on_editable_field);
	void forward_screen_touch_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_screen_drag_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_input_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor, bool p_focus_on_editable_field);

	// Internal API used by CefTexture and the runtime.
	void _process_frame();
	void _set_auto_start(bool p_auto_start) { auto_start = p_auto_start; }
	void _start();
	void _set_view(const Vector2i &p_physical_size, float p_device_scale);
	void _set_signal_forward_target(ObjectID p_target) { signal_forward_target = p_target; }
	GodotCefBrowserHost *_get_host() const { return host; }
	void _emit_browser_signal(const StringName &p_name, const Vector<Variant> &p_args);
	void _queue_browser_signal(const StringName &p_name, const Vector<Variant> &p_args);
	bool _has_browser_signal_connections(const StringName &p_name) const;
	void _on_render_target_resized() { pending_changed = true; }
	void _on_accelerated_osr_failed() { needs_recreate = true; }
	void _shutdown_for_exit();

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;
	virtual bool has_alpha() const override;

	CefTexture2D();
	~CefTexture2D();
};

VARIANT_ENUM_CAST(CefTexture2D::PopupPolicy);
