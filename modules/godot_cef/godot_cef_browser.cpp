/**************************************************************************/
/*  godot_cef_browser.cpp                                                 */
/**************************************************************************/

#include "godot_cef_browser.h"

#include "cef_texture_2d.h"
#include "common/godot_cef_ipc_contract.h"
#include "godot_cef_data.h"
#include "godot_cef_ipc.h"
#include "godot_cef_runtime.h"
#include "godot_cef_settings.h"

#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "servers/display/display_server.h"

using GodotCefIpcData::to_cef_string;
using GodotCefIpcData::to_godot_string;

static constexpr int AUDIO_MAX_BUFFERED_SECONDS = 2;

static Ref<DragDataInfo> _make_drag_data_info(CefRefPtr<CefDragData> p_data) {
	Ref<DragDataInfo> info = DragDataInfo::create();
	if (!p_data) {
		return info;
	}
	info->set_is_link(p_data->IsLink());
	info->set_is_file(p_data->IsFile());
	info->set_is_fragment(p_data->IsFragment());
	if (p_data->IsLink()) {
		info->set_link_url(to_godot_string(p_data->GetLinkURL()));
		info->set_link_title(to_godot_string(p_data->GetLinkTitle()));
	}
	if (p_data->IsFragment()) {
		info->set_fragment_text(to_godot_string(p_data->GetFragmentText()));
		info->set_fragment_html(to_godot_string(p_data->GetFragmentHtml()));
	}
	if (p_data->IsFile()) {
		std::vector<CefString> names;
		Array file_names;
		if (p_data->GetFileNames(names)) {
			for (const CefString &name : names) {
				file_names.push_back(to_godot_string(name));
			}
		} else if (!p_data->GetFileName().empty()) {
			file_names.push_back(to_godot_string(p_data->GetFileName()));
		}
		info->set_file_names(file_names);
	}
	return info;
}

static int _map_cursor(cef_cursor_type_t p_type) {
	switch (p_type) {
		case CT_IBEAM:
		case CT_VERTICALTEXT:
			return GodotCefBrowserHost::CURSOR_IBEAM;
		case CT_HAND:
			return GodotCefBrowserHost::CURSOR_POINTING_HAND;
		case CT_CROSS:
		case CT_CELL:
			return GodotCefBrowserHost::CURSOR_CROSS;
		case CT_WAIT:
			return GodotCefBrowserHost::CURSOR_WAIT;
		case CT_PROGRESS:
			return GodotCefBrowserHost::CURSOR_BUSY;
		case CT_HELP:
			return GodotCefBrowserHost::CURSOR_HELP;
		case CT_MOVE:
		case CT_MIDDLEPANNING:
		case CT_EASTPANNING:
		case CT_NORTHPANNING:
		case CT_NORTHEASTPANNING:
		case CT_NORTHWESTPANNING:
		case CT_SOUTHPANNING:
		case CT_SOUTHEASTPANNING:
		case CT_SOUTHWESTPANNING:
		case CT_WESTPANNING:
			return GodotCefBrowserHost::CURSOR_MOVE;
		case CT_NORTHRESIZE:
		case CT_SOUTHRESIZE:
		case CT_NORTHSOUTHRESIZE:
			return GodotCefBrowserHost::CURSOR_VSIZE;
		case CT_EASTRESIZE:
		case CT_WESTRESIZE:
		case CT_EASTWESTRESIZE:
			return GodotCefBrowserHost::CURSOR_HSIZE;
		case CT_NORTHEASTRESIZE:
		case CT_SOUTHWESTRESIZE:
		case CT_NORTHEASTSOUTHWESTRESIZE:
			return GodotCefBrowserHost::CURSOR_BDIAGSIZE;
		case CT_NORTHWESTRESIZE:
		case CT_SOUTHEASTRESIZE:
		case CT_NORTHWESTSOUTHEASTRESIZE:
			return GodotCefBrowserHost::CURSOR_FDIAGSIZE;
		case CT_ROWRESIZE:
			return GodotCefBrowserHost::CURSOR_VSPLIT;
		case CT_COLUMNRESIZE:
			return GodotCefBrowserHost::CURSOR_HSPLIT;
		case CT_NOTALLOWED:
		case CT_NODROP:
			return GodotCefBrowserHost::CURSOR_FORBIDDEN;
		case CT_GRAB:
			return GodotCefBrowserHost::CURSOR_CAN_DROP;
		case CT_GRABBING:
		case CT_COPY:
		case CT_ALIAS:
			return GodotCefBrowserHost::CURSOR_DRAG;
		default:
			return GodotCefBrowserHost::CURSOR_ARROW;
	}
}

static String _binary_preview(const PackedByteArray &p_bytes) {
	static constexpr int PREVIEW_BYTES = 32;
	String preview;
	for (int i = 0; i < MIN(PREVIEW_BYTES, p_bytes.size()); i++) {
		preview += String::num_int64(p_bytes[i], 16, false).lpad(2, "0") + " ";
	}
	if (p_bytes.size() > PREVIEW_BYTES) {
		preview += "...";
	}
	return preview.strip_edges();
}

/* GodotCefClient */

GodotCefClient::GodotCefClient(GodotCefBrowserHost *p_host, bool p_audio_capture) :
		host(p_host), audio_capture(p_audio_capture) {}

void GodotCefClient::push_event(const StringName &p_name, const Vector<Variant> &p_args) {
	MutexLock lock(event_mutex);
	GodotCefPendingSignal event;
	event.name = p_name;
	event.args = p_args;
	events.push_back(event);
}

void GodotCefClient::take_events(LocalVector<GodotCefPendingSignal> &r_events) {
	MutexLock lock(event_mutex);
	r_events.clear();
	SWAP(r_events, events);
}

void GodotCefClient::set_audio_sample_rate(int p_rate) {
	MutexLock lock(audio_mutex);
	audio_sample_rate = CLAMP(p_rate, 8000, 192000);
}

int GodotCefClient::get_audio_sample_rate() {
	MutexLock lock(audio_mutex);
	return audio_sample_rate;
}

int GodotCefClient::pop_audio(float *r_samples, int p_frames) {
	MutexLock lock(audio_mutex);
	const int frames = MIN(p_frames, int(audio_samples.size() / 2));
	if (frames <= 0) {
		return 0;
	}
	memcpy(r_samples, audio_samples.ptr(), sizeof(float) * frames * 2);
	const uint32_t remaining = audio_samples.size() - uint32_t(frames) * 2;
	memmove(audio_samples.ptr(), audio_samples.ptr() + frames * 2, sizeof(float) * remaining);
	audio_samples.resize(remaining);
	return frames;
}

int GodotCefClient::get_buffered_audio_frames() {
	MutexLock lock(audio_mutex);
	return int(audio_samples.size() / 2);
}

bool GodotCefClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
	if (!host || source_process != PID_RENDERER) {
		return false;
	}
	const std::string route = message->GetName().ToString();
	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (route == GodotCefIpc::ROUTE_IPC_RENDERER_TO_GODOT) {
		const String text = to_godot_string(args->GetString(0));
		host->_emit_debug_ipc("to_godot", "text", text, text.utf8().length());
		host->queue_signal(SNAME("ipc_message"), { text });
		return true;
	}
	if (route == GodotCefIpc::ROUTE_IPC_BINARY_RENDERER_TO_GODOT) {
		PackedByteArray bytes;
		if (CefRefPtr<CefBinaryValue> binary = args->GetBinary(0)) {
			bytes.resize(int64_t(binary->GetSize()));
			binary->GetData(bytes.ptrw(), binary->GetSize(), 0);
		}
		host->_emit_debug_ipc("to_godot", "binary", _binary_preview(bytes), bytes.size());
		host->queue_signal(SNAME("ipc_binary_message"), { bytes });
		return true;
	}
	if (route == GodotCefIpc::ROUTE_IPC_DATA_RENDERER_TO_GODOT) {
		const Variant data = GodotCefIpcData::cef_value_to_variant(args->GetValue(0));
		const String body = data.stringify();
		host->_emit_debug_ipc("to_godot", "data", body, body.utf8().length());
		host->queue_signal(SNAME("ipc_data_message"), { data });
		return true;
	}
	if (route == GodotCefIpc::ROUTE_TRIGGER_IME) {
		host->ime_requested = args->GetBool(0);
		return true;
	}
	if (route == GodotCefIpc::ROUTE_IME_CARET_POSITION) {
		host->ime_caret_rect = Rect2i(args->GetInt(0), args->GetInt(1), 1, MAX(1, args->GetInt(2)));
		return true;
	}
	return false;
}

bool GodotCefClient::GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
	if (!host) {
		return false;
	}
	rect = host->get_view_rect_dip();
	return true;
}

void GodotCefClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
	rect = host ? host->get_view_rect_dip() : CefRect(0, 0, 1, 1);
}

bool GodotCefClient::GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int &screenX, int &screenY) {
	// The view is not a native window; popups are composited in view space.
	screenX = viewX;
	screenY = viewY;
	return true;
}

bool GodotCefClient::GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo &screen_info) {
	if (!host) {
		return false;
	}
	const CefRect rect = host->get_view_rect_dip();
	screen_info.device_scale_factor = host->device_scale;
	screen_info.depth = 24;
	screen_info.depth_per_component = 8;
	screen_info.is_monochrome = false;
	screen_info.rect = rect;
	screen_info.available_rect = rect;
	return true;
}

void GodotCefClient::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) {
	if (!host) {
		return;
	}
	host->render_target.set_popup_visible(show);
	if (!show) {
		browser->GetHost()->Invalidate(PET_VIEW);
	}
}

void GodotCefClient::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect &rect) {
	if (!host) {
		return;
	}
	const float scale = host->device_scale;
	host->render_target.set_popup_rect(Rect2i(int(rect.x * scale), int(rect.y * scale), int(rect.width * scale), int(rect.height * scale)));
}

void GodotCefClient::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height) {
	if (!host || !host->owner) {
		return;
	}
	if (host->render_target.on_paint(type == PET_POPUP, buffer, width, height)) {
		host->owner->_on_render_target_resized();
	}
}

void GodotCefClient::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const CefAcceleratedPaintInfo &info) {
	if (!host || !host->owner) {
		return;
	}
	if (host->render_target.on_accelerated_paint(type == PET_POPUP, info)) {
		host->owner->_on_render_target_resized();
	}
	if (host->render_target.has_accelerated_failed()) {
		// Recreate the browser without shared textures.
		host->owner->_on_accelerated_osr_failed();
	}
}

bool GodotCefClient::StartDragging(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> drag_data, CefRenderHandler::DragOperationsMask allowed_ops, int x, int y) {
	if (!host) {
		return false;
	}
	host->dragging_from_browser = true;
	host->drag_over = true;
	const float scale = host->device_scale;
	// Continue the drag inside the page, like a native window would.
	CefMouseEvent event;
	event.x = x;
	event.y = y;
	browser->GetHost()->DragTargetDragEnter(drag_data, event, allowed_ops);
	host->queue_signal(SNAME("drag_started"), { _make_drag_data_info(drag_data), Vector2(x * scale, y * scale), int(allowed_ops) });
	return true;
}

void GodotCefClient::UpdateDragCursor(CefRefPtr<CefBrowser> browser, CefRenderHandler::DragOperation operation) {
	if (host) {
		host->drag_operation = int(operation);
		host->queue_signal(SNAME("drag_cursor_updated"), { int(operation) });
	}
}

void GodotCefClient::OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser, const CefRange &selected_range, const RectList &character_bounds) {
	if (!host || character_bounds.empty()) {
		return;
	}
	const CefRect &last = character_bounds.back();
	host->ime_caret_rect = Rect2i(last.x + last.width, last.y, 1, MAX(1, last.height));
}

bool GodotCefClient::OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id, const CefString &target_url, const CefString &target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture, const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client, CefBrowserSettings &settings, CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) {
	if (!host) {
		return true;
	}
	const String url = to_godot_string(target_url);
	switch (host->config.popup_policy) {
		case CefTexture2D::POPUP_POLICY_REDIRECT: {
			if (!url.is_empty()) {
				browser->GetMainFrame()->LoadURL(target_url);
			}
		} break;
		case CefTexture2D::POPUP_POLICY_SIGNAL_ONLY: {
			host->queue_signal(SNAME("popup_requested"), { url, int(target_disposition), user_gesture });
		} break;
		default:
			break;
	}
	// Popups never get their own native window.
	return true;
}

void GodotCefClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
	counted_as_open = true;
	GodotCefRuntime::notify_browser_opened();
}

void GodotCefClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	if (host && host->browser && host->browser->IsSame(browser)) {
		host->browser = nullptr;
	}
	if (counted_as_open) {
		counted_as_open = false;
		GodotCefRuntime::notify_browser_closed();
	}
}

void GodotCefClient::OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &url) {
	if (!host || !frame->IsMain()) {
		return;
	}
	host->current_url = to_godot_string(url);
	host->queue_signal(SNAME("url_changed"), { host->current_url });
}

void GodotCefClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title) {
	if (!host) {
		return;
	}
	host->title = to_godot_string(title);
	host->queue_signal(SNAME("title_changed"), { host->title });
}

bool GodotCefClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString &message, const CefString &source, int line) {
	if (host) {
		host->queue_signal(SNAME("console_message"), { int(level), to_godot_string(message), to_godot_string(source), line });
	}
	return false;
}

bool GodotCefClient::OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor, cef_cursor_type_t type, const CefCursorInfo &custom_cursor_info) {
	if (host) {
		host->cursor = _map_cursor(type);
	}
	return true;
}

bool GodotCefClient::OnTooltip(CefRefPtr<CefBrowser> browser, CefString &text) {
	if (host) {
		host->tooltip = to_godot_string(text);
	}
	return true;
}

void GodotCefClient::OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {
	if (!host) {
		return;
	}
	host->loading = isLoading;
	host->can_go_back = canGoBack;
	host->can_go_forward = canGoForward;
}

void GodotCefClient::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type) {
	if (host && frame->IsMain()) {
		host->queue_signal(SNAME("load_started"), { to_godot_string(frame->GetURL()) });
	}
}

void GodotCefClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
	if (host && frame->IsMain()) {
		host->queue_signal(SNAME("load_finished"), { to_godot_string(frame->GetURL()), httpStatusCode });
	}
}

void GodotCefClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString &errorText, const CefString &failedUrl) {
	if (host && frame->IsMain()) {
		host->queue_signal(SNAME("load_error"), { to_godot_string(failedUrl), int(errorCode), to_godot_string(errorText) });
	}
}

bool GodotCefClient::OnCertificateError(CefRefPtr<CefBrowser> browser, cef_errorcode_t cert_error, const CefString &request_url, CefRefPtr<CefSSLInfo> ssl_info, CefRefPtr<CefCallback> callback) {
	if (GodotCefSettings::is_certificate_errors_ignored()) {
		callback->Continue();
		return true;
	}
	return false;
}

void GodotCefClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status, int error_code, const CefString &error_string) {
	if (!host) {
		return;
	}
	host->ime_requested = false;
	String message = to_godot_string(error_string);
	if (message.is_empty()) {
		switch (status) {
			case TS_ABNORMAL_TERMINATION:
				message = "Abnormal Termination";
				break;
			case TS_PROCESS_WAS_KILLED:
				message = "Process Was Killed";
				break;
			case TS_PROCESS_CRASHED:
				message = "Process Crashed";
				break;
			case TS_PROCESS_OOM:
				message = "Process OOM";
				break;
			default:
				message = "Unknown";
				break;
		}
	}
	host->queue_signal(SNAME("render_process_terminated"), { int(status), message });
}

void GodotCefClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) {
	// Windowless browsers cannot show native menus.
	model->Clear();
}

bool GodotCefClient::OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) {
	return false;
}

bool GodotCefClient::OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString &origin_url, JSDialogType dialog_type, const CefString &message_text, const CefString &default_prompt_text, CefRefPtr<CefJSDialogCallback> callback, bool &suppress_message) {
	if (!host || !host->owner || !host->owner->_has_browser_signal_connections(SNAME("js_dialog_requested"))) {
		// Without a handler, alert() resolves immediately and confirm()/prompt() are declined.
		callback->Continue(dialog_type == JSDIALOGTYPE_ALERT, CefString());
		return true;
	}
	const int64_t id = host->next_js_dialog_id++;
	host->js_dialogs.insert(id, callback);
	host->queue_signal(SNAME("js_dialog_requested"), { int(dialog_type), to_godot_string(message_text), to_godot_string(default_prompt_text), id });
	return true;
}

bool GodotCefClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString &message_text, bool is_reload, CefRefPtr<CefJSDialogCallback> callback) {
	callback->Continue(true, CefString());
	return true;
}

bool GodotCefClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString &suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback) {
	if (host) {
		Ref<DownloadRequestInfo> info;
		info.instantiate();
		info->set_id(download_item->GetId());
		info->set_url(to_godot_string(download_item->GetURL()));
		info->set_original_url(to_godot_string(download_item->GetOriginalUrl()));
		info->set_suggested_file_name(to_godot_string(suggested_name));
		info->set_mime_type(to_godot_string(download_item->GetMimeType()));
		info->set_total_bytes(download_item->GetTotalBytes());
		host->queue_signal(SNAME("download_requested"), { info });
	}
	// Empty path: download with the suggested name into the default location.
	callback->Continue(CefString(), false);
	return true;
}

void GodotCefClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback) {
	if (!host) {
		return;
	}
	Ref<DownloadUpdateInfo> info;
	info.instantiate();
	info->set_id(download_item->GetId());
	info->set_url(to_godot_string(download_item->GetURL()));
	info->set_full_path(to_godot_string(download_item->GetFullPath()));
	info->set_received_bytes(download_item->GetReceivedBytes());
	info->set_total_bytes(download_item->GetTotalBytes());
	info->set_current_speed(download_item->GetCurrentSpeed());
	info->set_percent_complete(download_item->GetPercentComplete());
	info->set_is_in_progress(download_item->IsInProgress());
	info->set_is_complete(download_item->IsComplete());
	info->set_is_canceled(download_item->IsCanceled());
	host->queue_signal(SNAME("download_updated"), { info });
}

void GodotCefClient::OnFindResult(CefRefPtr<CefBrowser> browser, int identifier, int count, const CefRect &selectionRect, int activeMatchOrdinal, bool finalUpdate) {
	if (host) {
		host->queue_signal(SNAME("find_result"), { count, activeMatchOrdinal, finalUpdate });
	}
}

bool GodotCefClient::OnDragEnter(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> dragData, CefDragHandler::DragOperationsMask mask) {
	if (host) {
		host->queue_signal(SNAME("drag_entered"), { _make_drag_data_info(dragData), int(mask) });
	}
	return false;
}

struct GodotCefPermissionLabel {
	uint32_t bit;
	const char *label;
};

static const GodotCefPermissionLabel MEDIA_PERMISSIONS[] = {
	{ CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE, "microphone" },
	{ CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE, "camera" },
	{ CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE, "desktop_audio_capture" },
	{ CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE, "desktop_video_capture" },
};

static const GodotCefPermissionLabel PROMPT_PERMISSIONS[] = {
	{ CEF_PERMISSION_TYPE_CAMERA_STREAM, "camera" },
	{ CEF_PERMISSION_TYPE_MIC_STREAM, "microphone" },
	{ CEF_PERMISSION_TYPE_GEOLOCATION, "geolocation" },
	{ CEF_PERMISSION_TYPE_CLIPBOARD, "clipboard" },
	{ CEF_PERMISSION_TYPE_NOTIFICATIONS, "notifications" },
	{ CEF_PERMISSION_TYPE_MIDI_SYSEX, "midi_sysex" },
	{ CEF_PERMISSION_TYPE_POINTER_LOCK, "pointer_lock" },
	{ CEF_PERMISSION_TYPE_KEYBOARD_LOCK, "keyboard_lock" },
};

template <size_t N>
static LocalVector<GodotCefPermissionLabel> _split_permissions(uint32_t p_requested, const GodotCefPermissionLabel (&p_labels)[N], const char *p_unknown) {
	LocalVector<GodotCefPermissionLabel> out;
	uint32_t known = 0;
	for (const GodotCefPermissionLabel &label : p_labels) {
		known |= label.bit;
		if (p_requested & label.bit) {
			out.push_back(label);
		}
	}
	const uint32_t unknown = p_requested & ~known;
	if (unknown != 0 || out.is_empty()) {
		out.push_back({ unknown != 0 ? unknown : p_requested, p_unknown });
	}
	return out;
}

bool GodotCefClient::OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefMediaAccessCallback> callback) {
	if (!host) {
		callback->Cancel();
		return true;
	}
	switch (host->config.permission_policy) {
		case GodotCefSettings::PERMISSION_POLICY_ALLOW_ALL:
			callback->Continue(requested_permissions);
			return true;
		case GodotCefSettings::PERMISSION_POLICY_SIGNAL:
			break;
		default:
			callback->Continue(CEF_MEDIA_PERMISSION_NONE);
			return true;
	}

	const String origin = to_godot_string(requesting_origin);
	const LocalVector<GodotCefPermissionLabel> labels = _split_permissions(requested_permissions, MEDIA_PERMISSIONS, "unknown_media_permission");
	const int64_t aggregate_id = host->next_permission_id++;
	GodotCefBrowserHost::PermissionAggregate aggregate;
	aggregate.media_callback = callback;
	aggregate.requested = requested_permissions;
	aggregate.remaining = int(labels.size());
	host->permission_aggregates.insert(aggregate_id, aggregate);
	for (const GodotCefPermissionLabel &label : labels) {
		const int64_t request_id = host->next_permission_id++;
		host->permission_requests.insert(request_id, { aggregate_id, label.bit });
		host->queue_signal(SNAME("permission_requested"), { String(label.label), origin, request_id });
	}
	return true;
}

bool GodotCefClient::OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefPermissionPromptCallback> callback) {
	if (!host) {
		callback->Continue(CEF_PERMISSION_RESULT_DENY);
		return true;
	}
	switch (host->config.permission_policy) {
		case GodotCefSettings::PERMISSION_POLICY_ALLOW_ALL:
			callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
			return true;
		case GodotCefSettings::PERMISSION_POLICY_SIGNAL:
			break;
		default:
			callback->Continue(CEF_PERMISSION_RESULT_DENY);
			return true;
	}

	const String origin = to_godot_string(requesting_origin);
	const LocalVector<GodotCefPermissionLabel> labels = _split_permissions(requested_permissions, PROMPT_PERMISSIONS, "unknown_permission");
	const int64_t aggregate_id = host->next_permission_id++;
	GodotCefBrowserHost::PermissionAggregate aggregate;
	aggregate.prompt_callback = callback;
	aggregate.prompt_id = prompt_id;
	aggregate.requested = requested_permissions;
	aggregate.remaining = int(labels.size());
	host->permission_aggregates.insert(aggregate_id, aggregate);
	for (const GodotCefPermissionLabel &label : labels) {
		const int64_t request_id = host->next_permission_id++;
		host->permission_requests.insert(request_id, { aggregate_id, label.bit });
		host->queue_signal(SNAME("permission_requested"), { String(label.label), origin, request_id });
	}
	return true;
}

void GodotCefClient::OnDismissPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id, cef_permission_request_result_t result) {
	if (!host) {
		return;
	}
	LocalVector<int64_t> dismissed;
	for (const KeyValue<int64_t, GodotCefBrowserHost::PermissionAggregate> &kv : host->permission_aggregates) {
		if (kv.value.prompt_callback && kv.value.prompt_id == prompt_id) {
			dismissed.push_back(kv.key);
		}
	}
	for (int64_t aggregate_id : dismissed) {
		host->permission_aggregates.erase(aggregate_id);
	}
	LocalVector<int64_t> orphaned;
	for (const KeyValue<int64_t, GodotCefBrowserHost::PermissionRequest> &kv : host->permission_requests) {
		if (!host->permission_aggregates.has(kv.value.aggregate_id)) {
			orphaned.push_back(kv.key);
		}
	}
	for (int64_t request_id : orphaned) {
		host->permission_requests.erase(request_id);
	}
}

bool GodotCefClient::GetAudioParameters(CefRefPtr<CefBrowser> browser, CefAudioParameters &params) {
	params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
	params.sample_rate = get_audio_sample_rate();
	params.frames_per_buffer = 256;
	return true;
}

void GodotCefClient::OnAudioStreamStarted(CefRefPtr<CefBrowser> browser, const CefAudioParameters &params, int channels) {
	MutexLock lock(audio_mutex);
	audio_channels = MAX(1, channels);
	audio_samples.clear();
}

void GodotCefClient::OnAudioStreamPacket(CefRefPtr<CefBrowser> browser, const float **data, int frames, int64_t pts) {
	if (!data || frames <= 0) {
		return;
	}
	MutexLock lock(audio_mutex);
	const int max_samples = audio_sample_rate * AUDIO_MAX_BUFFERED_SECONDS * 2;
	const uint32_t start = audio_samples.size();
	audio_samples.resize(start + uint32_t(frames) * 2);
	float *out = audio_samples.ptr() + start;
	const float *left = data[0];
	const float *right = audio_channels > 1 ? data[1] : data[0];
	for (int i = 0; i < frames; i++) {
		out[i * 2] = left ? left[i] : 0.0f;
		out[i * 2 + 1] = right ? right[i] : 0.0f;
	}
	if (int(audio_samples.size()) > max_samples) {
		// Drop the oldest samples when nobody consumes the stream.
		const uint32_t excess = audio_samples.size() - uint32_t(max_samples);
		memmove(audio_samples.ptr(), audio_samples.ptr() + excess, sizeof(float) * max_samples);
		audio_samples.resize(uint32_t(max_samples));
	}
}

void GodotCefClient::OnAudioStreamStopped(CefRefPtr<CefBrowser> browser) {
	MutexLock lock(audio_mutex);
	audio_samples.clear();
}

void GodotCefClient::OnAudioStreamError(CefRefPtr<CefBrowser> browser, const CefString &message) {
	if (host) {
		ERR_PRINT("Godot CEF: audio stream error: " + to_godot_string(message));
	}
}

static HashMap<int64_t, CefRefPtr<CefFileDialogCallback>> file_dialog_callbacks;
static int64_t next_file_dialog_id = 1;

static void _file_dialog_done(bool p_status, const Vector<String> &p_paths, int p_filter_index, int64_t p_id) {
	CefRefPtr<CefFileDialogCallback> *callback = file_dialog_callbacks.getptr(p_id);
	if (!callback) {
		return;
	}
	if (p_status && !p_paths.is_empty()) {
		std::vector<CefString> paths;
		for (const String &path : p_paths) {
			paths.push_back(to_cef_string(path));
		}
		(*callback)->Continue(paths);
	} else {
		(*callback)->Cancel();
	}
	file_dialog_callbacks.erase(p_id);
}

bool GodotCefClient::OnFileDialog(CefRefPtr<CefBrowser> browser, FileDialogMode mode, const CefString &title, const CefString &default_file_path, const std::vector<CefString> &accept_filters, const std::vector<CefString> &accept_extensions, const std::vector<CefString> &accept_descriptions, CefRefPtr<CefFileDialogCallback> callback) {
	DisplayServer *ds = DisplayServer::get_singleton();
	if (!ds || !ds->has_feature(DisplayServerEnums::FEATURE_NATIVE_DIALOG_FILE)) {
		return false;
	}

	DisplayServerEnums::FileDialogMode godot_mode = DisplayServerEnums::FILE_DIALOG_MODE_OPEN_FILE;
	switch (mode) {
		case FILE_DIALOG_OPEN_MULTIPLE:
			godot_mode = DisplayServerEnums::FILE_DIALOG_MODE_OPEN_FILES;
			break;
		case FILE_DIALOG_OPEN_FOLDER:
			godot_mode = DisplayServerEnums::FILE_DIALOG_MODE_OPEN_DIR;
			break;
		case FILE_DIALOG_SAVE:
			godot_mode = DisplayServerEnums::FILE_DIALOG_MODE_SAVE_FILE;
			break;
		default:
			break;
	}

	PackedStringArray extensions;
	for (const CefString &filter : accept_filters) {
		const String value = to_godot_string(filter).strip_edges();
		if (value.begins_with(".")) {
			extensions.push_back("*" + value);
		}
	}
	for (const CefString &extension : accept_extensions) {
		const String value = to_godot_string(extension).strip_edges();
		if (value.begins_with(".") && !extensions.has("*" + value)) {
			extensions.push_back("*" + value);
		}
	}
	Vector<String> filters;
	if (!extensions.is_empty()) {
		filters.push_back(String(",").join(extensions));
	}

	const String default_path = to_godot_string(default_file_path);
	const int64_t id = next_file_dialog_id++;
	file_dialog_callbacks.insert(id, callback);
	const Error err = ds->file_dialog_show(to_godot_string(title), default_path.get_base_dir(), default_path.get_file(), false, godot_mode, filters, callable_mp_static(&_file_dialog_done).bind(id));
	if (err != OK) {
		file_dialog_callbacks.erase(id);
		return false;
	}
	return true;
}

/* GodotCefBrowserHost */

GodotCefBrowserHost::GodotCefBrowserHost(CefTexture2D *p_owner) :
		owner(p_owner) {}

GodotCefBrowserHost::~GodotCefBrowserHost() {
	close_browser();
	render_target.clear();
}

static cef_color_t _to_cef_color(const Color &p_color) {
	// Windowless browsers only support fully opaque or fully transparent backgrounds.
	const uint8_t alpha = p_color.a >= 0.5f ? 0xFF : 0x00;
	return CefColorSetARGB(alpha, uint8_t(p_color.get_r8()), uint8_t(p_color.get_g8()), uint8_t(p_color.get_b8()));
}

bool GodotCefBrowserHost::create_browser(const GodotCefBrowserConfig &p_config) {
	ERR_FAIL_COND_V(browser != nullptr, true);
	config = p_config;

	if (config.accelerated) {
		render_target.enable_accelerated();
	}
	client = new GodotCefClient(this, config.audio_capture);

	CefWindowInfo window_info;
	window_info.SetAsWindowless(kNullWindowHandle);
	window_info.shared_texture_enabled = render_target.is_accelerated();
	window_info.external_begin_frame_enabled = true;

	CefBrowserSettings settings;
	settings.windowless_frame_rate = config.max_frame_rate > 0 ? config.max_frame_rate : 60;
	settings.background_color = _to_cef_color(config.background_color);

	CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
	if (!config.preload_script.is_empty()) {
		extra_info->SetString(GodotCefIpc::EXTRA_INFO_PRELOAD_SCRIPT, to_cef_string(config.preload_script));
	}

	browser = CefBrowserHost::CreateBrowserSync(window_info, client, to_cef_string(config.url), settings, extra_info, nullptr);
	if (!browser) {
		client->detach();
		client = nullptr;
		return false;
	}
	current_url = config.url;
	return true;
}

void GodotCefBrowserHost::close_browser() {
	for (KeyValue<int64_t, PermissionAggregate> &kv : permission_aggregates) {
		if (kv.value.media_callback) {
			kv.value.media_callback->Cancel();
		} else if (kv.value.prompt_callback) {
			kv.value.prompt_callback->Continue(CEF_PERMISSION_RESULT_DISMISS);
		}
	}
	permission_aggregates.clear();
	permission_requests.clear();
	for (KeyValue<int64_t, CefRefPtr<CefJSDialogCallback>> &kv : js_dialogs) {
		kv.value->Continue(false, CefString());
	}
	js_dialogs.clear();

	if (browser) {
		CefRefPtr<CefBrowser> closing = browser;
		browser = nullptr;
		closing->GetHost()->CloseBrowser(true);
	}
	if (client) {
		client->detach();
		client = nullptr;
	}
	ime_requested = false;
	dragging_from_browser = false;
	drag_over = false;
	loading = false;
}

CefRefPtr<CefBrowserHost> GodotCefBrowserHost::get_cef_host() const {
	return browser ? browser->GetHost() : nullptr;
}

CefRect GodotCefBrowserHost::get_view_rect_dip() const {
	const float scale = device_scale > 0.0f ? device_scale : 1.0f;
	const int width = MAX(1, int(Math::ceil(physical_size.x / scale)));
	const int height = MAX(1, int(Math::ceil(physical_size.y / scale)));
	return CefRect(0, 0, width, height);
}

void GodotCefBrowserHost::set_view(const Size2i &p_physical_size, float p_device_scale) {
	const Size2i size = Size2i(MAX(1, p_physical_size.x), MAX(1, p_physical_size.y));
	const float scale = p_device_scale > 0.0f ? p_device_scale : 1.0f;
	const bool scale_changed = !Math::is_equal_approx(scale, device_scale);
	if (size == physical_size && !scale_changed) {
		return;
	}
	physical_size = size;
	device_scale = scale;
	if (browser) {
		CefRefPtr<CefBrowserHost> cef_host = browser->GetHost();
		if (scale_changed) {
			cef_host->NotifyScreenInfoChanged();
		}
		cef_host->WasResized();
	}
}

void GodotCefBrowserHost::set_background_color(const Color &p_color) {
	config.background_color = p_color;
}

void GodotCefBrowserHost::set_max_frame_rate(int p_max_frame_rate) {
	if (config.max_frame_rate == p_max_frame_rate) {
		return;
	}
	config.max_frame_rate = p_max_frame_rate;
	if (browser) {
		browser->GetHost()->SetWindowlessFrameRate(p_max_frame_rate > 0 ? p_max_frame_rate : 60);
	}
}

void GodotCefBrowserHost::process_frame() {
	if (browser) {
		const uint64_t now = OS::get_singleton()->get_ticks_usec();
		const uint64_t interval = config.max_frame_rate > 0 ? uint64_t(1000000 / config.max_frame_rate) : 0;
		if (interval == 0 || now - last_begin_frame_usec >= interval) {
			last_begin_frame_usec = now;
			browser->GetHost()->SendExternalBeginFrame();
		}
	}

	if (!client) {
		return;
	}
	LocalVector<GodotCefPendingSignal> events;
	CefRefPtr<GodotCefClient> keep_client = client;
	keep_client->take_events(events);
	for (const GodotCefPendingSignal &event : events) {
		emit_owner_signal(event.name, event.args);
	}
}

void GodotCefBrowserHost::load_url(const String &p_url) {
	config.url = p_url;
	if (browser && !p_url.is_empty()) {
		browser->GetMainFrame()->LoadURL(to_cef_string(p_url));
	}
}

void GodotCefBrowserHost::eval(const String &p_code) {
	if (!browser) {
		return;
	}
	CefRefPtr<CefFrame> frame = browser->GetMainFrame();
	frame->ExecuteJavaScript(to_cef_string(p_code), frame->GetURL(), 0);
}

bool GodotCefBrowserHost::_send_process_message(const char *p_route, const std::function<void(CefRefPtr<CefListValue>)> &p_fill) {
	if (!browser) {
		return false;
	}
	CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create(p_route);
	p_fill(message->GetArgumentList());
	browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, message);
	return true;
}

void GodotCefBrowserHost::send_ipc_message(const String &p_message) {
	const CharString utf8 = p_message.utf8();
	ERR_FAIL_COND_MSG(size_t(utf8.length()) > GodotCefIpc::MAX_IPC_DATA_BYTES, "IPC message exceeds the maximum payload size.");
	if (_send_process_message(GodotCefIpc::ROUTE_IPC_GODOT_TO_RENDERER, [&](CefRefPtr<CefListValue> args) { args->SetString(0, to_cef_string(p_message)); })) {
		_emit_debug_ipc("to_renderer", "text", p_message, utf8.length());
	}
}

void GodotCefBrowserHost::send_ipc_binary_message(const PackedByteArray &p_data) {
	ERR_FAIL_COND_MSG(size_t(p_data.size()) > GodotCefIpc::MAX_IPC_DATA_BYTES, "IPC binary message exceeds the maximum payload size.");
	const bool sent = _send_process_message(GodotCefIpc::ROUTE_IPC_BINARY_GODOT_TO_RENDERER, [&](CefRefPtr<CefListValue> args) {
		if (p_data.is_empty()) {
			args->SetNull(0);
		} else {
			args->SetBinary(0, CefBinaryValue::Create(p_data.ptr(), size_t(p_data.size())));
		}
	});
	if (sent) {
		_emit_debug_ipc("to_renderer", "binary", _binary_preview(p_data), p_data.size());
	}
}

void GodotCefBrowserHost::send_ipc_data(const Variant &p_data) {
	CefRefPtr<CefValue> value = GodotCefIpcData::variant_to_cef_value(p_data);
	if (_send_process_message(GodotCefIpc::ROUTE_IPC_DATA_GODOT_TO_RENDERER, [&](CefRefPtr<CefListValue> args) { args->SetValue(0, value); })) {
		const String body = p_data.stringify();
		_emit_debug_ipc("to_renderer", "data", body, body.utf8().length());
	}
}

void GodotCefBrowserHost::_emit_debug_ipc(const char *p_direction, const char *p_lane, const String &p_body, int64_t p_size) {
	if (!owner || !owner->_has_browser_signal_connections(SNAME("debug_ipc_message"))) {
		return;
	}
	Dictionary event;
	event["direction"] = p_direction;
	event["lane"] = p_lane;
	event["body"] = p_body;
	event["timestamp_unix_ms"] = int64_t(Time::get_singleton()->get_unix_time_from_system() * 1000.0);
	event["body_size_bytes"] = p_size;
	queue_signal(SNAME("debug_ipc_message"), { event });
}

void GodotCefBrowserHost::_finish_aggregate(int64_t p_aggregate_id) {
	PermissionAggregate *aggregate = permission_aggregates.getptr(p_aggregate_id);
	if (!aggregate) {
		return;
	}
	if (aggregate->media_callback) {
		aggregate->media_callback->Continue(aggregate->granted);
	} else if (aggregate->prompt_callback) {
		const bool all_granted = (aggregate->granted & aggregate->requested) == aggregate->requested;
		aggregate->prompt_callback->Continue(all_granted ? CEF_PERMISSION_RESULT_ACCEPT : CEF_PERMISSION_RESULT_DENY);
	}
	permission_aggregates.erase(p_aggregate_id);
}

void GodotCefBrowserHost::_resolve_permission(int64_t p_request_id, bool p_granted) {
	PermissionRequest *request = permission_requests.getptr(p_request_id);
	if (!request) {
		return;
	}
	const int64_t aggregate_id = request->aggregate_id;
	const uint32_t bit = request->bit;
	permission_requests.erase(p_request_id);

	PermissionAggregate *aggregate = permission_aggregates.getptr(aggregate_id);
	if (!aggregate) {
		return;
	}
	if (p_granted) {
		aggregate->granted |= bit;
	}
	aggregate->remaining--;
	// A denied prompt permission denies the whole prompt right away.
	if (aggregate->remaining <= 0 || (!p_granted && aggregate->prompt_callback)) {
		LocalVector<int64_t> siblings;
		for (const KeyValue<int64_t, PermissionRequest> &kv : permission_requests) {
			if (kv.value.aggregate_id == aggregate_id) {
				siblings.push_back(kv.key);
			}
		}
		for (int64_t sibling : siblings) {
			permission_requests.erase(sibling);
		}
		_finish_aggregate(aggregate_id);
	}
}

bool GodotCefBrowserHost::respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input) {
	CefRefPtr<CefJSDialogCallback> *callback = js_dialogs.getptr(p_dialog_id);
	if (!callback) {
		return false;
	}
	(*callback)->Continue(p_success, to_cef_string(p_user_input));
	js_dialogs.erase(p_dialog_id);
	return true;
}

bool GodotCefBrowserHost::grant_permission(int64_t p_request_id) {
	if (!permission_requests.has(p_request_id)) {
		return false;
	}
	_resolve_permission(p_request_id, true);
	return true;
}

bool GodotCefBrowserHost::deny_permission(int64_t p_request_id) {
	if (!permission_requests.has(p_request_id)) {
		return false;
	}
	_resolve_permission(p_request_id, false);
	return true;
}

int GodotCefBrowserHost::pop_audio(float *r_samples, int p_frames) {
	return client ? client->pop_audio(r_samples, p_frames) : 0;
}

int GodotCefBrowserHost::get_buffered_audio_frames() const {
	return client ? client->get_buffered_audio_frames() : 0;
}

int GodotCefBrowserHost::get_audio_sample_rate() const {
	return client ? client->get_audio_sample_rate() : 48000;
}

void GodotCefBrowserHost::set_audio_sample_rate(int p_rate) {
	if (client) {
		client->set_audio_sample_rate(p_rate);
	}
}

void GodotCefBrowserHost::emit_owner_signal(const StringName &p_name, const Vector<Variant> &p_args) {
	if (owner) {
		owner->_emit_browser_signal(p_name, p_args);
	}
}

void GodotCefBrowserHost::queue_signal(const StringName &p_name, const Vector<Variant> &p_args) {
	if (client) {
		client->push_event(p_name, p_args);
	}
}
