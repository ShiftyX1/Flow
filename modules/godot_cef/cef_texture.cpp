/**************************************************************************/
/*  cef_texture.cpp                                                       */
/**************************************************************************/

#include "cef_texture.h"

#include "godot_cef_browser.h"
#include "godot_cef_data.h"
#include "godot_cef_input.h"
#include "godot_cef_runtime.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/main_loop.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/canvas_item_material.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

static Ref<CanvasItemMaterial> *shared_premultiplied_material = nullptr;

GodotCefBrowserHost *CefTexture::_get_host() const {
	return browser_texture.is_valid() ? browser_texture->_get_host() : nullptr;
}

void CefTexture::_update_scale() {
	float new_pixel_scale = 1.0f;
	if (is_inside_tree()) {
		const Transform2D xform = get_viewport()->get_stretch_transform() * get_global_transform_with_canvas();
		new_pixel_scale = CLAMP(Math::abs(xform.get_scale().x), 0.1f, 8.0f);
	}
	float new_device_scale = 1.0f;
	DisplayServer *ds = DisplayServer::get_singleton();
	Window *window = is_inside_tree() ? get_window() : nullptr;
	if (ds && window) {
		const int screen = window->get_current_screen();
#ifdef WINDOWS_ENABLED
		new_device_scale = ds->screen_get_dpi(screen) / 96.0f;
#else
		new_device_scale = ds->screen_get_scale(screen);
#endif
		new_device_scale = CLAMP(new_device_scale, 0.5f, 8.0f);
	}
	pixel_scale = new_pixel_scale;
	device_scale = new_device_scale;
}

void CefTexture::_update_view() {
	if (browser_texture.is_null()) {
		return;
	}
	_update_scale();
	const Size2 size = get_size();
	if (size.x < 1.0f || size.y < 1.0f) {
		return;
	}
	const Vector2i physical = Vector2i(MAX(1, int(Math::round(size.x * pixel_scale))), MAX(1, int(Math::round(size.y * pixel_scale))));
	browser_texture->_set_view(physical, device_scale);
	browser_texture->_start();
}

void CefTexture::_update_cursor_and_tooltip() {
	GodotCefBrowserHost *host = _get_host();
	if (!host) {
		return;
	}
	const CursorShape shape = CursorShape(host->get_cursor());
	if (shape != get_default_cursor_shape() && is_visible_in_tree() && get_global_rect().has_point(get_global_mouse_position())) {
		DisplayServer::get_singleton()->cursor_set_shape(DisplayServerEnums::CursorShape(shape));
	}
	last_tooltip = host->get_tooltip();
}

void CefTexture::_set_ime_active(bool p_active) {
	if (ime_active == p_active) {
		return;
	}
	ime_active = p_active;
	Window *window = is_inside_tree() ? get_window() : nullptr;
	DisplayServer *ds = DisplayServer::get_singleton();
	if (!window || !ds || !ds->has_feature(DisplayServerEnums::FEATURE_IME)) {
		return;
	}
	const DisplayServerEnums::WindowID wid = window->get_window_id();
	if (wid == DisplayServerEnums::INVALID_WINDOW_ID) {
		return;
	}
	if (!p_active) {
		if (ime_composing) {
			GodotCefBrowserHost *host = _get_host();
			if (host && host->has_browser()) {
				GodotCefInput::ime_cancel_composition(host->get_cef_host());
			}
			ime_composing = false;
		}
		ds->window_set_ime_position(Point2(), wid);
	}
	ds->window_set_ime_active(p_active, wid);
}

void CefTexture::_update_ime() {
	GodotCefBrowserHost *host = _get_host();
	const bool want = host && host->is_ime_requested() && has_focus() && is_visible_in_tree();
	_set_ime_active(want);
	if (!ime_active) {
		return;
	}
	if (!ime_position_overridden) {
		const Rect2i caret = host->get_ime_caret_rect();
		const float to_local = device_scale / MAX(pixel_scale, 0.01f);
		ime_position = Vector2i(Math::round(caret.position.x * to_local), Math::round((caret.position.y + caret.size.y) * to_local));
	}
	Window *window = get_window();
	const DisplayServerEnums::WindowID wid = window->get_window_id();
	if (wid == DisplayServerEnums::INVALID_WINDOW_ID) {
		return;
	}
	Point2 pos = Point2(ime_position) + get_global_position();
	if (window->get_embedder()) {
		pos += get_viewport()->get_popup_base_transform().get_origin();
	}
	pos = window->get_screen_transform().xform(pos);
	DisplayServer::get_singleton()->window_set_ime_position(pos, wid);
}

void CefTexture::_apply_premultiplied_material() {
	if (get_material().is_valid() || get_use_parent_material()) {
		return;
	}
	// CEF renders premultiplied alpha. Assigned through RenderingServer so the material
	// property stays empty and is not saved with the scene.
	if (!shared_premultiplied_material) {
		shared_premultiplied_material = memnew(Ref<CanvasItemMaterial>);
		shared_premultiplied_material->instantiate();
		(*shared_premultiplied_material)->set_blend_mode(CanvasItemMaterial::BLEND_MODE_PREMULT_ALPHA);
	}
	RenderingServer::get_singleton()->canvas_item_set_material(get_canvas_item(), (*shared_premultiplied_material)->get_rid());
}

void CefTexture::cleanup_shared_resources() {
	if (shared_premultiplied_material) {
		memdelete(shared_premultiplied_material);
		shared_premultiplied_material = nullptr;
	}
}

Vector2 CefTexture::_local_to_texture(const Vector2 &p_local) const {
	return p_local * pixel_scale;
}

void CefTexture::_on_files_dropped(const PackedStringArray &p_files) {
	if (!is_visible_in_tree() || browser_texture.is_null()) {
		return;
	}
	const Vector2 local = get_local_mouse_position();
	if (!Rect2(Point2(), get_size()).has_point(local)) {
		return;
	}
	Array files;
	for (const String &file : p_files) {
		files.push_back(file);
	}
	const Vector2 pos = _local_to_texture(local);
	browser_texture->drag_enter(files, pos, DragOperation::EVERY);
	browser_texture->drag_over(pos, DragOperation::EVERY);
	browser_texture->drag_drop(pos);
}

void CefTexture::_finish_browser_drag(const Vector2 &p_local_position) {
	GodotCefBrowserHost *host = _get_host();
	if (!host) {
		return;
	}
	const Vector2 pos = _local_to_texture(p_local_position);
	if (Rect2(Point2(), get_size()).has_point(p_local_position)) {
		browser_texture->drag_drop(pos);
		browser_texture->drag_source_ended(pos, host->get_drag_operation());
	} else {
		browser_texture->drag_leave();
		browser_texture->drag_source_ended(pos, DragOperation::NONE);
	}
	browser_texture->drag_source_system_ended();
}

void CefTexture::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());
	if (browser_texture.is_null()) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->is_pressed() && get_focus_mode_with_override() != FOCUS_NONE) {
			grab_focus();
		}
		if (browser_texture->is_dragging_from_browser() && mb->get_button_index() == MouseButton::LEFT && !mb->is_pressed()) {
			_finish_browser_drag(mb->get_position());
		} else {
			browser_texture->forward_mouse_button_event(mb, pixel_scale, device_scale);
		}
		accept_event();
		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (browser_texture->is_dragging_from_browser()) {
			browser_texture->drag_over(_local_to_texture(mm->get_position()), DragOperation::EVERY);
		} else {
			browser_texture->forward_mouse_motion_event(mm, pixel_scale, device_scale);
		}
		accept_event();
		return;
	}

	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->get_keycode() == Key::NONE && key->get_unicode() != 0) {
			ime_composing = false;
		}
		browser_texture->forward_key_event(key, ime_active);
		accept_event();
		return;
	}

	if (Object::cast_to<InputEventPanGesture>(p_event.ptr()) || Object::cast_to<InputEventMagnifyGesture>(p_event.ptr()) ||
			Object::cast_to<InputEventScreenTouch>(p_event.ptr()) || Object::cast_to<InputEventScreenDrag>(p_event.ptr())) {
		browser_texture->forward_input_event(p_event, pixel_scale, device_scale, ime_active);
		accept_event();
	}
}

String CefTexture::get_tooltip(const Point2 &p_pos) const {
	if (!last_tooltip.is_empty()) {
		return last_tooltip;
	}
	return TextureRect::get_tooltip(p_pos);
}

Control::CursorShape CefTexture::get_cursor_shape(const Point2 &p_pos) const {
	GodotCefBrowserHost *host = _get_host();
	if (host && host->has_browser()) {
		return CursorShape(host->get_cursor());
	}
	return TextureRect::get_cursor_shape(p_pos);
}

void CefTexture::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			GodotCefRuntime::ensure_frame_hook();
			set_process_internal(true);
			Window *window = get_window();
			if (window && !window->is_connected(SNAME("files_dropped"), callable_mp(this, &CefTexture::_on_files_dropped))) {
				window->connect(SNAME("files_dropped"), callable_mp(this, &CefTexture::_on_files_dropped));
			}
			_update_view();
		} break;
		case NOTIFICATION_READY: {
			_apply_premultiplied_material();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_set_ime_active(false);
			Window *window = get_window();
			if (window && window->is_connected(SNAME("files_dropped"), callable_mp(this, &CefTexture::_on_files_dropped))) {
				window->disconnect(SNAME("files_dropped"), callable_mp(this, &CefTexture::_on_files_dropped));
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_view();
			_update_cursor_and_tooltip();
			_update_ime();
		} break;
		case NOTIFICATION_RESIZED: {
			_update_view();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			GodotCefBrowserHost *host = _get_host();
			if (host && host->has_browser()) {
				host->get_cef_host()->WasHidden(!is_visible_in_tree());
			}
			if (!is_visible_in_tree()) {
				_set_ime_active(false);
			}
		} break;
		case NOTIFICATION_FOCUS_ENTER: {
			browser_texture->set_focused(true);
		} break;
		case NOTIFICATION_FOCUS_EXIT: {
			browser_texture->set_focused(false);
			_set_ime_active(false);
		} break;
		case NOTIFICATION_MOUSE_EXIT: {
			if (!browser_texture->is_dragging_from_browser()) {
				browser_texture->forward_mouse_exit(get_local_mouse_position(), pixel_scale, device_scale);
			}
		} break;
		case MainLoop::NOTIFICATION_OS_IME_UPDATE: {
			GodotCefBrowserHost *host = _get_host();
			if (!ime_active || !host || !host->has_browser()) {
				break;
			}
			DisplayServer *ds = DisplayServer::get_singleton();
			const String text = ds->ime_get_text();
			if (!text.is_empty()) {
				GodotCefInput::ime_set_composition(host->get_cef_host(), text, ds->ime_get_selection());
				ime_composing = true;
			} else if (ime_composing) {
				GodotCefInput::ime_cancel_composition(host->get_cef_host());
				ime_composing = false;
			}
		} break;
	}
}

void CefTexture::_validate_property(PropertyInfo &p_property) const {
	// The browser texture is runtime state and must not be saved with the scene.
	if (p_property.name == "texture") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

/* Forwarded API */

void CefTexture::set_url(const String &p_url) {
	browser_texture->set_url(p_url);
}

String CefTexture::get_url() const {
	return browser_texture->get_url();
}

String CefTexture::get_current_url() const {
	return browser_texture->get_current_url();
}

String CefTexture::get_title() const {
	return browser_texture->get_title();
}

void CefTexture::set_enable_accelerated_osr(bool p_enable) {
	browser_texture->set_enable_accelerated_osr(p_enable);
}

bool CefTexture::get_enable_accelerated_osr() const {
	return browser_texture->get_enable_accelerated_osr();
}

bool CefTexture::is_accelerated() const {
	return browser_texture->is_accelerated();
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
	ime_position_overridden = true;
}

Vector2i CefTexture::get_ime_position() const {
	return ime_position;
}

bool CefTexture::is_browser_ready() const {
	return browser_texture->is_browser_ready();
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

void CefTexture::shutdown() {
	_set_ime_active(false);
	browser_texture->shutdown();
}

Ref<AudioStreamGenerator> CefTexture::create_audio_stream() const {
	return browser_texture->create_audio_stream();
}

int CefTexture::push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback) {
	return browser_texture->push_audio_to_playback(p_playback);
}

bool CefTexture::has_audio_data() const {
	return browser_texture->has_audio_data();
}

int CefTexture::get_audio_buffer_size() const {
	return browser_texture->get_audio_buffer_size();
}

bool CefTexture::is_audio_capture_enabled() const {
	return browser_texture->is_audio_capture_enabled();
}

void CefTexture::drag_enter(const Array &p_file_paths, const Vector2 &p_position, int p_allowed_ops) {
	browser_texture->drag_enter(p_file_paths, _local_to_texture(p_position), p_allowed_ops);
}

void CefTexture::drag_over(const Vector2 &p_position, int p_allowed_ops) {
	browser_texture->drag_over(_local_to_texture(p_position), p_allowed_ops);
}

void CefTexture::drag_leave() {
	browser_texture->drag_leave();
}

void CefTexture::drag_drop(const Vector2 &p_position) {
	browser_texture->drag_drop(_local_to_texture(p_position));
}

void CefTexture::drag_source_ended(const Vector2 &p_position, int p_operation) {
	browser_texture->drag_source_ended(_local_to_texture(p_position), p_operation);
}

void CefTexture::drag_source_system_ended() {
	browser_texture->drag_source_system_ended();
}

bool CefTexture::is_dragging_from_browser() const {
	return browser_texture->is_dragging_from_browser();
}

bool CefTexture::is_drag_over() const {
	return browser_texture->is_drag_over();
}

bool CefTexture::grant_permission(int64_t p_request_id) const {
	return browser_texture->grant_permission(p_request_id);
}

bool CefTexture::deny_permission(int64_t p_request_id) const {
	return browser_texture->deny_permission(p_request_id);
}

bool CefTexture::respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input) const {
	return browser_texture->respond_js_dialog(p_dialog_id, p_success, p_user_input);
}

bool CefTexture::get_all_cookies() const {
	return browser_texture->get_all_cookies();
}

bool CefTexture::get_cookies(const String &p_url, bool p_include_http_only) const {
	return browser_texture->get_cookies(p_url, p_include_http_only);
}

bool CefTexture::set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly) const {
	return browser_texture->set_cookie(p_url, p_name, p_value, p_domain, p_path, p_secure, p_httponly);
}

bool CefTexture::delete_cookies(const String &p_url, const String &p_cookie_name) const {
	return browser_texture->delete_cookies(p_url, p_cookie_name);
}

bool CefTexture::clear_cookies() const {
	return browser_texture->clear_cookies();
}

bool CefTexture::flush_cookies() const {
	return browser_texture->flush_cookies();
}

void CefTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_browser_texture"), &CefTexture::get_browser_texture);
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CefTexture::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CefTexture::get_url);
	ClassDB::bind_method(D_METHOD("get_current_url"), &CefTexture::get_current_url);
	ClassDB::bind_method(D_METHOD("get_title"), &CefTexture::get_title);
	ClassDB::bind_method(D_METHOD("set_enable_accelerated_osr", "enable"), &CefTexture::set_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("get_enable_accelerated_osr"), &CefTexture::get_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("is_accelerated"), &CefTexture::is_accelerated);
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
	ClassDB::bind_method(D_METHOD("is_browser_ready"), &CefTexture::is_browser_ready);
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
	ClassDB::bind_method(D_METHOD("find_text", "query", "forward", "match_case"), &CefTexture::find_text, DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("find_next"), &CefTexture::find_next);
	ClassDB::bind_method(D_METHOD("find_previous"), &CefTexture::find_previous);
	ClassDB::bind_method(D_METHOD("stop_finding"), &CefTexture::stop_finding);
	ClassDB::bind_method(D_METHOD("shutdown"), &CefTexture::shutdown);
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
	ClassDB::bind_method(D_METHOD("respond_js_dialog", "dialog_id", "success", "user_input"), &CefTexture::respond_js_dialog, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_all_cookies"), &CefTexture::get_all_cookies);
	ClassDB::bind_method(D_METHOD("get_cookies", "url", "include_http_only"), &CefTexture::get_cookies, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_cookie", "url", "name", "value", "domain", "path", "secure", "httponly"), &CefTexture::set_cookie, DEFVAL(String()), DEFVAL("/"), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("delete_cookies", "url", "cookie_name"), &CefTexture::delete_cookies, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("clear_cookies"), &CefTexture::clear_cookies);
	ClassDB::bind_method(D_METHOD("flush_cookies"), &CefTexture::flush_cookies);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_accelerated_osr"), "set_enable_accelerated_osr", "get_enable_accelerated_osr");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "set_background_color", "get_background_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "popup_policy", PROPERTY_HINT_ENUM, "Block,Redirect,Signal Only"), "set_popup_policy", "get_popup_policy");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script", PROPERTY_HINT_MULTILINE_TEXT), "set_preload_script", "get_preload_script");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script_path", PROPERTY_HINT_FILE, "*.js"), "set_preload_script_path", "get_preload_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "ime_position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_ime_position", "get_ime_position");

	CefTexture2D::_bind_browser_signals(get_class_static());
}

CefTexture::CefTexture() {
	browser_texture.instantiate();
	browser_texture->_set_auto_start(false);
	browser_texture->_set_signal_forward_target(get_instance_id());
	set_texture(browser_texture);
	set_expand_mode(EXPAND_IGNORE_SIZE);
	set_stretch_mode(STRETCH_SCALE);
	set_focus_mode(FOCUS_ALL);
	set_mouse_filter(MOUSE_FILTER_STOP);
}

CefTexture::~CefTexture() {
	if (browser_texture.is_valid()) {
		browser_texture->_set_signal_forward_target(ObjectID());
		// Close this control's browser right away instead of waiting for the last reference.
		browser_texture->shutdown();
	}
}
