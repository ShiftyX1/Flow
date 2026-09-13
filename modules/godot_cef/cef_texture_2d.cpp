/**************************************************************************/
/*  cef_texture_2d.cpp                                                    */
/**************************************************************************/

#include "cef_texture_2d.h"

#include "godot_cef_runtime.h"

#include "core/object/class_db.h"

void CefTexture2D::set_url(const String &p_url) {
	url = p_url;
	GodotCefRuntime::ensure_browser_runtime();
	emit_changed();
}

String CefTexture2D::get_url() const {
	return url;
}

void CefTexture2D::set_enable_accelerated_osr(bool p_enable) {
	enable_accelerated_osr = p_enable;
}

bool CefTexture2D::get_enable_accelerated_osr() const {
	return enable_accelerated_osr;
}

void CefTexture2D::set_background_color(const Color &p_color) {
	background_color = p_color;
}

Color CefTexture2D::get_background_color() const {
	return background_color;
}

void CefTexture2D::set_popup_policy(int p_policy) {
	popup_policy = CLAMP(p_policy, POPUP_POLICY_BLOCK, POPUP_POLICY_SIGNAL_ONLY);
}

int CefTexture2D::get_popup_policy() const {
	return popup_policy;
}

void CefTexture2D::set_preload_script(const String &p_script) {
	preload_script = p_script;
}

String CefTexture2D::get_preload_script() const {
	return preload_script;
}

void CefTexture2D::set_preload_script_path(const String &p_path) {
	preload_script_path = p_path;
}

String CefTexture2D::get_preload_script_path() const {
	return preload_script_path;
}

void CefTexture2D::set_texture_size(const Vector2i &p_size) {
	texture_size = Vector2i(MAX(1, p_size.x), MAX(1, p_size.y));
	emit_changed();
}

Vector2i CefTexture2D::get_texture_size() const {
	return texture_size;
}

void CefTexture2D::eval(const String &p_code) {
	(void)p_code;
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::go_back() {
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::go_forward() {
	GodotCefRuntime::ensure_browser_runtime();
}

bool CefTexture2D::can_go_back() const {
	return false;
}

bool CefTexture2D::can_go_forward() const {
	return false;
}

void CefTexture2D::reload() {
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::reload_ignore_cache() {
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::stop_loading() {
	GodotCefRuntime::ensure_browser_runtime();
}

bool CefTexture2D::is_loading() const {
	return false;
}

void CefTexture2D::set_zoom_level(double p_zoom_level) {
	zoom_level = p_zoom_level;
	GodotCefRuntime::ensure_browser_runtime();
}

double CefTexture2D::get_zoom_level() const {
	return zoom_level;
}

void CefTexture2D::set_audio_muted(bool p_muted) {
	audio_muted = p_muted;
	GodotCefRuntime::ensure_browser_runtime();
}

bool CefTexture2D::is_audio_muted() const {
	return audio_muted;
}

void CefTexture2D::send_ipc_message(const String &p_message) {
	(void)p_message;
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::send_ipc_binary_message(const PackedByteArray &p_data) {
	(void)p_data;
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::send_ipc_data(const Variant &p_data) {
	(void)p_data;
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::find_text(const String &p_query, bool p_forward, bool p_match_case) {
	(void)p_forward;
	last_find_query = p_query;
	last_find_match_case = p_match_case;
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::find_next() {
	find_text(last_find_query, true, last_find_match_case);
}

void CefTexture2D::find_previous() {
	find_text(last_find_query, false, last_find_match_case);
}

void CefTexture2D::stop_finding() {
	last_find_query.clear();
	GodotCefRuntime::ensure_browser_runtime();
}

void CefTexture2D::shutdown() {
	last_find_query.clear();
	last_find_match_case = false;
}

void CefTexture2D::forward_mouse_button_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	forward_input_event(p_event, p_pixel_scale_factor, p_device_scale_factor, false);
}

void CefTexture2D::forward_mouse_motion_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	forward_input_event(p_event, p_pixel_scale_factor, p_device_scale_factor, false);
}

void CefTexture2D::forward_pan_gesture_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	forward_input_event(p_event, p_pixel_scale_factor, p_device_scale_factor, false);
}

void CefTexture2D::forward_magnify_gesture_event(const Ref<InputEvent> &p_event) {
	forward_input_event(p_event, 1.0, 1.0, false);
}

void CefTexture2D::forward_key_event(const Ref<InputEvent> &p_event, bool p_focus_on_editable_field) {
	forward_input_event(p_event, 1.0, 1.0, p_focus_on_editable_field);
}

void CefTexture2D::forward_screen_touch_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	forward_input_event(p_event, p_pixel_scale_factor, p_device_scale_factor, false);
}

void CefTexture2D::forward_screen_drag_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	forward_input_event(p_event, p_pixel_scale_factor, p_device_scale_factor, false);
}

void CefTexture2D::forward_input_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor, bool p_focus_on_editable_field) {
	(void)p_event;
	(void)p_pixel_scale_factor;
	(void)p_device_scale_factor;
	(void)p_focus_on_editable_field;
	GodotCefRuntime::ensure_browser_runtime();
}

int CefTexture2D::get_width() const {
	return texture_size.x;
}

int CefTexture2D::get_height() const {
	return texture_size.y;
}

RID CefTexture2D::get_rid() const {
	return RID();
}

bool CefTexture2D::has_alpha() const {
	return true;
}

void CefTexture2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CefTexture2D::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CefTexture2D::get_url);
	ClassDB::bind_method(D_METHOD("set_enable_accelerated_osr", "enable"), &CefTexture2D::set_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("get_enable_accelerated_osr"), &CefTexture2D::get_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("set_background_color", "color"), &CefTexture2D::set_background_color);
	ClassDB::bind_method(D_METHOD("get_background_color"), &CefTexture2D::get_background_color);
	ClassDB::bind_method(D_METHOD("set_popup_policy", "policy"), &CefTexture2D::set_popup_policy);
	ClassDB::bind_method(D_METHOD("get_popup_policy"), &CefTexture2D::get_popup_policy);
	ClassDB::bind_method(D_METHOD("set_preload_script", "script"), &CefTexture2D::set_preload_script);
	ClassDB::bind_method(D_METHOD("get_preload_script"), &CefTexture2D::get_preload_script);
	ClassDB::bind_method(D_METHOD("set_preload_script_path", "path"), &CefTexture2D::set_preload_script_path);
	ClassDB::bind_method(D_METHOD("get_preload_script_path"), &CefTexture2D::get_preload_script_path);
	ClassDB::bind_method(D_METHOD("set_texture_size", "size"), &CefTexture2D::set_texture_size);
	ClassDB::bind_method(D_METHOD("get_texture_size"), &CefTexture2D::get_texture_size);
	ClassDB::bind_method(D_METHOD("eval", "code"), &CefTexture2D::eval);
	ClassDB::bind_method(D_METHOD("go_back"), &CefTexture2D::go_back);
	ClassDB::bind_method(D_METHOD("go_forward"), &CefTexture2D::go_forward);
	ClassDB::bind_method(D_METHOD("can_go_back"), &CefTexture2D::can_go_back);
	ClassDB::bind_method(D_METHOD("can_go_forward"), &CefTexture2D::can_go_forward);
	ClassDB::bind_method(D_METHOD("reload"), &CefTexture2D::reload);
	ClassDB::bind_method(D_METHOD("reload_ignore_cache"), &CefTexture2D::reload_ignore_cache);
	ClassDB::bind_method(D_METHOD("stop_loading"), &CefTexture2D::stop_loading);
	ClassDB::bind_method(D_METHOD("is_loading"), &CefTexture2D::is_loading);
	ClassDB::bind_method(D_METHOD("set_zoom_level", "zoom_level"), &CefTexture2D::set_zoom_level);
	ClassDB::bind_method(D_METHOD("get_zoom_level"), &CefTexture2D::get_zoom_level);
	ClassDB::bind_method(D_METHOD("set_audio_muted", "muted"), &CefTexture2D::set_audio_muted);
	ClassDB::bind_method(D_METHOD("is_audio_muted"), &CefTexture2D::is_audio_muted);
	ClassDB::bind_method(D_METHOD("send_ipc_message", "message"), &CefTexture2D::send_ipc_message);
	ClassDB::bind_method(D_METHOD("send_ipc_binary_message", "data"), &CefTexture2D::send_ipc_binary_message);
	ClassDB::bind_method(D_METHOD("send_ipc_data", "data"), &CefTexture2D::send_ipc_data);
	ClassDB::bind_method(D_METHOD("find_text", "query", "forward", "match_case"), &CefTexture2D::find_text);
	ClassDB::bind_method(D_METHOD("find_next"), &CefTexture2D::find_next);
	ClassDB::bind_method(D_METHOD("find_previous"), &CefTexture2D::find_previous);
	ClassDB::bind_method(D_METHOD("stop_finding"), &CefTexture2D::stop_finding);
	ClassDB::bind_method(D_METHOD("shutdown"), &CefTexture2D::shutdown);
	ClassDB::bind_method(D_METHOD("forward_mouse_button_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_mouse_button_event);
	ClassDB::bind_method(D_METHOD("forward_mouse_motion_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_mouse_motion_event);
	ClassDB::bind_method(D_METHOD("forward_pan_gesture_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_pan_gesture_event);
	ClassDB::bind_method(D_METHOD("forward_magnify_gesture_event", "event"), &CefTexture2D::forward_magnify_gesture_event);
	ClassDB::bind_method(D_METHOD("forward_key_event", "event", "focus_on_editable_field"), &CefTexture2D::forward_key_event);
	ClassDB::bind_method(D_METHOD("forward_screen_touch_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_screen_touch_event);
	ClassDB::bind_method(D_METHOD("forward_screen_drag_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_screen_drag_event);
	ClassDB::bind_method(D_METHOD("forward_input_event", "event", "pixel_scale_factor", "device_scale_factor", "focus_on_editable_field"), &CefTexture2D::forward_input_event);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_accelerated_osr"), "set_enable_accelerated_osr", "get_enable_accelerated_osr");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "set_background_color", "get_background_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "popup_policy", PROPERTY_HINT_ENUM, "Block:0,Redirect:1,SignalOnly:2"), "set_popup_policy", "get_popup_policy");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script", PROPERTY_HINT_MULTILINE_TEXT), "set_preload_script", "get_preload_script");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script_path", PROPERTY_HINT_FILE), "set_preload_script_path", "get_preload_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "texture_size"), "set_texture_size", "get_texture_size");

	BIND_ENUM_CONSTANT(POPUP_POLICY_BLOCK);
	BIND_ENUM_CONSTANT(POPUP_POLICY_REDIRECT);
	BIND_ENUM_CONSTANT(POPUP_POLICY_SIGNAL_ONLY);
}
