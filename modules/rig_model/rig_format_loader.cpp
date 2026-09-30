#include "rig_format_loader.h"

#include "rig_animation_data.h"
#include "rig_controller_data.h"
#include "rig_model_data.h"

ResourceFormatLoaderRigData::Kind ResourceFormatLoaderRigData::kind_for_path(const String &p_path) {
	const String lower = p_path.to_lower();
	if (lower.ends_with(".geo.json")) {
		return KIND_MODEL;
	}
	if (lower.ends_with(".anim.json") || lower.ends_with(".animation.json")) {
		return KIND_ANIMATION;
	}
	if (lower.ends_with(".controller.json") || lower.ends_with(".ctrl.json") || lower.ends_with(".animation_controller.json")) {
		return KIND_CONTROLLER;
	}
	return KIND_NONE;
}

bool ResourceFormatLoaderRigData::recognize_path(const String &p_path, const String &p_for_type) const {
	const Kind kind = kind_for_path(p_path);
	if (kind == KIND_NONE) {
		return false;
	}
	if (p_for_type.is_empty()) {
		return true;
	}
	return handles_type(p_for_type);
}

void ResourceFormatLoaderRigData::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

bool ResourceFormatLoaderRigData::handles_type(const String &p_type) const {
	return p_type == "RigModelData" || p_type == "RigAnimationData" || p_type == "RigControllerData" || p_type == "Resource";
}

String ResourceFormatLoaderRigData::get_resource_type(const String &p_path) const {
	switch (kind_for_path(p_path)) {
		case KIND_MODEL:
			return "RigModelData";
		case KIND_ANIMATION:
			return "RigAnimationData";
		case KIND_CONTROLLER:
			return "RigControllerData";
		default:
			return "";
	}
}

Ref<Resource> ResourceFormatLoaderRigData::load(const String &p_path, const String &p_original_path,
		Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Error err = ERR_FILE_UNRECOGNIZED;
	Ref<Resource> result;

	switch (kind_for_path(p_path)) {
		case KIND_MODEL: {
			Ref<RigModelData> res;
			res.instantiate();
			err = res->load_from_file(p_path);
			result = res;
		} break;
		case KIND_ANIMATION: {
			Ref<RigAnimationData> res;
			res.instantiate();
			err = res->load_from_file(p_path);
			result = res;
		} break;
		case KIND_CONTROLLER: {
			Ref<RigControllerData> res;
			res.instantiate();
			err = res->load_from_file(p_path);
			result = res;
		} break;
		default:
			break;
	}

	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		return Ref<Resource>();
	}
	return result;
}
