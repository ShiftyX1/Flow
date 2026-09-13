/**************************************************************************/
/*  godot_cef_runtime.h                                                   */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"

class GodotCefRuntime {
	static bool initialized;
	static String last_error;

public:
	static void initialize();
	static void shutdown();
	static bool is_initialized();
	static String get_last_error();
	static Error ensure_browser_runtime();
};
