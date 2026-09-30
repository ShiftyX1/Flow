#pragma once

#include "core/io/resource_loader.h"

// Loads the data-driven rig files as resources. All of them use the ".json" extension, so the
// loader recognises them by suffix (and is registered before the generic JSON loader):
//
//   *.geo.json                                   -> RigModelData
//   *.anim.json, *.animation.json                -> RigAnimationData
//   *.controller.json, *.ctrl.json,
//   *.animation_controller.json                  -> RigControllerData
class ResourceFormatLoaderRigData : public ResourceFormatLoader {
public:
	enum Kind {
		KIND_NONE,
		KIND_MODEL,
		KIND_ANIMATION,
		KIND_CONTROLLER,
	};

	static Kind kind_for_path(const String &p_path);

	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr,
			bool p_use_sub_threads = false, float *r_progress = nullptr,
			CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual bool recognize_path(const String &p_path, const String &p_for_type = String()) const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};
