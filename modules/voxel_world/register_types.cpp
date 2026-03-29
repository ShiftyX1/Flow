#ifndef _3D_DISABLED

#include "register_types.h"

#include "voxel_world.h"

#include "core/object/class_db.h"

void initialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(VoxelWorld);
	}
}

void uninitialize_voxel_world_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

#endif
