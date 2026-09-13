/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/

#include "register_types.h"

#include "cef_ipc_inspector.h"
#include "cef_texture.h"
#include "cef_texture_2d.h"
#include "godot_cef_data.h"
#include "godot_cef_runtime.h"
#include "godot_cef_settings.h"

#include "core/object/class_db.h"

void initialize_godot_cef_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GodotCefRuntime::initialize();
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(CefTexture2D);
		GDREGISTER_CLASS(CefTexture);
		GDREGISTER_CLASS(CefIpcInspector);
		GDREGISTER_CLASS(DragDataInfo);
		GDREGISTER_CLASS(DragOperation);
		GDREGISTER_CLASS(DownloadRequestInfo);
		GDREGISTER_CLASS(DownloadUpdateInfo);
		GDREGISTER_CLASS(CookieInfo);

		GodotCefSettings::register_project_settings();
	}
}

void uninitialize_godot_cef_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GodotCefRuntime::shutdown();
	}
}
