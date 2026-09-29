/**************************************************************************/
/*  godot_cef_paths.h                                                     */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

// Location of the CEF runtime assembled by the build (see godot_cef_builders.py).
//
// macOS:   <root>/Godot CEF.app/Contents/Frameworks/{Chromium Embedded Framework.framework, Godot CEF Helper*.app}
// Windows: <root>/{libcef.dll, godot_cef_helper.exe, *.pak, icudtl.dat, locales/...}
struct GodotCefRuntimePaths {
	static const char *RUNTIME_DIR_NAME;
	static const char *RUNTIME_DIR_ENV;
	static const char *MACOS_RUNTIME_APP;
	static const char *MACOS_FRAMEWORK;
	static const char *MACOS_HELPER;
	static const char *WINDOWS_LIBRARY;
	static const char *WINDOWS_HELPER;

	String root_dir;
	String main_bundle_path;
	String framework_dir;
	String library_path;
	String subprocess_path;
	String resources_dir;
	String locales_dir;

	static GodotCefRuntimePaths from_macos_runtime_dir(const String &p_root_dir);
	static GodotCefRuntimePaths from_windows_runtime_dir(const String &p_root_dir);
	static GodotCefRuntimePaths from_runtime_dir(const String &p_root_dir);

	// Directories where the runtime is looked up, in priority order.
	static Vector<String> get_candidate_runtime_dirs(const String &p_executable_path);
	// Returns the first candidate whose files exist, or an empty struct.
	static GodotCefRuntimePaths detect(const String &p_executable_path);

	bool is_empty() const { return root_dir.is_empty(); }
	bool files_exist() const;
};
