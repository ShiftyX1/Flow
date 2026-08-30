/**************************************************************************/
/*  godot_cef_paths.cpp                                                   */
/**************************************************************************/

#include "godot_cef_paths.h"

GodotCefRuntimePaths GodotCefRuntimePaths::from_macos_universal_dir(const String &p_root_dir) {
	GodotCefRuntimePaths paths;
	paths.root_dir = p_root_dir;
	paths.framework_dir = p_root_dir.path_join("Godot CEF.app/Contents/Frameworks");
	paths.subprocess_path = paths.framework_dir.path_join("Godot CEF Helper.app/Contents/MacOS/Godot CEF Helper");
	paths.library_path = p_root_dir.path_join("Godot CEF.framework/libgdcef.dylib");
	return paths;
}

bool GodotCefRuntimePaths::is_valid_for_macos() const {
	return !root_dir.is_empty() &&
			framework_dir.ends_with("Godot CEF.app/Contents/Frameworks") &&
			subprocess_path.ends_with("Godot CEF Helper.app/Contents/MacOS/Godot CEF Helper") &&
			library_path.ends_with("Godot CEF.framework/libgdcef.dylib");
}
