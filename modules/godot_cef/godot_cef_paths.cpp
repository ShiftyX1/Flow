/**************************************************************************/
/*  godot_cef_paths.cpp                                                   */
/**************************************************************************/

#include "godot_cef_paths.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

const char *GodotCefRuntimePaths::RUNTIME_DIR_NAME = "GodotCEF";
const char *GodotCefRuntimePaths::RUNTIME_DIR_ENV = "GODOT_CEF_RUNTIME_DIR";
const char *GodotCefRuntimePaths::MACOS_RUNTIME_APP = "Godot CEF.app";
const char *GodotCefRuntimePaths::MACOS_FRAMEWORK = "Chromium Embedded Framework.framework";
const char *GodotCefRuntimePaths::MACOS_HELPER = "Godot CEF Helper";
const char *GodotCefRuntimePaths::WINDOWS_LIBRARY = "libcef.dll";
const char *GodotCefRuntimePaths::WINDOWS_HELPER = "godot_cef_helper.exe";

GodotCefRuntimePaths GodotCefRuntimePaths::from_macos_runtime_dir(const String &p_root_dir) {
	GodotCefRuntimePaths paths;
	paths.root_dir = p_root_dir;
	paths.main_bundle_path = p_root_dir.path_join(MACOS_RUNTIME_APP);
	const String frameworks = paths.main_bundle_path.path_join("Contents/Frameworks");
	paths.framework_dir = frameworks.path_join(MACOS_FRAMEWORK);
	paths.library_path = paths.framework_dir.path_join("Chromium Embedded Framework");
	paths.subprocess_path = frameworks.path_join(String(MACOS_HELPER) + ".app/Contents/MacOS").path_join(MACOS_HELPER);
	paths.resources_dir = paths.framework_dir.path_join("Resources");
	paths.locales_dir = paths.resources_dir;
	return paths;
}

GodotCefRuntimePaths GodotCefRuntimePaths::from_windows_runtime_dir(const String &p_root_dir) {
	GodotCefRuntimePaths paths;
	paths.root_dir = p_root_dir;
	paths.library_path = p_root_dir.path_join(WINDOWS_LIBRARY);
	paths.subprocess_path = p_root_dir.path_join(WINDOWS_HELPER);
	paths.resources_dir = p_root_dir;
	paths.locales_dir = p_root_dir.path_join("locales");
	return paths;
}

GodotCefRuntimePaths GodotCefRuntimePaths::from_runtime_dir(const String &p_root_dir) {
#ifdef WINDOWS_ENABLED
	return from_windows_runtime_dir(p_root_dir);
#else
	return from_macos_runtime_dir(p_root_dir);
#endif
}

Vector<String> GodotCefRuntimePaths::get_candidate_runtime_dirs(const String &p_executable_path) {
	Vector<String> dirs;
	OS *os = OS::get_singleton();
	if (os && os->has_environment(RUNTIME_DIR_ENV)) {
		dirs.push_back(os->get_environment(RUNTIME_DIR_ENV));
	}

	const String exe_dir = p_executable_path.get_base_dir();
	dirs.push_back(exe_dir.path_join(RUNTIME_DIR_NAME));
#ifdef MACOS_ENABLED
	// Exported or editor app bundle: Contents/MacOS/<exe> -> Contents/Frameworks/GodotCEF.
	const String contents_dir = exe_dir.get_base_dir();
	dirs.push_back(contents_dir.path_join("Frameworks").path_join(RUNTIME_DIR_NAME));
	dirs.push_back(contents_dir.path_join("Resources").path_join(RUNTIME_DIR_NAME));
	// Development build started from inside a bundle next to bin/GodotCEF.
	dirs.push_back(contents_dir.get_base_dir().get_base_dir().path_join(RUNTIME_DIR_NAME));
#endif
	return dirs;
}

GodotCefRuntimePaths GodotCefRuntimePaths::detect(const String &p_executable_path) {
	for (const String &dir : get_candidate_runtime_dirs(p_executable_path)) {
		if (dir.is_empty()) {
			continue;
		}
		GodotCefRuntimePaths paths = from_runtime_dir(dir.simplify_path());
		if (paths.files_exist()) {
			return paths;
		}
	}
	return GodotCefRuntimePaths();
}

bool GodotCefRuntimePaths::files_exist() const {
	if (root_dir.is_empty()) {
		return false;
	}
	if (!FileAccess::exists(library_path) || !FileAccess::exists(subprocess_path)) {
		return false;
	}
	return DirAccess::exists(resources_dir);
}
