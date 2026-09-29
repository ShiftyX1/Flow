/**************************************************************************/
/*  godot_cef_browser.h                                                   */
/**************************************************************************/

#pragma once

#include "godot_cef_include.h"
#include "godot_cef_osr.h"

#include "core/object/object_id.h"
#include "core/os/mutex.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"

#include <functional>

class CefTexture2D;
class GodotCefBrowserHost;

struct GodotCefBrowserConfig {
	String url;
	Color background_color;
	int popup_policy = 0;
	String preload_script;
	bool accelerated = true;
	bool audio_capture = false;
	int max_frame_rate = 0;
	int permission_policy = 0;
};

// Events produced on CEF threads, emitted as signals from the main loop.
struct GodotCefPendingSignal {
	StringName name;
	Vector<Variant> args;
};

// Implements every CEF handler for one browser. Lives as long as CEF keeps a reference;
// the owning GodotCefBrowserHost detaches itself when it is destroyed.
class GodotCefClient : public CefClient,
					   public CefRenderHandler,
					   public CefLifeSpanHandler,
					   public CefDisplayHandler,
					   public CefLoadHandler,
					   public CefRequestHandler,
					   public CefContextMenuHandler,
					   public CefFocusHandler,
					   public CefJSDialogHandler,
					   public CefDownloadHandler,
					   public CefFindHandler,
					   public CefDragHandler,
					   public CefPermissionHandler,
					   public CefAudioHandler,
					   public CefDialogHandler {
	GodotCefBrowserHost *host = nullptr; // UI thread only.
	bool audio_capture = false;
	bool counted_as_open = false;

	Mutex event_mutex;
	LocalVector<GodotCefPendingSignal> events;

	Mutex audio_mutex;
	LocalVector<float> audio_samples; // Interleaved stereo.
	int audio_sample_rate = 48000;
	int audio_channels = 2;

public:
	GodotCefClient(GodotCefBrowserHost *p_host, bool p_audio_capture);

	void detach() { host = nullptr; }
	void push_event(const StringName &p_name, const Vector<Variant> &p_args);
	void take_events(LocalVector<GodotCefPendingSignal> &r_events);

	void set_audio_sample_rate(int p_rate);
	int pop_audio(float *r_samples, int p_frames);
	int get_buffered_audio_frames();
	int get_audio_sample_rate();

	// CefClient
	CefRefPtr<CefAudioHandler> GetAudioHandler() override { return audio_capture ? this : nullptr; }
	CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
	CefRefPtr<CefDialogHandler> GetDialogHandler() override { return this; }
	CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
	CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
	CefRefPtr<CefDragHandler> GetDragHandler() override { return this; }
	CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }
	CefRefPtr<CefFocusHandler> GetFocusHandler() override { return this; }
	CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
	CefRefPtr<CefPermissionHandler> GetPermissionHandler() override { return this; }
	CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
	CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
	bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

	// CefRenderHandler
	bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
	bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int viewX, int viewY, int &screenX, int &screenY) override;
	bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo &screen_info) override;
	void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
	void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect &rect) override;
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void *buffer, int width, int height) override;
	void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const CefAcceleratedPaintInfo &info) override;
	bool StartDragging(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> drag_data, CefRenderHandler::DragOperationsMask allowed_ops, int x, int y) override;
	void UpdateDragCursor(CefRefPtr<CefBrowser> browser, CefRenderHandler::DragOperation operation) override;
	void OnImeCompositionRangeChanged(CefRefPtr<CefBrowser> browser, const CefRange &selected_range, const RectList &character_bounds) override;

	// CefLifeSpanHandler
	bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int popup_id, const CefString &target_url, const CefString &target_frame_name, CefLifeSpanHandler::WindowOpenDisposition target_disposition, bool user_gesture, const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client, CefBrowserSettings &settings, CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) override;
	void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	// CefDisplayHandler
	void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &url) override;
	void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString &title) override;
	bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString &message, const CefString &source, int line) override;
	bool OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor, cef_cursor_type_t type, const CefCursorInfo &custom_cursor_info) override;
	bool OnTooltip(CefRefPtr<CefBrowser> browser, CefString &text) override;

	// CefLoadHandler
	void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) override;
	void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transition_type) override;
	void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;
	void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString &errorText, const CefString &failedUrl) override;

	// CefRequestHandler
	bool OnCertificateError(CefRefPtr<CefBrowser> browser, cef_errorcode_t cert_error, const CefString &request_url, CefRefPtr<CefSSLInfo> ssl_info, CefRefPtr<CefCallback> callback) override;
	void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status, int error_code, const CefString &error_string) override;

	// CefContextMenuHandler
	void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override;

	// CefFocusHandler
	bool OnSetFocus(CefRefPtr<CefBrowser> browser, FocusSource source) override;

	// CefJSDialogHandler
	bool OnJSDialog(CefRefPtr<CefBrowser> browser, const CefString &origin_url, JSDialogType dialog_type, const CefString &message_text, const CefString &default_prompt_text, CefRefPtr<CefJSDialogCallback> callback, bool &suppress_message) override;
	bool OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser, const CefString &message_text, bool is_reload, CefRefPtr<CefJSDialogCallback> callback) override;

	// CefDownloadHandler
	bool CanDownload(CefRefPtr<CefBrowser> browser, const CefString &url, const CefString &request_method) override { return true; }
	bool OnBeforeDownload(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, const CefString &suggested_name, CefRefPtr<CefBeforeDownloadCallback> callback) override;
	void OnDownloadUpdated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDownloadItem> download_item, CefRefPtr<CefDownloadItemCallback> callback) override;

	// CefFindHandler
	void OnFindResult(CefRefPtr<CefBrowser> browser, int identifier, int count, const CefRect &selectionRect, int activeMatchOrdinal, bool finalUpdate) override;

	// CefDragHandler
	bool OnDragEnter(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDragData> dragData, CefDragHandler::DragOperationsMask mask) override;

	// CefPermissionHandler
	bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefMediaAccessCallback> callback) override;
	bool OnShowPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id, const CefString &requesting_origin, uint32_t requested_permissions, CefRefPtr<CefPermissionPromptCallback> callback) override;
	void OnDismissPermissionPrompt(CefRefPtr<CefBrowser> browser, uint64_t prompt_id, cef_permission_request_result_t result) override;

	// CefAudioHandler
	bool GetAudioParameters(CefRefPtr<CefBrowser> browser, CefAudioParameters &params) override;
	void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser, const CefAudioParameters &params, int channels) override;
	void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser, const float **data, int frames, int64_t pts) override;
	void OnAudioStreamStopped(CefRefPtr<CefBrowser> browser) override;
	void OnAudioStreamError(CefRefPtr<CefBrowser> browser, const CefString &message) override;

	// CefDialogHandler
	bool OnFileDialog(CefRefPtr<CefBrowser> browser, FileDialogMode mode, const CefString &title, const CefString &default_file_path, const std::vector<CefString> &accept_filters, const std::vector<CefString> &accept_extensions, const std::vector<CefString> &accept_descriptions, CefRefPtr<CefFileDialogCallback> callback) override;

	IMPLEMENT_REFCOUNTING(GodotCefClient);
};

// Owns one CEF browser on behalf of a CefTexture2D.
class GodotCefBrowserHost {
	friend class GodotCefClient;

public:
	enum CursorType {
		CURSOR_ARROW,
		CURSOR_IBEAM,
		CURSOR_POINTING_HAND,
		CURSOR_CROSS,
		CURSOR_WAIT,
		CURSOR_BUSY,
		CURSOR_DRAG,
		CURSOR_CAN_DROP,
		CURSOR_FORBIDDEN,
		CURSOR_VSIZE,
		CURSOR_HSIZE,
		CURSOR_BDIAGSIZE,
		CURSOR_FDIAGSIZE,
		CURSOR_MOVE,
		CURSOR_VSPLIT,
		CURSOR_HSPLIT,
		CURSOR_HELP,
	};

private:
	CefTexture2D *owner = nullptr;
	CefRefPtr<GodotCefClient> client;
	CefRefPtr<CefBrowser> browser;
	GodotCefBrowserConfig config;
	GodotCefRenderTarget render_target;

	Size2i physical_size = Size2i(1024, 1024);
	float device_scale = 1.0f;
	uint64_t last_begin_frame_usec = 0;

	String current_url;
	String title;
	bool loading = false;
	bool can_go_back = false;
	bool can_go_forward = false;
	int cursor = CURSOR_ARROW;
	String tooltip;

	bool ime_requested = false;
	Rect2i ime_caret_rect; // CEF view coordinates (DIP).

	bool dragging_from_browser = false;
	bool drag_over = false;
	int drag_operation = 0;

	struct PermissionAggregate {
		CefRefPtr<CefMediaAccessCallback> media_callback;
		CefRefPtr<CefPermissionPromptCallback> prompt_callback;
		uint64_t prompt_id = 0;
		uint32_t requested = 0;
		uint32_t granted = 0;
		int remaining = 0;
	};
	struct PermissionRequest {
		int64_t aggregate_id = 0;
		uint32_t bit = 0;
	};
	HashMap<int64_t, PermissionRequest> permission_requests;
	HashMap<int64_t, PermissionAggregate> permission_aggregates;
	int64_t next_permission_id = 1;

	HashMap<int64_t, CefRefPtr<CefJSDialogCallback>> js_dialogs;
	int64_t next_js_dialog_id = 1;

	void _resolve_permission(int64_t p_request_id, bool p_granted);
	void _finish_aggregate(int64_t p_aggregate_id);
	void _emit_debug_ipc(const char *p_direction, const char *p_lane, const String &p_body, int64_t p_size);
	bool _send_process_message(const char *p_route, const std::function<void(CefRefPtr<CefListValue>)> &p_fill);

public:
	explicit GodotCefBrowserHost(CefTexture2D *p_owner);
	~GodotCefBrowserHost();

	bool create_browser(const GodotCefBrowserConfig &p_config);
	void close_browser();
	bool has_browser() const { return browser != nullptr; }
	CefRefPtr<CefBrowser> get_browser() const { return browser; }
	CefRefPtr<CefBrowserHost> get_cef_host() const;
	GodotCefRenderTarget &get_render_target() { return render_target; }
	const GodotCefBrowserConfig &get_config() const { return config; }

	// Physical pixel size of the rendered texture and the device scale (physical pixels per DIP).
	void set_view(const Size2i &p_physical_size, float p_device_scale);
	Size2i get_physical_size() const { return physical_size; }
	float get_device_scale() const { return device_scale; }
	CefRect get_view_rect_dip() const;

	void set_popup_policy(int p_policy) { config.popup_policy = p_policy; }
	void set_background_color(const Color &p_color);
	void set_max_frame_rate(int p_max_frame_rate);

	// Main loop tick: external begin frame and signal emission.
	void process_frame();

	// Navigation.
	void load_url(const String &p_url);
	void eval(const String &p_code);
	const String &get_current_url() const { return current_url; }
	const String &get_title() const { return title; }
	bool is_loading() const { return loading; }
	bool get_can_go_back() const { return can_go_back; }
	bool get_can_go_forward() const { return can_go_forward; }

	// IPC.
	void send_ipc_message(const String &p_message);
	void send_ipc_binary_message(const PackedByteArray &p_data);
	void send_ipc_data(const Variant &p_data);

	// Cursor / IME / drag state read by CefTexture.
	int get_cursor() const { return cursor; }
	const String &get_tooltip() const { return tooltip; }
	bool is_ime_requested() const { return ime_requested; }
	Rect2i get_ime_caret_rect() const { return ime_caret_rect; }
	bool is_dragging_from_browser() const { return dragging_from_browser; }
	void set_dragging_from_browser(bool p_dragging) { dragging_from_browser = p_dragging; }
	bool is_drag_over() const { return drag_over; }
	void set_drag_over(bool p_over) { drag_over = p_over; }
	int get_drag_operation() const { return drag_operation; }

	// Permissions.
	bool grant_permission(int64_t p_request_id);
	bool deny_permission(int64_t p_request_id);

	bool respond_js_dialog(int64_t p_dialog_id, bool p_success, const String &p_user_input);

	// Audio.
	int pop_audio(float *r_samples, int p_frames);
	int get_buffered_audio_frames() const;
	int get_audio_sample_rate() const;
	void set_audio_sample_rate(int p_rate);

	void emit_owner_signal(const StringName &p_name, const Vector<Variant> &p_args);
	void queue_signal(const StringName &p_name, const Vector<Variant> &p_args);
};
