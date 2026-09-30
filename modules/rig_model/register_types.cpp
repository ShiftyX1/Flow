#ifndef _3D_DISABLED

#include "register_types.h"

#include "model_rig.h"
#include "rig_animation_data.h"
#include "rig_controller_data.h"
#include "rig_format_loader.h"
#include "rig_model_data.h"

#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

static Ref<ResourceFormatLoaderRigData> rig_data_loader;

void initialize_rig_model_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(RigModelData);
	GDREGISTER_CLASS(RigAnimationData);
	GDREGISTER_CLASS(RigControllerData);
	GDREGISTER_CLASS(ModelRig);

	rig_data_loader.instantiate();
	// Registered at the front so "*.geo.json" etc. win over the generic JSON loader.
	ResourceLoader::add_resource_format_loader(rig_data_loader, true);
}

void uninitialize_rig_model_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (rig_data_loader.is_valid()) {
		ResourceLoader::remove_resource_format_loader(rig_data_loader);
		rig_data_loader.unref();
	}
}

#endif
