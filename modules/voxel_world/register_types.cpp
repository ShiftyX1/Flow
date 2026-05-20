#ifndef _3D_DISABLED

#include "register_types.h"

#include "voxel_biome_registry.h"
#include "voxel_block_registry.h"
#include "voxel_scene.h"
#include "voxel_scene_data.h"
#include "voxel_scene_format_loader.h"
#include "voxel_world.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "editor/voxel_block_registry_editor_plugin.h"
#include "editor/voxel_scene_editor_plugin.h"
#endif

static Ref<ResourceFormatLoaderVoxelScene> voxel_scene_loader;
static Ref<ResourceFormatSaverVoxelScene> voxel_scene_saver;

void initialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(VoxelBiomeRegistry);
		GDREGISTER_CLASS(VoxelBlockRegistry);
		GDREGISTER_CLASS(VoxelSceneData);
		GDREGISTER_CLASS(VoxelScene);
		GDREGISTER_CLASS(VoxelWorld);

		voxel_scene_loader.instantiate();
		ResourceLoader::add_resource_format_loader(voxel_scene_loader);
		voxel_scene_saver.instantiate();
		ResourceSaver::add_resource_format_saver(voxel_scene_saver);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<VoxelBlockRegistryEditorPlugin>();
		EditorPlugins::add_by_type<VoxelSceneEditorPlugin>();
	}
#endif
}

void uninitialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (voxel_scene_loader.is_valid()) {
		ResourceLoader::remove_resource_format_loader(voxel_scene_loader);
		voxel_scene_loader.unref();
	}
	if (voxel_scene_saver.is_valid()) {
		ResourceSaver::remove_resource_format_saver(voxel_scene_saver);
		voxel_scene_saver.unref();
	}
}

#endif
