#include "register_types.h"

#include "moba_map_3d.h"
#include "moba_map_marker_3d.h"
#include "moba_map_preview_3d.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/moba_map_editor_plugin.h"
#endif

void initialize_moba_map_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(MobaMapData);
		GDREGISTER_CLASS(MobaMapPalette);
		GDREGISTER_CLASS(MobaMapMarker3D);
		GDREGISTER_CLASS(MobaMap3D);
		GDREGISTER_CLASS(MobaMapPreview3D);
	}
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<MobaMapEditorPlugin>();
	}
#endif
}

void uninitialize_moba_map_module(ModuleInitializationLevel p_level) {
}
