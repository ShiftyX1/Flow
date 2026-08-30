/**************************************************************************/
/*  godot_cef_paths.h                                                     */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

struct GodotCefRuntimePaths {
	String root_dir;
	String framework_dir;
	String subprocess_path;
	String library_path;

	static GodotCefRuntimePaths from_macos_universal_dir(const String &p_root_dir);
	bool is_valid_for_macos() const;
};
