/**************************************************************************/
/*  cef_texture_2d.h                                                      */
/**************************************************************************/

#pragma once

#include "core/input/input_event.h"
#include "scene/resources/texture.h"

class CefTexture2D : public Texture2D {
	GDCLASS(CefTexture2D, Texture2D);

	String url = "https://google.com";
	bool enable_accelerated_osr = true;
	Color background_color = Color(0, 0, 0, 0);
	int popup_policy = 0;
	String preload_script;
	String preload_script_path;
	Vector2i texture_size = Vector2i(1024, 1024);
	double zoom_level = 0.0;
	bool audio_muted = false;
	String last_find_query;
	bool last_find_match_case = false;

protected:
	static void _bind_methods();

public:
	enum PopupPolicy {
		POPUP_POLICY_BLOCK = 0,
		POPUP_POLICY_REDIRECT = 1,
		POPUP_POLICY_SIGNAL_ONLY = 2,
	};

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
	void set_texture_size(const Vector2i &p_size);
	Vector2i get_texture_size() const;

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

	void forward_mouse_button_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_mouse_motion_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_pan_gesture_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_magnify_gesture_event(const Ref<InputEvent> &p_event);
	void forward_key_event(const Ref<InputEvent> &p_event, bool p_focus_on_editable_field);
	void forward_screen_touch_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_screen_drag_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor);
	void forward_input_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor, bool p_focus_on_editable_field);

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;
	virtual bool has_alpha() const override;
};

VARIANT_ENUM_CAST(CefTexture2D::PopupPolicy);
