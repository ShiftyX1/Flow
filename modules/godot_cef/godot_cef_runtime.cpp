/**************************************************************************/
/*  godot_cef_runtime.cpp                                                 */
/**************************************************************************/

#include "godot_cef_runtime.h"

#include "core/io/file_access.h"

bool GodotCefRuntime::initialized = false;
String GodotCefRuntime::last_error;

void GodotCefRuntime::initialize() {
	initialized = false;
	last_error = "Godot CEF C++ backend is not linked yet; CEF SDK integration is required.";

	if (FileAccess::exists("res://addons/godot_cef/godot_cef.gdextension")) {
		WARN_PRINT_ONCE("Godot CEF native module is enabled, but res://addons/godot_cef/godot_cef.gdextension is present. Remove the old GDExtension addon to avoid duplicate Godot CEF integration.");
	}
}

void GodotCefRuntime::shutdown() {
	initialized = false;
}

bool GodotCefRuntime::is_initialized() {
	return initialized;
}

String GodotCefRuntime::get_last_error() {
	return last_error;
}

Error GodotCefRuntime::ensure_browser_runtime() {
	if (initialized) {
		return OK;
	}
	ERR_PRINT_ONCE(last_error);
	return ERR_UNAVAILABLE;
}
