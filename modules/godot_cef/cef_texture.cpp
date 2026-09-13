/**************************************************************************/
/*  cef_texture.cpp                                                       */
/**************************************************************************/

#include "cef_texture.h"

#include "godot_cef_data.h"
#include "godot_cef_settings.h"

#include "core/object/class_db.h"

void CefTexture::set_url(const String &p_url) {
	browser_texture->set_url(p_url);
}

String CefTexture::get_url() const {
	return browser_texture->get_url();
}

void CefTexture::set_enable_accelerated_osr(bool p_enable) {
	browser_texture->set_enable_accelerated_osr(p_enable);
}

bool CefTexture::get_enable_accelerated_osr() const {
	return browser_texture->get_enable_accelerated_osr();
}

void CefTexture::set_background_color(const Color &p_color) {
	browser_texture->set_background_color(p_color);
}

Color CefTexture::get_background_color() const {
	return browser_texture->get_background_color();
}

void CefTexture::set_popup_policy(int p_policy) {
	browser_texture->set_popup_policy(p_policy);
}

int CefTexture::get_popup_policy() const {
	return browser_texture->get_popup_policy();
}

void CefTexture::set_preload_script(const String &p_script) {
	browser_texture->set_preload_script(p_script);
}

String CefTexture::get_preload_script() const {
	return browser_texture->get_preload_script();
}

void CefTexture::set_preload_script_path(const String &p_path) {
	browser_texture->set_preload_script_path(p_path);
}

String CefTexture::get_preload_script_path() const {
	return browser_texture->get_preload_script_path();
}

void CefTexture::set_ime_position(const Vector2i &p_position) {
	ime_position = p_position;
}

Vector2i CefTexture::get_ime_position() const {
	return ime_position;
}

void CefTexture::eval(const String &p_code) {
	browser_texture->eval(p_code);
}

void CefTexture::go_back() {
	browser_texture->go_back();
}

void CefTexture::go_forward() {
	browser_texture->go_forward();
}

bool CefTexture::can_go_back() const {
	return browser_texture->can_go_back();
}

bool CefTexture::can_go_forward() const {
	return browser_texture->can_go_forward();
}

void CefTexture::reload() {
	browser_texture->reload();
}

void CefTexture::reload_ignore_cache() {
	browser_texture->reload_ignore_cache();
}

void CefTexture::stop_loading() {
	browser_texture->stop_loading();
}

bool CefTexture::is_loading() const {
	return browser_texture->is_loading();
}

void CefTexture::set_zoom_level(double p_zoom_level) {
	browser_texture->set_zoom_level(p_zoom_level);
}

double CefTexture::get_zoom_level() const {
	return browser_texture->get_zoom_level();
}

void CefTexture::set_audio_muted(bool p_muted) {
	browser_texture->set_audio_muted(p_muted);
}

bool CefTexture::is_audio_muted() const {
	return browser_texture->is_audio_muted();
}

void CefTexture::send_ipc_message(const String &p_message) {
	browser_texture->send_ipc_message(p_message);
}

void CefTexture::send_ipc_binary_message(const PackedByteArray &p_data) {
	browser_texture->send_ipc_binary_message(p_data);
}

void CefTexture::send_ipc_data(const Variant &p_data) {
	browser_texture->send_ipc_data(p_data);
}

void CefTexture::find_text(const String &p_query, bool p_forward, bool p_match_case) {
	browser_texture->find_text(p_query, p_forward, p_match_case);
}

void CefTexture::find_next() {
	browser_texture->find_next();
}

void CefTexture::find_previous() {
	browser_texture->find_previous();
}

void CefTexture::stop_finding() {
	browser_texture->stop_finding();
}

Ref<AudioStreamGenerator> CefTexture::create_audio_stream() const {
	Ref<AudioStreamGenerator> stream;
	stream.instantiate();
	stream->set_mix_rate(48000.0);
	stream->set_buffer_length(0.1);
	return stream;
}

int CefTexture::push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback) {
	(void)p_playback;
	return 0;
}

bool CefTexture::has_audio_data() const {
	return false;
}

int CefTexture::get_audio_buffer_size() const {
	return 0;
}

bool CefTexture::is_audio_capture_enabled() const {
	return GodotCefSettings::is_audio_capture_enabled();
}

void CefTexture::drag_enter(const Array &p_file_paths, const Vector2 &p_position, int p_allowed_ops) {
	(void)p_file_paths;
	(void)p_position;
	(void)p_allowed_ops;
	drag_over_browser = true;
}

void CefTexture::drag_over(const Vector2 &p_position, int p_allowed_ops) {
	(void)p_position;
	(void)p_allowed_ops;
}

void CefTexture::drag_leave() {
	drag_over_browser = false;
}

void CefTexture::drag_drop(const Vector2 &p_position) {
	(void)p_position;
	drag_over_browser = false;
}

void CefTexture::drag_source_ended(const Vector2 &p_position, int p_operation) {
	(void)p_position;
	(void)p_operation;
	dragging_from_browser = false;
}

void CefTexture::drag_source_system_ended() {
	dragging_from_browser = false;
}

bool CefTexture::is_dragging_from_browser() const {
	return dragging_from_browser;
}

bool CefTexture::is_drag_over() const {
	return drag_over_browser;
}

bool CefTexture::grant_permission(int64_t p_request_id) const {
	(void)p_request_id;
	return false;
}

bool CefTexture::deny_permission(int64_t p_request_id) const {
	(void)p_request_id;
	return false;
}

bool CefTexture::get_all_cookies() const {
	return false;
}

bool CefTexture::get_cookies(const String &p_url, bool p_include_http_only) const {
	(void)p_url;
	(void)p_include_http_only;
	return false;
}

bool CefTexture::set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly) const {
	(void)p_url;
	(void)p_name;
	(void)p_value;
	(void)p_domain;
	(void)p_path;
	(void)p_secure;
	(void)p_httponly;
	return false;
}

bool CefTexture::delete_cookies(const String &p_url, const String &p_cookie_name) const {
	(void)p_url;
	(void)p_cookie_name;
	return false;
}

bool CefTexture::clear_cookies() const {
	return delete_cookies(String(), String());
}

bool CefTexture::flush_cookies() const {
	return false;
}

void CefTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CefTexture::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CefTexture::get_url);
	ClassDB::bind_method(D_METHOD("set_enable_accelerated_osr", "enable"), &CefTexture::set_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("get_enable_accelerated_osr"), &CefTexture::get_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("set_background_color", "color"), &CefTexture::set_background_color);
	ClassDB::bind_method(D_METHOD("get_background_color"), &CefTexture::get_background_color);
	ClassDB::bind_method(D_METHOD("set_popup_policy", "policy"), &CefTexture::set_popup_policy);
	ClassDB::bind_method(D_METHOD("get_popup_policy"), &CefTexture::get_popup_policy);
	ClassDB::bind_method(D_METHOD("set_preload_script", "script"), &CefTexture::set_preload_script);
	ClassDB::bind_method(D_METHOD("get_preload_script"), &CefTexture::get_preload_script);
	ClassDB::bind_method(D_METHOD("set_preload_script_path", "path"), &CefTexture::set_preload_script_path);
	ClassDB::bind_method(D_METHOD("get_preload_script_path"), &CefTexture::get_preload_script_path);
	ClassDB::bind_method(D_METHOD("set_ime_position", "position"), &CefTexture::set_ime_position);
	ClassDB::bind_method(D_METHOD("get_ime_position"), &CefTexture::get_ime_position);
	ClassDB::bind_method(D_METHOD("eval", "code"), &CefTexture::eval);
	ClassDB::bind_method(D_METHOD("go_back"), &CefTexture::go_back);
	ClassDB::bind_method(D_METHOD("go_forward"), &CefTexture::go_forward);
	ClassDB::bind_method(D_METHOD("can_go_back"), &CefTexture::can_go_back);
	ClassDB::bind_method(D_METHOD("can_go_forward"), &CefTexture::can_go_forward);
	ClassDB::bind_method(D_METHOD("reload"), &CefTexture::reload);
	ClassDB::bind_method(D_METHOD("reload_ignore_cache"), &CefTexture::reload_ignore_cache);
	ClassDB::bind_method(D_METHOD("stop_loading"), &CefTexture::stop_loading);
	ClassDB::bind_method(D_METHOD("is_loading"), &CefTexture::is_loading);
	ClassDB::bind_method(D_METHOD("set_zoom_level", "zoom_level"), &CefTexture::set_zoom_level);
	ClassDB::bind_method(D_METHOD("get_zoom_level"), &CefTexture::get_zoom_level);
	ClassDB::bind_method(D_METHOD("set_audio_muted", "muted"), &CefTexture::set_audio_muted);
	ClassDB::bind_method(D_METHOD("is_audio_muted"), &CefTexture::is_audio_muted);
	ClassDB::bind_method(D_METHOD("send_ipc_message", "message"), &CefTexture::send_ipc_message);
	ClassDB::bind_method(D_METHOD("send_ipc_binary_message", "data"), &CefTexture::send_ipc_binary_message);
	ClassDB::bind_method(D_METHOD("send_ipc_data", "data"), &CefTexture::send_ipc_data);
	ClassDB::bind_method(D_METHOD("find_text", "query", "forward", "match_case"), &CefTexture::find_text);
	ClassDB::bind_method(D_METHOD("find_next"), &CefTexture::find_next);
	ClassDB::bind_method(D_METHOD("find_previous"), &CefTexture::find_previous);
	ClassDB::bind_method(D_METHOD("stop_finding"), &CefTexture::stop_finding);
	ClassDB::bind_method(D_METHOD("create_audio_stream"), &CefTexture::create_audio_stream);
	ClassDB::bind_method(D_METHOD("push_audio_to_playback", "playback"), &CefTexture::push_audio_to_playback);
	ClassDB::bind_method(D_METHOD("has_audio_data"), &CefTexture::has_audio_data);
	ClassDB::bind_method(D_METHOD("get_audio_buffer_size"), &CefTexture::get_audio_buffer_size);
	ClassDB::bind_method(D_METHOD("is_audio_capture_enabled"), &CefTexture::is_audio_capture_enabled);
	ClassDB::bind_method(D_METHOD("drag_enter", "file_paths", "position", "allowed_ops"), &CefTexture::drag_enter);
	ClassDB::bind_method(D_METHOD("drag_over", "position", "allowed_ops"), &CefTexture::drag_over);
	ClassDB::bind_method(D_METHOD("drag_leave"), &CefTexture::drag_leave);
	ClassDB::bind_method(D_METHOD("drag_drop", "position"), &CefTexture::drag_drop);
	ClassDB::bind_method(D_METHOD("drag_source_ended", "position", "operation"), &CefTexture::drag_source_ended);
	ClassDB::bind_method(D_METHOD("drag_source_system_ended"), &CefTexture::drag_source_system_ended);
	ClassDB::bind_method(D_METHOD("is_dragging_from_browser"), &CefTexture::is_dragging_from_browser);
	ClassDB::bind_method(D_METHOD("is_drag_over"), &CefTexture::is_drag_over);
	ClassDB::bind_method(D_METHOD("grant_permission", "request_id"), &CefTexture::grant_permission);
	ClassDB::bind_method(D_METHOD("deny_permission", "request_id"), &CefTexture::deny_permission);
	ClassDB::bind_method(D_METHOD("get_all_cookies"), &CefTexture::get_all_cookies);
	ClassDB::bind_method(D_METHOD("get_cookies", "url", "include_http_only"), &CefTexture::get_cookies);
	ClassDB::bind_method(D_METHOD("set_cookie", "url", "name", "value", "domain", "path", "secure", "httponly"), &CefTexture::set_cookie);
	ClassDB::bind_method(D_METHOD("delete_cookies", "url", "cookie_name"), &CefTexture::delete_cookies);
	ClassDB::bind_method(D_METHOD("clear_cookies"), &CefTexture::clear_cookies);
	ClassDB::bind_method(D_METHOD("flush_cookies"), &CefTexture::flush_cookies);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_accelerated_osr"), "set_enable_accelerated_osr", "get_enable_accelerated_osr");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "set_background_color", "get_background_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "popup_policy", PROPERTY_HINT_ENUM, "Block:0,Redirect:1,SignalOnly:2"), "set_popup_policy", "get_popup_policy");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script", PROPERTY_HINT_MULTILINE_TEXT), "set_preload_script", "get_preload_script");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script_path", PROPERTY_HINT_FILE), "set_preload_script_path", "get_preload_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "ime_position"), "set_ime_position", "get_ime_position");

	ADD_SIGNAL(MethodInfo("ipc_message", PropertyInfo(Variant::STRING, "message")));
	ADD_SIGNAL(MethodInfo("ipc_binary_message", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("ipc_data_message", PropertyInfo(Variant::NIL, "data")));
	ADD_SIGNAL(MethodInfo("debug_ipc_message", PropertyInfo(Variant::NIL, "event")));
	ADD_SIGNAL(MethodInfo("url_changed", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("title_changed", PropertyInfo(Variant::STRING, "title")));
	ADD_SIGNAL(MethodInfo("load_started", PropertyInfo(Variant::STRING, "url")));
	ADD_SIGNAL(MethodInfo("load_finished", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "http_status_code")));
	ADD_SIGNAL(MethodInfo("load_error", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "error_code"), PropertyInfo(Variant::STRING, "error_text")));
	ADD_SIGNAL(MethodInfo("console_message", PropertyInfo(Variant::INT, "level"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "source"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("drag_started", PropertyInfo(Variant::OBJECT, "drag_data", PROPERTY_HINT_RESOURCE_TYPE, "DragDataInfo"), PropertyInfo(Variant::VECTOR2, "position"), PropertyInfo(Variant::INT, "allowed_ops")));
	ADD_SIGNAL(MethodInfo("drag_cursor_updated", PropertyInfo(Variant::INT, "operation")));
	ADD_SIGNAL(MethodInfo("drag_entered", PropertyInfo(Variant::OBJECT, "drag_data", PROPERTY_HINT_RESOURCE_TYPE, "DragDataInfo"), PropertyInfo(Variant::INT, "mask")));
	ADD_SIGNAL(MethodInfo("download_requested", PropertyInfo(Variant::OBJECT, "download_info", PROPERTY_HINT_RESOURCE_TYPE, "DownloadRequestInfo")));
	ADD_SIGNAL(MethodInfo("download_updated", PropertyInfo(Variant::OBJECT, "download_info", PROPERTY_HINT_RESOURCE_TYPE, "DownloadUpdateInfo")));
	ADD_SIGNAL(MethodInfo("render_process_terminated", PropertyInfo(Variant::INT, "status"), PropertyInfo(Variant::STRING, "error_message")));
	ADD_SIGNAL(MethodInfo("popup_requested", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "disposition"), PropertyInfo(Variant::BOOL, "user_gesture")));
	ADD_SIGNAL(MethodInfo("permission_requested", PropertyInfo(Variant::STRING, "permission_type"), PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "request_id")));
	ADD_SIGNAL(MethodInfo("find_result", PropertyInfo(Variant::INT, "count"), PropertyInfo(Variant::INT, "active_index"), PropertyInfo(Variant::BOOL, "final_update")));
	ADD_SIGNAL(MethodInfo("cookies_received", PropertyInfo(Variant::ARRAY, "cookies")));
	ADD_SIGNAL(MethodInfo("cookie_set", PropertyInfo(Variant::BOOL, "success")));
	ADD_SIGNAL(MethodInfo("cookies_deleted", PropertyInfo(Variant::INT, "num_deleted")));
	ADD_SIGNAL(MethodInfo("cookies_flushed"));
}

CefTexture::CefTexture() {
	browser_texture.instantiate();
	set_texture(browser_texture);
}
