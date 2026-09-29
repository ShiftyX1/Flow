/**************************************************************************/
/*  godot_cef_export_plugin.cpp                                           */
/**************************************************************************/

#include "godot_cef_export_plugin.h"

#include "../godot_cef_paths.h"

#include "core/io/dir_access.h"
#include "core/os/os.h"

static const char *OPTION_INCLUDE_RUNTIME = "godot_cef/include_runtime";
static const char *OPTION_RUNTIME_PATH = "godot_cef/runtime_path";

bool GodotCefExportPlugin::_is_supported_os(const String &p_os_name) {
	return p_os_name == "macOS" || p_os_name == "Windows";
}

bool GodotCefExportPlugin::supports_platform(const Ref<EditorExportPlatform> &p_export_platform) const {
	return p_export_platform.is_valid() && _is_supported_os(p_export_platform->get_os_name());
}

void GodotCefExportPlugin::_get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const {
	if (!supports_platform(p_export_platform)) {
		return;
	}
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, OPTION_INCLUDE_RUNTIME), true));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, OPTION_RUNTIME_PATH, PROPERTY_HINT_GLOBAL_DIR), ""));
}

String GodotCefExportPlugin::_find_runtime_dir(const String &p_os_name) const {
	Ref<EditorExportPreset> preset = get_export_preset();
	if (preset.is_valid()) {
		const String custom = preset->get(OPTION_RUNTIME_PATH);
		if (!custom.is_empty()) {
			return custom;
		}
	}
	// The editor's own runtime only matches exports for the platform it runs on.
	if (p_os_name != OS::get_singleton()->get_name()) {
		return String();
	}
	const GodotCefRuntimePaths paths = GodotCefRuntimePaths::detect(OS::get_singleton()->get_executable_path());
	return paths.root_dir;
}

String GodotCefExportPlugin::_get_export_option_warning(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const {
	if (p_option_name != OPTION_RUNTIME_PATH || !supports_platform(p_export_platform)) {
		return String();
	}
	Ref<EditorExportPreset> preset = get_export_preset();
	if (preset.is_valid() && !bool(preset->get(OPTION_INCLUDE_RUNTIME))) {
		return String();
	}
	const String os_name = p_export_platform->get_os_name();
	const String runtime_dir = _find_runtime_dir(os_name);
	if (runtime_dir.is_empty()) {
		return vformat("No %s CEF runtime found. Set the path to a GodotCEF directory built for %s.", os_name, os_name);
	}
	if (!GodotCefRuntimePaths::from_runtime_dir(runtime_dir).files_exist()) {
		return vformat("\"%s\" does not contain a complete CEF runtime.", runtime_dir);
	}
	return String();
}

void GodotCefExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	Ref<EditorExportPlatform> platform = get_export_platform();
	Ref<EditorExportPreset> preset = get_export_preset();
	if (!supports_platform(platform) || preset.is_null() || !bool(preset->get(OPTION_INCLUDE_RUNTIME))) {
		return;
	}
	const String os_name = platform->get_os_name();
	const String runtime_dir = _find_runtime_dir(os_name);
	if (runtime_dir.is_empty() || !DirAccess::exists(runtime_dir)) {
		platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, "Godot CEF", vformat("No CEF runtime for %s was found; CefTexture will not work in this export.", os_name));
		return;
	}
	// The runtime directory must end up named GodotCEF: next to the executable on Windows,
	// in Contents/Frameworks on macOS (the default location for shared objects).
	String source = runtime_dir.simplify_path();
	if (source.ends_with("/")) {
		source = source.substr(0, source.length() - 1);
	}
	if (source.get_file() != GodotCefRuntimePaths::RUNTIME_DIR_NAME) {
		platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, "Godot CEF", vformat("The CEF runtime directory must be named \"%s\" (got \"%s\").", GodotCefRuntimePaths::RUNTIME_DIR_NAME, source));
		return;
	}
	add_shared_object(source, Vector<String>());
}

void GodotCefEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			export_plugin.instantiate();
			add_export_plugin(export_plugin);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			remove_export_plugin(export_plugin);
			export_plugin.unref();
		} break;
	}
}
