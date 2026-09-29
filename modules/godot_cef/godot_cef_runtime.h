/**************************************************************************/
/*  godot_cef_runtime.h                                                   */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/object/object_id.h"
#include "core/string/ustring.h"

class CefTexture2D;

// Process-wide CEF lifecycle. CEF runs with an external message pump on the main thread,
// pumped from SceneTree::process_frame. All methods except schedule_message_pump_work()
// must be called from the main thread.
class GodotCefRuntime {
public:
	enum State {
		STATE_UNINITIALIZED,
		STATE_INITIALIZED,
		STATE_FAILED,
		STATE_SHUT_DOWN,
	};

	static void initialize();
	static void shutdown();

	static Error ensure_initialized();
	static bool is_initialized();
	static State get_state();
	static String get_last_error();

	static void register_texture(CefTexture2D *p_texture);
	static void unregister_texture(CefTexture2D *p_texture);
	static void ensure_frame_hook();

	// Called from any thread by CefBrowserProcessHandler::OnScheduleMessagePumpWork.
	static void schedule_message_pump_work(int64_t p_delay_ms);

	static void notify_browser_opened();
	static void notify_browser_closed();
	static int get_open_browser_count();

	static void process_frame();
};
