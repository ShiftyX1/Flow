#ifndef _3D_DISABLED

#include "register_types.h"

#include "voxel_block_registry.h"
#include "voxel_world.h"

#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "editor/voxel_block_registry_editor_plugin.h"
#endif

void initialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(VoxelBlockRegistry);
		GDREGISTER_CLASS(VoxelWorld);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<VoxelBlockRegistryEditorPlugin>();
	}
#endif
}

void uninitialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

#endif
