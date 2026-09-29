/**************************************************************************/
/*  cef_texture_2d.cpp                                                    */
/**************************************************************************/

#include "cef_texture_2d.h"

#include "godot_cef_browser.h"
#include "godot_cef_data.h"
#include "godot_cef_input.h"
#include "godot_cef_ipc.h"
#include "godot_cef_runtime.h"
#include "godot_cef_settings.h"

#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/audio/audio_server.h"
#include "servers/audio/effects/audio_stream_generator.h"
#include "servers/rendering/rendering_server.h"

using GodotCefIpcData::to_cef_string;
using GodotCefIpcData::to_godot_string;

/* Cookie callbacks */

namespace {

void _queue_on_texture(ObjectID p_texture, const StringName &p_signal, const Vector<Variant> &p_args) {
	CefTexture2D *texture = ObjectDB::get_instance<CefTexture2D>(p_texture);
	if (texture) {
		texture->_queue_browser_signal(p_signal, p_args);
	}
}

class GodotCefCookieVisitor : public CefCookieVisitor {
	ObjectID texture;
	Array cookies;

public:
	explicit GodotCefCookieVisitor(ObjectID p_texture) :
			texture(p_texture) {}

	~GodotCefCookieVisitor() override {
		if (GodotCefRuntime::is_initialized()) {
			_queue_on_texture(texture, SNAME("cookies_received"), { cookies });
		}
	}

	bool Visit(const CefCookie &p_cookie, int p_count, int p_total, bool &r_delete_cookie) override {
		Ref<CookieInfo> info;
		info.instantiate();
		info->set_name(to_godot_string(CefString(&p_cookie.name)));
		info->set_value(to_godot_string(CefString(&p_cookie.value)));
		info->set_domain(to_godot_string(CefString(&p_cookie.domain)));
		info->set_path(to_godot_string(CefString(&p_cookie.path)));
		info->set_secure(p_cookie.secure);
		info->set_http_only(p_cookie.httponly);
		info->set_same_site(int(p_cookie.same_site));
		info->set_has_expires(p_cookie.has_expires);
		cookies.push_back(info);
		return true;
	}

	IMPLEMENT_REFCOUNTING(GodotCefCookieVisitor);
};

class GodotCefSetCookieCallback : public CefSetCookieCallback {
	ObjectID texture;

public:
	explicit GodotCefSetCookieCallback(ObjectID p_texture) :
			texture(p_texture) {}

	void OnComplete(bool p_success) override {
		_queue_on_texture(texture, SNAME("cookie_set"), { p_success });
	}

	IMPLEMENT_REFCOUNTING(GodotCefSetCookieCallback);
};

class GodotCefDeleteCookiesCallback : public CefDeleteCookiesCallback {
	ObjectID texture;

public:
	explicit GodotCefDeleteCookiesCallback(ObjectID p_texture) :
			texture(p_texture) {}

	void OnComplete(int p_num_deleted) override {
		_queue_on_texture(texture, SNAME("cookies_deleted"), { p_num_deleted });
	}

	IMPLEMENT_REFCOUNTING(GodotCefDeleteCookiesCallback);
};

class GodotCefFlushCookiesCallback : public CefCompletionCallback {
	ObjectID texture;

public:
	explicit GodotCefFlushCookiesCallback(ObjectID p_texture) :
			texture(p_texture) {}

	void OnComplete() override {
		_queue_on_texture(texture, SNAME("cookies_flushed"), {});
	}

	IMPLEMENT_REFCOUNTING(GodotCefFlushCookiesCallback);
};

CefRefPtr<CefCookieManager> _get_cookie_manager() {
	if (GodotCefRuntime::ensure_initialized() != OK) {
		return nullptr;
	}
	return CefCookieManager::GetGlobalManager(nullptr);
}

GodotCefInput::Scale _make_scale(double p_pixel, double p_device) {
	GodotCefInput::Scale scale;
	scale.pixel = p_pixel > 0.0 ? float(p_pixel) : 1.0f;
	scale.device = p_device > 0.0 ? float(p_device) : 1.0f;
	return scale;
}

} // namespace

/* Lifecycle */

void CefTexture2D::_ensure_texture() const {
	if (texture.is_null() && RenderingServer::get_singleton()) {
		texture = RenderingServer::get_singleton()->texture_2d_placeholder_create();
	}
}

bool CefTexture2D::_should_run() const {
	return (auto_start || started) && !shutdown_requested && !creation_failed && !url.is_empty();
}

String CefTexture2D::_get_effective_preload_script() const {
	String script;
	if (!preload_script_path.is_empty()) {
		Error err = OK;
		script = FileAccess::get_file_as_string(preload_script_path, &err);
		if (err != OK) {
			ERR_PRINT(vformat("Godot CEF: failed to read preload script \"%s\".", preload_script_path));
			script = String();
		}
	}
	if (!preload_script.is_empty()) {
		if (!script.is_empty()) {
			script += "\n";
		}
		script += preload_script;
	}
	return script;
}

void CefTexture2D::_create_browser() {
	if (GodotCefRuntime::ensure_initialized() != OK) {
		creation_failed = true;
		return;
	}
	_ensure_texture();
	if (!host) {
		host = memnew(GodotCefBrowserHost(this));
	}
	host->get_render_target().set_texture(texture);
	host->set_view(texture_size, device_scale_factor);
	if (AudioServer::get_singleton()) {
		host->set_audio_sample_rate(int(AudioServer::get_singleton()->get_mix_rate()));
	}

	GodotCefBrowserConfig config;
	config.url = url;
	config.background_color = background_color;
	config.popup_policy = popup_policy;
	config.preload_script = _get_effective_preload_script();
	config.accelerated = enable_accelerated_osr && !accelerated_disabled;
	config.audio_capture = GodotCefSettings::is_audio_capture_enabled();
	config.max_frame_rate = GodotCefSettings::get_max_frame_rate();
	config.permission_policy = GodotCefSettings::get_default_permission_policy();

	if (!host->create_browser(config)) {
		ERR_PRINT(vformat("Godot CEF: failed to create a browser for \"%s\".", url));
		creation_failed = true;
		return;
	}
	CefRefPtr<CefBrowserHost> cef_host = host->get_cef_host();
	if (cef_host) {
		if (zoom_level != 0.0) {
			cef_host->SetZoomLevel(zoom_level);
		}
		if (audio_muted) {
			cef_host->SetAudioMuted(true);
		}
	}
}

void CefTexture2D::_destroy_browser() {
	if (host) {
		memdelete(host);
		host = nullptr;
	}
}

void CefTexture2D::_process_frame() {
	if (needs_recreate) {
		needs_recreate = false;
		accelerated_disabled = true;
		WARN_PRINT("Godot CEF: accelerated OSR failed, recreating the browser with software rendering.");
		_destroy_browser();
	}

	if (_should_run() && (!host || !host->has_browser())) {
		_create_browser();
	}
	if (host) {
		host->process_frame();
	}
	if (pending_changed) {
		pending_changed = false;
		emit_changed();
	}
}

void CefTexture2D::_start() {
	started = true;
}

void CefTexture2D::_apply_view() {
	if (host) {
		host->set_view(texture_size, device_scale_factor);
	}
}

void CefTexture2D::_set_view(const Vector2i &p_physical_size, float p_device_scale) {
	texture_size = Vector2i(MAX(1, p_physical_size.x), MAX(1, p_physical_size.y));
	device_scale_factor = p_device_scale > 0.0f ? p_device_scale : 1.0f;
	_apply_view();
}

void CefTexture2D::_emit_browser_signal(const StringName &p_name, const Vector<Variant> &p_args) {
	LocalVector<const Variant *> argptrs;
	argptrs.resize(p_args.size());
	for (int i = 0; i < p_args.size(); i++) {
		argptrs[i] = &p_args[i];
	}
	// Keep this texture alive while user code runs.
	Ref<CefTexture2D> self = this;
	emit_signalp(p_name, argptrs.ptr(), argptrs.size());
	Object *target = ObjectDB::get_instance(signal_forward_target);
	if (target) {
		target->emit_signalp(p_name, argptrs.ptr(), argptrs.size());
	}
}

void CefTexture2D::_queue_browser_signal(const StringName &p_name, const Vector<Variant> &p_args) {
	if (host && host->has_browser()) {
		host->queue_signal(p_name, p_args);
		return;
	}
	Array args;
	for (const Variant &arg : p_args) {
		args.push_back(arg);
	}
	callable_mp(this, &CefTexture2D::_emit_deferred_signal).call_deferred(p_name, args);
}

void CefTexture2D::_emit_deferred_signal(const StringName &p_name, const Array &p_args) {
	Vector<Variant> args;
	for (const Variant &arg : p_args) {
		args.push_back(arg);
	}
	_emit_browser_signal(p_name, args);
}

bool CefTexture2D::_has_browser_signal_connections(const StringName &p_name) const {
	if (has_connections(p_name)) {
		return true;
	}
	Object *target = ObjectDB::get_instance(signal_forward_target);
	return target && target->has_connections(p_name);
}

void CefTexture2D::_shutdown_for_exit() {
	_destroy_browser();
}

/* Properties */

void CefTexture2D::set_url(const String &p_url) {
	url = p_url;
	shutdown_requested = false;
	creation_failed = false;
	if (host && host->has_browser()) {
		host->load_url(url);
	}
}

String CefTexture2D::get_url() const {
	return url;
}

String CefTexture2D::get_current_url() const {
	return host && host->has_browser() ? host->get_current_url() : url;
}

String CefTexture2D::get_title() const {
	return host ? host->get_title() : String();
}

void CefTexture2D::set_enable_accelerated_osr(bool p_enable) {
	enable_accelerated_osr = p_enable;
}

bool CefTexture2D::get_enable_accelerated_osr() const {
	return enable_accelerated_osr;
}

bool CefTexture2D::is_accelerated() const {
	return host && host->has_browser() && host->get_render_target().is_accelerated();
}

void CefTexture2D::set_background_color(const Color &p_color) {
	background_color = p_color;
	if (host) {
		host->set_background_color(p_color);
	}
}

Color CefTexture2D::get_background_color() const {
	return background_color;
}

void CefTexture2D::set_popup_policy(int p_policy) {
	popup_policy = CLAMP(p_policy, int(POPUP_POLICY_BLOCK), int(POPUP_POLICY_SIGNAL_ONLY));
	if (host) {
		host->set_popup_policy(popup_policy);
	}
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
	_apply_view();
	emit_changed();
}

Vector2i CefTexture2D::get_texture_size() const {
	return texture_size;
}

void CefTexture2D::set_device_scale_factor(float p_scale) {
	device_scale_factor = p_scale > 0.0f ? p_scale : 1.0f;
	_apply_view();
}

float CefTexture2D::get_device_scale_factor() const {
	return device_scale_factor;
}

bool CefTexture2D::is_browser_ready() const {
	return host && host->has_browser();
}

/* Navigation */

void CefTexture2D::eval(const String &p_code) {
	if (host) {
		host->eval(p_code);
	}
}

void CefTexture2D::go_back() {
	if (host && host->has_browser()) {
		host->get_browser()->GoBack();
	}
}

void CefTexture2D::go_forward() {
	if (host && host->has_browser()) {
		host->get_browser()->GoForward();
	}
}

bool CefTexture2D::can_go_back() const {
	return host && host->get_can_go_back();
}

bool CefTexture2D::can_go_forward() const {
	return host && host->get_can_go_forward();
}

void CefTexture2D::reload() {
	if (host && host->has_browser()) {
		host->get_browser()->Reload();
	}
}

void CefTexture2D::reload_ignore_cache() {
	if (host && host->has_browser()) {
		host->get_browser()->ReloadIgnoreCache();
	}
}

void CefTexture2D::stop_loading() {
	if (host && host->has_browser()) {
		host->get_browser()->StopLoad();
	}
}

bool CefTexture2D::is_loading() const {
	return host && host->is_loading();
}

void CefTexture2D::set_zoom_level(double p_zoom_level) {
	zoom_level = p_zoom_level;
	if (host && host->has_browser()) {
		host->get_cef_host()->SetZoomLevel(zoom_level);
	}
}

double CefTexture2D::get_zoom_level() const {
	if (host && host->has_browser()) {
		return host->get_cef_host()->GetZoomLevel();
	}
	return zoom_level;
}

void CefTexture2D::set_audio_muted(bool p_muted) {
	audio_muted = p_muted;
	if (host && host->has_browser()) {
		host->get_cef_host()->SetAudioMuted(p_muted);
	}
}

bool CefTexture2D::is_audio_muted() const {
	if (host && host->has_browser()) {
		return host->get_cef_host()->IsAudioMuted();
	}
	return audio_muted;
}

void CefTexture2D::send_ipc_message(const String &p_message) {
	if (host) {
		host->send_ipc_message(p_message);
	}
}

void CefTexture2D::send_ipc_binary_message(const PackedByteArray &p_data) {
	if (host) {
		host->send_ipc_binary_message(p_data);
	}
}

void CefTexture2D::send_ipc_data(const Variant &p_data) {
	if (host) {
		host->send_ipc_data(p_data);
	}
}

void CefTexture2D::find_text(const String &p_query, bool p_forward, bool p_match_case) {
	last_find_query = p_query;
	last_find_match_case = p_match_case;
	if (!host || !host->has_browser()) {
		return;
	}
	if (p_query.is_empty()) {
		host->get_cef_host()->StopFinding(true);
		return;
	}
	host->get_cef_host()->Find(to_cef_string(p_query), p_forward, p_match_case, false);
}

void CefTexture2D::find_next() {
	if (host && host->has_browser() && !last_find_query.is_empty()) {
		host->get_cef_host()->Find(to_cef_string(last_find_query), true, last_find_match_case, true);
	}
}

void CefTexture2D::find_previous() {
	if (host && host->has_browser() && !last_find_query.is_empty()) {
		host->get_cef_host()->Find(to_cef_string(last_find_query), false, last_find_match_case, true);
	}
}

void CefTexture2D::stop_finding() {
	last_find_query = String();
	if (host && host->has_browser()) {
		host->get_cef_host()->StopFinding(true);
	}
}

void CefTexture2D::set_focused(bool p_focused) {
	if (host && host->has_browser()) {
		host->get_cef_host()->SetFocus(p_focused);
	}
}

void CefTexture2D::shutdown() {
	shutdown_requested = true;
	_destroy_browser();
}

/* Permissions and dialogs */

bool CefTexture2D::grant_permission(int64_t p_request_id) {
	return host && host->grant_permission(p_request_id);
}

bool CefTexture2D::deny_permission(int64_t p_request_id) {
	return host && host->deny_permission(p_request_id);
}

bool CefTexture2D::respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input) {
	return host && host->respond_js_dialog(p_dialog_id, p_success, p_user_input);
}

/* Cookies */

bool CefTexture2D::get_all_cookies() {
	CefRefPtr<CefCookieManager> manager = _get_cookie_manager();
	return manager && manager->VisitAllCookies(new GodotCefCookieVisitor(get_instance_id()));
}

bool CefTexture2D::get_cookies(const String &p_url, bool p_include_http_only) {
	CefRefPtr<CefCookieManager> manager = _get_cookie_manager();
	return manager && manager->VisitUrlCookies(to_cef_string(p_url), p_include_http_only, new GodotCefCookieVisitor(get_instance_id()));
}

bool CefTexture2D::set_cookie(const String &p_url, const String &p_name, const String &p_value, const String &p_domain, const String &p_path, bool p_secure, bool p_httponly) {
	CefRefPtr<CefCookieManager> manager = _get_cookie_manager();
	if (!manager) {
		return false;
	}
	CefCookie cookie;
	CefString(&cookie.name).FromString(p_name.utf8().get_data());
	CefString(&cookie.value).FromString(p_value.utf8().get_data());
	CefString(&cookie.domain).FromString(p_domain.utf8().get_data());
	CefString(&cookie.path).FromString((p_path.is_empty() ? String("/") : p_path).utf8().get_data());
	cookie.secure = p_secure;
	cookie.httponly = p_httponly;
	return manager->SetCookie(to_cef_string(p_url), cookie, new GodotCefSetCookieCallback(get_instance_id()));
}

bool CefTexture2D::delete_cookies(const String &p_url, const String &p_cookie_name) {
	CefRefPtr<CefCookieManager> manager = _get_cookie_manager();
	return manager && manager->DeleteCookies(to_cef_string(p_url), to_cef_string(p_cookie_name), new GodotCefDeleteCookiesCallback(get_instance_id()));
}

bool CefTexture2D::clear_cookies() {
	return delete_cookies(String(), String());
}

bool CefTexture2D::flush_cookies() {
	CefRefPtr<CefCookieManager> manager = _get_cookie_manager();
	return manager && manager->FlushStore(new GodotCefFlushCookiesCallback(get_instance_id()));
}

/* Audio */

Ref<AudioStreamGenerator> CefTexture2D::create_audio_stream() {
	Ref<AudioStreamGenerator> stream;
	stream.instantiate();
	int rate = host ? host->get_audio_sample_rate() : 0;
	if (rate <= 0 && AudioServer::get_singleton()) {
		rate = int(AudioServer::get_singleton()->get_mix_rate());
	}
	stream->set_mix_rate(rate > 0 ? rate : 48000);
	stream->set_buffer_length(0.1);
	return stream;
}

int CefTexture2D::push_audio_to_playback(const Ref<AudioStreamGeneratorPlayback> &p_playback) {
	ERR_FAIL_COND_V(p_playback.is_null(), 0);
	if (!host) {
		return 0;
	}
	const int frames = MIN(p_playback->get_frames_available(), host->get_buffered_audio_frames());
	if (frames <= 0) {
		return 0;
	}
	LocalVector<float> samples;
	samples.resize(uint32_t(frames) * 2);
	const int popped = host->pop_audio(samples.ptr(), frames);
	if (popped <= 0) {
		return 0;
	}
	PackedVector2Array buffer;
	buffer.resize(popped);
	Vector2 *dst = buffer.ptrw();
	for (int i = 0; i < popped; i++) {
		dst[i] = Vector2(samples[i * 2], samples[i * 2 + 1]);
	}
	p_playback->push_buffer(buffer);
	return popped;
}

bool CefTexture2D::has_audio_data() const {
	return host && host->get_buffered_audio_frames() > 0;
}

int CefTexture2D::get_audio_buffer_size() const {
	return host ? host->get_buffered_audio_frames() : 0;
}

bool CefTexture2D::is_audio_capture_enabled() const {
	return GodotCefSettings::is_audio_capture_enabled();
}

/* Drag and drop */

void CefTexture2D::drag_enter(const Array &p_file_paths, const Vector2 &p_position, int p_allowed_ops) {
	if (!host || !host->has_browser()) {
		return;
	}
	CefRefPtr<CefDragData> data = CefDragData::Create();
	for (const Variant &path : p_file_paths) {
		const String file = path;
		data->AddFile(to_cef_string(file), to_cef_string(file.get_file()));
	}
	const CefMouseEvent event = GodotCefInput::make_mouse_event(p_position, _make_scale(1.0, device_scale_factor), 0);
	host->get_cef_host()->DragTargetDragEnter(data, event, static_cast<CefBrowserHost::DragOperationsMask>(p_allowed_ops));
	host->set_drag_over(true);
}

void CefTexture2D::drag_over(const Vector2 &p_position, int p_allowed_ops) {
	if (!host || !host->has_browser() || !host->is_drag_over()) {
		return;
	}
	const CefMouseEvent event = GodotCefInput::make_mouse_event(p_position, _make_scale(1.0, device_scale_factor), 0);
	host->get_cef_host()->DragTargetDragOver(event, static_cast<CefBrowserHost::DragOperationsMask>(p_allowed_ops));
}

void CefTexture2D::drag_leave() {
	if (!host || !host->has_browser() || !host->is_drag_over()) {
		return;
	}
	host->get_cef_host()->DragTargetDragLeave();
	host->set_drag_over(false);
}

void CefTexture2D::drag_drop(const Vector2 &p_position) {
	if (!host || !host->has_browser() || !host->is_drag_over()) {
		return;
	}
	const CefMouseEvent event = GodotCefInput::make_mouse_event(p_position, _make_scale(1.0, device_scale_factor), 0);
	host->get_cef_host()->DragTargetDrop(event);
	host->set_drag_over(false);
}

void CefTexture2D::drag_source_ended(const Vector2 &p_position, int p_operation) {
	if (!host || !host->has_browser()) {
		return;
	}
	const float scale = device_scale_factor > 0.0f ? device_scale_factor : 1.0f;
	host->get_cef_host()->DragSourceEndedAt(int(p_position.x / scale), int(p_position.y / scale), static_cast<CefBrowserHost::DragOperationsMask>(p_operation));
	host->set_dragging_from_browser(false);
}

void CefTexture2D::drag_source_system_ended() {
	if (!host || !host->has_browser()) {
		return;
	}
	host->get_cef_host()->DragSourceSystemDragEnded();
	host->set_dragging_from_browser(false);
}

bool CefTexture2D::is_dragging_from_browser() const {
	return host && host->is_dragging_from_browser();
}

bool CefTexture2D::is_drag_over() const {
	return host && host->is_drag_over();
}

/* Input forwarding */

void CefTexture2D::forward_mouse_button_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_mouse_button(host->get_cef_host(), mb, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_mouse_motion_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_mouse_motion(host->get_cef_host(), mm, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_mouse_exit(const Vector2 &p_position, double p_pixel_scale_factor, double p_device_scale_factor) {
	if (host && host->has_browser()) {
		GodotCefInput::send_mouse_leave(host->get_cef_host(), p_position, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_pan_gesture_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	Ref<InputEventPanGesture> pan = p_event;
	if (pan.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_pan_gesture(host->get_cef_host(), pan, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_magnify_gesture_event(const Ref<InputEvent> &p_event) {
	Ref<InputEventMagnifyGesture> magnify = p_event;
	if (magnify.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_magnify_gesture(host->get_cef_host(), magnify);
	}
}

void CefTexture2D::forward_key_event(const Ref<InputEvent> &p_event, bool p_focus_on_editable_field) {
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_key(host->get_browser(), key, p_focus_on_editable_field);
	}
}

void CefTexture2D::forward_screen_touch_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	Ref<InputEventScreenTouch> touch = p_event;
	if (touch.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_screen_touch(host->get_cef_host(), touch, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_screen_drag_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor) {
	Ref<InputEventScreenDrag> drag = p_event;
	if (drag.is_valid() && host && host->has_browser()) {
		GodotCefInput::send_screen_drag(host->get_cef_host(), drag, _make_scale(p_pixel_scale_factor, p_device_scale_factor));
	}
}

void CefTexture2D::forward_input_event(const Ref<InputEvent> &p_event, double p_pixel_scale_factor, double p_device_scale_factor, bool p_focus_on_editable_field) {
	if (Object::cast_to<InputEventMouseButton>(p_event.ptr())) {
		forward_mouse_button_event(p_event, p_pixel_scale_factor, p_device_scale_factor);
	} else if (Object::cast_to<InputEventMouseMotion>(p_event.ptr())) {
		forward_mouse_motion_event(p_event, p_pixel_scale_factor, p_device_scale_factor);
	} else if (Object::cast_to<InputEventPanGesture>(p_event.ptr())) {
		forward_pan_gesture_event(p_event, p_pixel_scale_factor, p_device_scale_factor);
	} else if (Object::cast_to<InputEventMagnifyGesture>(p_event.ptr())) {
		forward_magnify_gesture_event(p_event);
	} else if (Object::cast_to<InputEventKey>(p_event.ptr())) {
		forward_key_event(p_event, p_focus_on_editable_field);
	} else if (Object::cast_to<InputEventScreenTouch>(p_event.ptr())) {
		forward_screen_touch_event(p_event, p_pixel_scale_factor, p_device_scale_factor);
	} else if (Object::cast_to<InputEventScreenDrag>(p_event.ptr())) {
		forward_screen_drag_event(p_event, p_pixel_scale_factor, p_device_scale_factor);
	}
}

/* Texture2D */

int CefTexture2D::get_width() const {
	if (host && host->get_render_target().get_size().x > 0) {
		return host->get_render_target().get_size().x;
	}
	return texture_size.x;
}

int CefTexture2D::get_height() const {
	if (host && host->get_render_target().get_size().y > 0) {
		return host->get_render_target().get_size().y;
	}
	return texture_size.y;
}

RID CefTexture2D::get_rid() const {
	_ensure_texture();
	GodotCefRuntime::ensure_frame_hook();
	return texture;
}

bool CefTexture2D::has_alpha() const {
	return true;
}

/* Binding */

void CefTexture2D::_bind_browser_signals(const StringName &p_class) {
	const StringName &class_name = p_class;
	ClassDB::add_signal(class_name, MethodInfo("ipc_message", PropertyInfo(Variant::STRING, "message")));
	ClassDB::add_signal(class_name, MethodInfo("ipc_binary_message", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ClassDB::add_signal(class_name, MethodInfo("ipc_data_message", PropertyInfo(Variant::NIL, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ClassDB::add_signal(class_name, MethodInfo("debug_ipc_message", PropertyInfo(Variant::DICTIONARY, "event")));
	ClassDB::add_signal(class_name, MethodInfo("url_changed", PropertyInfo(Variant::STRING, "url")));
	ClassDB::add_signal(class_name, MethodInfo("title_changed", PropertyInfo(Variant::STRING, "title")));
	ClassDB::add_signal(class_name, MethodInfo("load_started", PropertyInfo(Variant::STRING, "url")));
	ClassDB::add_signal(class_name, MethodInfo("load_finished", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "http_status_code")));
	ClassDB::add_signal(class_name, MethodInfo("load_error", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "error_code"), PropertyInfo(Variant::STRING, "error_text")));
	ClassDB::add_signal(class_name, MethodInfo("console_message", PropertyInfo(Variant::INT, "level"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "source"), PropertyInfo(Variant::INT, "line")));
	ClassDB::add_signal(class_name, MethodInfo("drag_started", PropertyInfo(Variant::OBJECT, "drag_data", PROPERTY_HINT_RESOURCE_TYPE, "DragDataInfo"), PropertyInfo(Variant::VECTOR2, "position"), PropertyInfo(Variant::INT, "allowed_ops")));
	ClassDB::add_signal(class_name, MethodInfo("drag_cursor_updated", PropertyInfo(Variant::INT, "operation")));
	ClassDB::add_signal(class_name, MethodInfo("drag_entered", PropertyInfo(Variant::OBJECT, "drag_data", PROPERTY_HINT_RESOURCE_TYPE, "DragDataInfo"), PropertyInfo(Variant::INT, "mask")));
	ClassDB::add_signal(class_name, MethodInfo("download_requested", PropertyInfo(Variant::OBJECT, "download_info", PROPERTY_HINT_RESOURCE_TYPE, "DownloadRequestInfo")));
	ClassDB::add_signal(class_name, MethodInfo("download_updated", PropertyInfo(Variant::OBJECT, "download_info", PROPERTY_HINT_RESOURCE_TYPE, "DownloadUpdateInfo")));
	ClassDB::add_signal(class_name, MethodInfo("render_process_terminated", PropertyInfo(Variant::INT, "status"), PropertyInfo(Variant::STRING, "error_message")));
	ClassDB::add_signal(class_name, MethodInfo("popup_requested", PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "disposition"), PropertyInfo(Variant::BOOL, "user_gesture")));
	ClassDB::add_signal(class_name, MethodInfo("permission_requested", PropertyInfo(Variant::STRING, "permission_type"), PropertyInfo(Variant::STRING, "url"), PropertyInfo(Variant::INT, "request_id")));
	ClassDB::add_signal(class_name, MethodInfo("js_dialog_requested", PropertyInfo(Variant::INT, "dialog_type"), PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::STRING, "default_prompt_text"), PropertyInfo(Variant::INT, "dialog_id")));
	ClassDB::add_signal(class_name, MethodInfo("find_result", PropertyInfo(Variant::INT, "count"), PropertyInfo(Variant::INT, "active_index"), PropertyInfo(Variant::BOOL, "final_update")));
	ClassDB::add_signal(class_name, MethodInfo("cookies_received", PropertyInfo(Variant::ARRAY, "cookies")));
	ClassDB::add_signal(class_name, MethodInfo("cookie_set", PropertyInfo(Variant::BOOL, "success")));
	ClassDB::add_signal(class_name, MethodInfo("cookies_deleted", PropertyInfo(Variant::INT, "num_deleted")));
	ClassDB::add_signal(class_name, MethodInfo("cookies_flushed"));
}

void CefTexture2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_url", "url"), &CefTexture2D::set_url);
	ClassDB::bind_method(D_METHOD("get_url"), &CefTexture2D::get_url);
	ClassDB::bind_method(D_METHOD("get_current_url"), &CefTexture2D::get_current_url);
	ClassDB::bind_method(D_METHOD("get_title"), &CefTexture2D::get_title);
	ClassDB::bind_method(D_METHOD("set_enable_accelerated_osr", "enable"), &CefTexture2D::set_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("get_enable_accelerated_osr"), &CefTexture2D::get_enable_accelerated_osr);
	ClassDB::bind_method(D_METHOD("is_accelerated"), &CefTexture2D::is_accelerated);
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
	ClassDB::bind_method(D_METHOD("set_device_scale_factor", "scale"), &CefTexture2D::set_device_scale_factor);
	ClassDB::bind_method(D_METHOD("get_device_scale_factor"), &CefTexture2D::get_device_scale_factor);
	ClassDB::bind_method(D_METHOD("is_browser_ready"), &CefTexture2D::is_browser_ready);

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
	ClassDB::bind_method(D_METHOD("find_text", "query", "forward", "match_case"), &CefTexture2D::find_text, DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("find_next"), &CefTexture2D::find_next);
	ClassDB::bind_method(D_METHOD("find_previous"), &CefTexture2D::find_previous);
	ClassDB::bind_method(D_METHOD("stop_finding"), &CefTexture2D::stop_finding);
	ClassDB::bind_method(D_METHOD("set_focused", "focused"), &CefTexture2D::set_focused);
	ClassDB::bind_method(D_METHOD("shutdown"), &CefTexture2D::shutdown);

	ClassDB::bind_method(D_METHOD("grant_permission", "request_id"), &CefTexture2D::grant_permission);
	ClassDB::bind_method(D_METHOD("deny_permission", "request_id"), &CefTexture2D::deny_permission);
	ClassDB::bind_method(D_METHOD("respond_js_dialog", "dialog_id", "success", "user_input"), &CefTexture2D::respond_js_dialog, DEFVAL(String()));

	ClassDB::bind_method(D_METHOD("get_all_cookies"), &CefTexture2D::get_all_cookies);
	ClassDB::bind_method(D_METHOD("get_cookies", "url", "include_http_only"), &CefTexture2D::get_cookies, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_cookie", "url", "name", "value", "domain", "path", "secure", "httponly"), &CefTexture2D::set_cookie, DEFVAL(String()), DEFVAL("/"), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("delete_cookies", "url", "cookie_name"), &CefTexture2D::delete_cookies, DEFVAL(String()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("clear_cookies"), &CefTexture2D::clear_cookies);
	ClassDB::bind_method(D_METHOD("flush_cookies"), &CefTexture2D::flush_cookies);

	ClassDB::bind_method(D_METHOD("create_audio_stream"), &CefTexture2D::create_audio_stream);
	ClassDB::bind_method(D_METHOD("push_audio_to_playback", "playback"), &CefTexture2D::push_audio_to_playback);
	ClassDB::bind_method(D_METHOD("has_audio_data"), &CefTexture2D::has_audio_data);
	ClassDB::bind_method(D_METHOD("get_audio_buffer_size"), &CefTexture2D::get_audio_buffer_size);
	ClassDB::bind_method(D_METHOD("is_audio_capture_enabled"), &CefTexture2D::is_audio_capture_enabled);

	ClassDB::bind_method(D_METHOD("drag_enter", "file_paths", "position", "allowed_ops"), &CefTexture2D::drag_enter);
	ClassDB::bind_method(D_METHOD("drag_over", "position", "allowed_ops"), &CefTexture2D::drag_over);
	ClassDB::bind_method(D_METHOD("drag_leave"), &CefTexture2D::drag_leave);
	ClassDB::bind_method(D_METHOD("drag_drop", "position"), &CefTexture2D::drag_drop);
	ClassDB::bind_method(D_METHOD("drag_source_ended", "position", "operation"), &CefTexture2D::drag_source_ended);
	ClassDB::bind_method(D_METHOD("drag_source_system_ended"), &CefTexture2D::drag_source_system_ended);
	ClassDB::bind_method(D_METHOD("is_dragging_from_browser"), &CefTexture2D::is_dragging_from_browser);
	ClassDB::bind_method(D_METHOD("is_drag_over"), &CefTexture2D::is_drag_over);

	ClassDB::bind_method(D_METHOD("forward_mouse_button_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_mouse_button_event, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_mouse_motion_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_mouse_motion_event, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_mouse_exit", "position", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_mouse_exit, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_pan_gesture_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_pan_gesture_event, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_magnify_gesture_event", "event"), &CefTexture2D::forward_magnify_gesture_event);
	ClassDB::bind_method(D_METHOD("forward_key_event", "event", "focus_on_editable_field"), &CefTexture2D::forward_key_event, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("forward_screen_touch_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_screen_touch_event, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_screen_drag_event", "event", "pixel_scale_factor", "device_scale_factor"), &CefTexture2D::forward_screen_drag_event, DEFVAL(1.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("forward_input_event", "event", "pixel_scale_factor", "device_scale_factor", "focus_on_editable_field"), &CefTexture2D::forward_input_event, DEFVAL(1.0), DEFVAL(1.0), DEFVAL(false));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "url"), "set_url", "get_url");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_accelerated_osr"), "set_enable_accelerated_osr", "get_enable_accelerated_osr");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "set_background_color", "get_background_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "popup_policy", PROPERTY_HINT_ENUM, "Block,Redirect,Signal Only"), "set_popup_policy", "get_popup_policy");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script", PROPERTY_HINT_MULTILINE_TEXT), "set_preload_script", "get_preload_script");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preload_script_path", PROPERTY_HINT_FILE, "*.js"), "set_preload_script_path", "get_preload_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "texture_size", PROPERTY_HINT_NONE, "suffix:px"), "set_texture_size", "get_texture_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "device_scale_factor", PROPERTY_HINT_RANGE, "0.25,4,0.05"), "set_device_scale_factor", "get_device_scale_factor");

	BIND_ENUM_CONSTANT(POPUP_POLICY_BLOCK);
	BIND_ENUM_CONSTANT(POPUP_POLICY_REDIRECT);
	BIND_ENUM_CONSTANT(POPUP_POLICY_SIGNAL_ONLY);

	_bind_browser_signals(get_class_static());
}

CefTexture2D::CefTexture2D() {
	GodotCefRuntime::register_texture(this);
}

CefTexture2D::~CefTexture2D() {
	GodotCefRuntime::unregister_texture(this);
	_destroy_browser();
	if (texture.is_valid() && RenderingServer::get_singleton()) {
		RenderingServer::get_singleton()->free_rid(texture);
	}
}
