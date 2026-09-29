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

#ifdef TOOLS_ENABLED
#include "editor/godot_cef_export_plugin.h"
#include "editor/plugins/editor_plugin.h"
#endif

void initialize_godot_cef_module(ModuleInitializationLevel p_level) {
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
		GodotCefRuntime::initialize();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(GodotCefExportPlugin);
		GDREGISTER_INTERNAL_CLASS(GodotCefEditorPlugin);
		EditorPlugins::add_by_type<GodotCefEditorPlugin>();
	}
#endif
}

void uninitialize_godot_cef_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// The scene tree is already gone, the RenderingServer still exists.
		GodotCefRuntime::shutdown();
		CefTexture::cleanup_shared_resources();
	}
}
