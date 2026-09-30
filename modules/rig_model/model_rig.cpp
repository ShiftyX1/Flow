#include "model_rig.h"

#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/object/object.h"
#include "core/variant/typed_array.h"

namespace {

// Bedrock animation positions use an inverted X axis relative to the engine.
Vector3 bedrock_position_to_engine(const Vector3 &p_value) {
	return Vector3(-p_value.x, p_value.y, p_value.z);
}

StringName short_clip_name(const StringName &p_clip) {
	const String s = String(p_clip);
	const int dot = s.rfind_char('.');
	return StringName(dot >= 0 ? s.substr(dot + 1) : s);
}

} // namespace

ModelRig::ModelRig() {
	set_process_internal(true);
}

// ---------------------------------------------------------------------------------------------
// Construction / teardown
// ---------------------------------------------------------------------------------------------

void ModelRig::_clear_rig() {
	// Detach API attachments so they survive a rebuild.
	for (int i = 0; i < attachments.size(); i++) {
		Node *n = Object::cast_to<Node>(ObjectDB::get_instance(attachments[i].node));
		if (n && n->get_parent()) {
			n->get_parent()->remove_child(n);
		}
	}
	if (rig_root) {
		remove_child(rig_root);
		memdelete(rig_root);
		rig_root = nullptr;
	}
	bones.clear();
	locators.clear();
	acc_rotation.clear();
	acc_position.clear();
	acc_scale.clear();
	touched.clear();
	prev_touched.clear();
	generation++;
}

void ModelRig::_rebuild() {
	if (building) {
		return;
	}
	building = true;
	_clear_rig();

	if (model.is_valid() && model->get_bone_count() > 0) {
		const int n = model->get_bone_count();
		const float scale = model->get_unit_scale();

		rig_root = memnew(Node3D);
		rig_root->set_name("Rig");
		add_child(rig_root);

		bones.resize(n);
		for (int i = 0; i < n; i++) {
			const RigModelData::Bone &b = model->get_bone(i);
			BoneRT &rt = bones.write[i];
			rt.node = memnew(Node3D);
			rt.node->set_name(String(b.name));
			rt.rest_rotation = b.rotation;
		}
		for (int i = 0; i < n; i++) {
			const RigModelData::Bone &b = model->get_bone(i);
			BoneRT &rt = bones.write[i];
			if (b.parent >= 0) {
				bones[b.parent].node->add_child(rt.node);
				rt.rest_position = (b.pivot - model->get_bone(b.parent).pivot) * scale;
			} else {
				rig_root->add_child(rt.node);
				rt.rest_position = b.pivot * scale;
			}

			Ref<ArrayMesh> mesh = model->get_bone_mesh(i);
			if (mesh.is_valid()) {
				rt.mesh = memnew(MeshInstance3D);
				rt.mesh->set_name("Mesh");
				rt.mesh->set_mesh(mesh);
				rt.node->add_child(rt.mesh);
			}

			for (int l = 0; l < b.locators.size(); l++) {
				Node3D *loc = memnew(Node3D);
				loc->set_name(String(b.locators[l].name));
				loc->set_position((b.locators[l].offset - b.pivot) * scale);
				loc->set_basis(Basis::from_euler(RigModelData::bedrock_rotation_to_engine(b.locators[l].rotation), EulerOrder::XYZ));
				rt.node->add_child(loc);
				locators[b.locators[l].name] = loc;
			}

			Transform3D rest(Basis::from_euler(RigModelData::bedrock_rotation_to_engine(rt.rest_rotation), EulerOrder::XYZ), rt.rest_position);
			rt.node->set_transform(rest);
		}

		acc_rotation.resize(n);
		acc_position.resize(n);
		acc_scale.resize(n);
		touched.resize(n);
		prev_touched.resize(n);
		for (int i = 0; i < n; i++) {
			touched.write[i] = 0;
			prev_touched.write[i] = 0;
		}

		auto_materials.clear();
		_apply_render_settings();
	}

	// Re-attach surviving attachments to the new hierarchy.
	for (int i = attachments.size() - 1; i >= 0; i--) {
		Node *n = Object::cast_to<Node>(ObjectDB::get_instance(attachments[i].node));
		if (!n) {
			attachments.remove_at(i);
			continue;
		}
		Node3D *target = _find_attach_target(attachments[i].target);
		if (target) {
			target->add_child(n);
		} else {
			n->queue_free();
			attachments.remove_at(i);
		}
	}

	building = false;
	_restart_animation_state();
}

Ref<Material> ModelRig::_get_bone_material(int p_bone) {
	if (material_override.is_valid()) {
		return material_override;
	}
	const String bone_path = model.is_valid() ? model->get_bone(p_bone).texture_path : String();
	const Ref<StandardMaterial3D> *cached = auto_materials.getptr(bone_path);
	if (cached) {
		return *cached;
	}

	Ref<Texture2D> tex;
	if (!bone_path.is_empty()) {
		if (ResourceLoader::exists(bone_path)) {
			tex = ResourceLoader::load(bone_path, "Texture2D");
		} else {
			WARN_PRINT_ONCE(vformat("ModelRig: bone texture '%s' does not exist.", bone_path));
		}
	} else {
		tex = texture;
		if (tex.is_null() && model.is_valid() && !model->get_texture_path().is_empty() && ResourceLoader::exists(model->get_texture_path())) {
			tex = ResourceLoader::load(model->get_texture_path(), "Texture2D");
		}
	}

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
	mat->set_alpha_scissor_threshold(alpha_cutoff);
	mat->set_cull_mode(double_sided ? BaseMaterial3D::CULL_DISABLED : BaseMaterial3D::CULL_BACK);
	mat->set_roughness(1.0f);
	mat->set_specular(0.0f);
	mat->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, tex);
	auto_materials[bone_path] = mat;
	return mat;
}

void ModelRig::_apply_render_settings() {
	for (int i = 0; i < bones.size(); i++) {
		MeshInstance3D *mi = bones[i].mesh;
		if (!mi) {
			continue;
		}
		mi->set_material_override(_get_bone_material(i));
		mi->set_cast_shadows_setting((GeometryInstance3D::ShadowCastingSetting)cast_shadows);
		mi->set_layer_mask(visual_layers);
	}
}

// ---------------------------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------------------------

void ModelRig::set_model(const Ref<RigModelData> &p_model) {
	model = p_model;
	_rebuild();
	notify_property_list_changed();
}

void ModelRig::set_animations(const TypedArray<RigAnimationData> &p_animations) {
	animation_sets.clear();
	for (int i = 0; i < p_animations.size(); i++) {
		animation_sets.push_back(Ref<RigAnimationData>(p_animations[i]));
	}
	_restart_animation_state();
	notify_property_list_changed();
}

TypedArray<RigAnimationData> ModelRig::get_animations() const {
	TypedArray<RigAnimationData> out;
	for (int i = 0; i < animation_sets.size(); i++) {
		out.push_back(animation_sets[i]);
	}
	return out;
}

void ModelRig::set_controllers(const TypedArray<RigControllerData> &p_controllers) {
	controller_sets.clear();
	for (int i = 0; i < p_controllers.size(); i++) {
		controller_sets.push_back(Ref<RigControllerData>(p_controllers[i]));
	}
	_restart_animation_state();
}

TypedArray<RigControllerData> ModelRig::get_controllers() const {
	TypedArray<RigControllerData> out;
	for (int i = 0; i < controller_sets.size(); i++) {
		out.push_back(controller_sets[i]);
	}
	return out;
}

void ModelRig::set_texture(const Ref<Texture2D> &p_texture) {
	texture = p_texture;
	auto_materials.clear();
	_apply_render_settings();
}

void ModelRig::set_material_override(const Ref<Material> &p_material) {
	material_override = p_material;
	_apply_render_settings();
}

void ModelRig::set_double_sided(bool p_double_sided) {
	double_sided = p_double_sided;
	auto_materials.clear();
	_apply_render_settings();
}

void ModelRig::set_alpha_cutoff(float p_cutoff) {
	alpha_cutoff = CLAMP(p_cutoff, 0.0f, 1.0f);
	auto_materials.clear();
	_apply_render_settings();
}

void ModelRig::set_autoplay_animation(const StringName &p_name) {
	autoplay_animation = p_name;
	if (is_inside_tree() || Engine::get_singleton()->is_editor_hint()) {
		_restart_animation_state();
	}
}

void ModelRig::set_cast_shadows(int p_mode) {
	cast_shadows = CLAMP(p_mode, 0, 3);
	_apply_render_settings();
}

void ModelRig::set_visual_layers(uint32_t p_layers) {
	visual_layers = p_layers;
	_apply_render_settings();
}

void ModelRig::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "autoplay_animation") {
		String names;
		for (int i = 0; i < animation_sets.size(); i++) {
			if (animation_sets[i].is_null()) {
				continue;
			}
			const PackedStringArray list = animation_sets[i]->get_animation_names();
			for (int j = 0; j < list.size(); j++) {
				if (!names.is_empty()) {
					names += ",";
				}
				names += list[j];
			}
		}
		p_property.hint = PROPERTY_HINT_ENUM_SUGGESTION;
		p_property.hint_string = names;
	}
}

// ---------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------

StringName ModelRig::_query_key(const StringName &p_name) {
	const StringName *cached = query_key_cache.getptr(p_name);
	if (cached) {
		return *cached;
	}
	const String s = String(p_name).to_lower();
	StringName key;
	if (s.contains(".")) {
		key = RigExpression::normalize_name(s);
	} else {
		key = StringName("query." + s);
	}
	query_key_cache[p_name] = key;
	return key;
}

void ModelRig::set_query(const StringName &p_name, float p_value) {
	ctx.set(_query_key(p_name), p_value);
}

void ModelRig::set_queries(const Dictionary &p_values) {
	const Array keys = p_values.keys();
	for (int i = 0; i < keys.size(); i++) {
		const Variant v = p_values[keys[i]];
		float f = 0.0f;
		if (v.get_type() == Variant::BOOL) {
			f = (bool)v ? 1.0f : 0.0f;
		} else {
			f = (float)(double)v;
		}
		set_query(StringName(String(keys[i])), f);
	}
}

float ModelRig::get_query(const StringName &p_name) {
	return ctx.get(_query_key(p_name));
}

void ModelRig::set_variable(const StringName &p_name, float p_value) {
	ctx.set(RigExpression::normalize_name("variable." + String(p_name)), p_value);
}

float ModelRig::get_variable(const StringName &p_name) const {
	return ctx.get(RigExpression::normalize_name("variable." + String(p_name)));
}

// ---------------------------------------------------------------------------------------------
// Tracks and controllers
// ---------------------------------------------------------------------------------------------

bool ModelRig::_find_clip(const StringName &p_name, Ref<RigAnimationData> &r_data, StringName &r_clip) const {
	for (int i = 0; i < animation_sets.size(); i++) {
		if (animation_sets[i].is_null()) {
			continue;
		}
		const RigAnimationData::Clip *clip = animation_sets[i]->find_clip(p_name);
		if (clip) {
			r_data = animation_sets[i];
			r_clip = clip->name;
			return true;
		}
	}
	return false;
}

void ModelRig::_resolve_track(Track &r_track, const RigAnimationData::Clip &p_clip) {
	r_track.bone_idx.resize(p_clip.tracks.size());
	for (int i = 0; i < p_clip.tracks.size(); i++) {
		r_track.bone_idx.write[i] = model.is_valid() ? model->find_bone(p_clip.tracks[i].bone) : -1;
	}
	r_track.data_revision = r_track.data->get_revision();
	r_track.generation = generation;
}

void ModelRig::_begin_fade_out(Track &r_track, float p_duration) {
	r_track.fade_start = r_track.fade;
	r_track.fade_time = 0.0f;
	r_track.fade_duration = p_duration;
	r_track.phase = FADE_OUT;
	if (p_duration <= 0.0f) {
		r_track.fade = 0.0f;
	}
}

void ModelRig::_run_scripts(const Vector<RigExpression> &p_scripts) {
	for (int i = 0; i < p_scripts.size(); i++) {
		p_scripts[i].evaluate(ctx);
	}
}

void ModelRig::_enter_state(int p_controller, int p_state, bool p_initial) {
	ControllerRT &c = controllers.write[p_controller];
	const RigControllerData::Controller &def = c.data->get_controller(c.index);
	ERR_FAIL_INDEX(p_state, def.states.size());

	if (!p_initial && c.state >= 0 && c.state < def.states.size()) {
		_run_scripts(def.states[c.state].on_exit);
	}

	const RigControllerData::State &ns = def.states[p_state];
	const float blend = p_initial ? 0.0f : ns.blend_transition;
	for (int i = 0; i < tracks.size(); i++) {
		Track &t = tracks.write[i];
		if (t.owner == p_controller && t.phase != FADE_OUT) {
			_begin_fade_out(t, blend);
		}
	}

	c.state = p_state;
	c.state_time = 0.0f;
	c.revision = c.data->get_revision();
	_run_scripts(ns.on_entry);

	for (int i = 0; i < ns.animations.size(); i++) {
		Track t;
		if (!_find_clip(ns.animations[i].name, t.data, t.clip)) {
			WARN_PRINT_ONCE(vformat("ModelRig: animation '%s' used by controller '%s' was not found in any assigned RigAnimationData.", ns.animations[i].name, c.name));
			continue;
		}
		t.owner = p_controller;
		t.phase_key = StringName("query.anim_phase." + String(short_clip_name(t.clip)).to_lower());
		t.blend = ns.animations[i].blend;
		t.has_blend = !(t.blend.is_constant() && Math::is_equal_approx(t.blend.evaluate(ctx), 1.0f));
		if (blend > 0.0f) {
			t.phase = FADE_IN;
			t.fade = 0.0f;
			t.fade_duration = blend;
		}
		tracks.push_back(t);
	}

	if (!p_initial) {
		StateEvent ev;
		ev.controller = c.name;
		ev.state = ns.name;
		pending_state_events.push_back(ev);
	}
}

void ModelRig::_start_controllers() {
	controllers.clear();
	for (int i = 0; i < controller_sets.size(); i++) {
		if (controller_sets[i].is_null()) {
			continue;
		}
		for (int ci = 0; ci < controller_sets[i]->get_controller_count(); ci++) {
			ControllerRT rt;
			rt.data = controller_sets[i];
			rt.index = ci;
			rt.name = controller_sets[i]->get_controller(ci).name;
			controllers.push_back(rt);
		}
	}
	for (int i = 0; i < controllers.size(); i++) {
		_enter_state(i, controllers[i].data->get_controller(controllers[i].index).initial_state, true);
	}
}

void ModelRig::_apply_autoplay() {
	if (autoplay_animation != StringName() && has_animation(autoplay_animation)) {
		play_animation(autoplay_animation);
	}
}

void ModelRig::_restart_animation_state() {
	if (building) {
		return;
	}
	tracks.clear();
	pending_state_events.clear();
	pending_finished.clear();
	_start_controllers();
	_apply_autoplay();
	if (!bones.is_empty()) {
		for (int i = 0; i < touched.size(); i++) {
			prev_touched.write[i] = 1;
		}
		advance(0.0f);
	}
}

void ModelRig::_step_controllers(float p_delta) {
	static const StringName q_state_time = StringName("query.state_time");
	static const StringName q_all_finished = StringName("query.all_animations_finished");
	static const StringName q_any_finished = StringName("query.any_animation_finished");

	for (int ci = 0; ci < controllers.size(); ci++) {
		if (!controllers[ci].enabled) {
			continue;
		}
		if (controllers[ci].data->get_controller_count() == 0) {
			continue;
		}
		if (controllers[ci].revision != controllers[ci].data->get_revision()) {
			// Data was reloaded; state indices may be stale.
			controllers.write[ci].index = MIN(controllers[ci].index, controllers[ci].data->get_controller_count() - 1);
			_enter_state(ci, controllers[ci].data->get_controller(controllers[ci].index).initial_state, true);
		}
		ControllerRT &c = controllers.write[ci];
		const RigControllerData::Controller &def = c.data->get_controller(c.index);
		if (c.state < 0 || c.state >= def.states.size()) {
			continue;
		}
		c.state_time += p_delta;

		int count = 0;
		bool any_finished = false;
		bool all_finished = true;
		for (int i = 0; i < tracks.size(); i++) {
			const Track &t = tracks[i];
			if (t.owner != ci || t.phase == FADE_OUT) {
				continue;
			}
			count++;
			if (t.finished) {
				any_finished = true;
			} else {
				all_finished = false;
			}
			const RigAnimationData::Clip *clip = t.data->find_clip(t.clip);
			float phase = 0.0f;
			if (clip && clip->length > 0.0f) {
				phase = t.finished ? 1.0f : CLAMP(t.time / clip->length, 0.0f, 1.0f);
			}
			ctx.set(t.phase_key, phase);
		}
		ctx.set(q_state_time, c.state_time);
		ctx.set(q_all_finished, (count == 0 || all_finished) ? 1.0f : 0.0f);
		ctx.set(q_any_finished, any_finished ? 1.0f : 0.0f);

		const RigControllerData::State &state = def.states[c.state];
		for (int ti = 0; ti < state.transitions.size(); ti++) {
			const RigControllerData::Transition &tr = state.transitions[ti];
			if (tr.target_index < 0) {
				continue;
			}
			if (tr.condition.evaluate(ctx) != 0.0f) {
				_enter_state(ci, tr.target_index, false);
				break;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Manual animation control
// ---------------------------------------------------------------------------------------------

bool ModelRig::play_animation(const StringName &p_name, float p_blend_in, float p_weight, float p_speed) {
	Ref<RigAnimationData> data;
	StringName clip_name;
	if (!_find_clip(p_name, data, clip_name)) {
		return false;
	}
	for (int i = 0; i < tracks.size(); i++) {
		Track &t = tracks.write[i];
		if (t.owner == -1 && t.clip == clip_name && t.phase != FADE_OUT) {
			t.weight = p_weight;
			t.speed = p_speed;
			return true;
		}
	}
	Track t;
	t.data = data;
	t.clip = clip_name;
	t.owner = -1;
	t.weight = p_weight;
	t.speed = p_speed;
	if (p_blend_in > 0.0f) {
		t.phase = FADE_IN;
		t.fade = 0.0f;
		t.fade_duration = p_blend_in;
	}
	tracks.push_back(t);
	return true;
}

void ModelRig::stop_animation(const StringName &p_name, float p_blend_out) {
	StringName clip_name;
	if (p_name != StringName()) {
		Ref<RigAnimationData> data;
		if (!_find_clip(p_name, data, clip_name)) {
			return;
		}
	}
	for (int i = 0; i < tracks.size(); i++) {
		Track &t = tracks.write[i];
		if (t.owner == -1 && t.phase != FADE_OUT && (p_name == StringName() || t.clip == clip_name)) {
			_begin_fade_out(t, p_blend_out);
		}
	}
}

bool ModelRig::is_animation_playing(const StringName &p_name) const {
	Ref<RigAnimationData> data;
	StringName clip_name;
	if (!_find_clip(p_name, data, clip_name)) {
		return false;
	}
	for (int i = 0; i < tracks.size(); i++) {
		const Track &t = tracks[i];
		if (t.clip == clip_name && t.phase != FADE_OUT && !t.finished) {
			return true;
		}
	}
	return false;
}

bool ModelRig::has_animation(const StringName &p_name) const {
	Ref<RigAnimationData> data;
	StringName clip_name;
	return _find_clip(p_name, data, clip_name);
}

PackedStringArray ModelRig::get_animation_names() const {
	PackedStringArray out;
	for (int i = 0; i < animation_sets.size(); i++) {
		if (animation_sets[i].is_valid()) {
			out.append_array(animation_sets[i]->get_animation_names());
		}
	}
	return out;
}

// ---------------------------------------------------------------------------------------------
// Controllers API
// ---------------------------------------------------------------------------------------------

StringName ModelRig::get_controller_state(const StringName &p_controller) const {
	for (int i = 0; i < controllers.size(); i++) {
		const ControllerRT &c = controllers[i];
		if (c.name == p_controller || String(c.name).ends_with("." + String(p_controller))) {
			const RigControllerData::Controller &def = c.data->get_controller(c.index);
			if (c.state >= 0 && c.state < def.states.size()) {
				return def.states[c.state].name;
			}
		}
	}
	return StringName();
}

bool ModelRig::set_controller_state(const StringName &p_controller, const StringName &p_state) {
	for (int i = 0; i < controllers.size(); i++) {
		const ControllerRT &c = controllers[i];
		if (c.name == p_controller || String(c.name).ends_with("." + String(p_controller))) {
			const RigControllerData::Controller &def = c.data->get_controller(c.index);
			const int *idx = def.state_lookup.getptr(p_state);
			if (!idx) {
				return false;
			}
			_enter_state(i, *idx, false);
			return true;
		}
	}
	return false;
}

void ModelRig::set_controller_enabled(const StringName &p_controller, bool p_enabled) {
	for (int i = 0; i < controllers.size(); i++) {
		ControllerRT &c = controllers.write[i];
		if (c.name == p_controller || String(c.name).ends_with("." + String(p_controller))) {
			c.enabled = p_enabled;
			if (!p_enabled) {
				for (int t = 0; t < tracks.size(); t++) {
					if (tracks[t].owner == i && tracks[t].phase != FADE_OUT) {
						_begin_fade_out(tracks.write[t], 0.0f);
					}
				}
			} else if (c.state >= 0) {
				_enter_state(i, c.state, true);
			}
		}
	}
}

PackedStringArray ModelRig::get_controller_names() const {
	PackedStringArray out;
	for (int i = 0; i < controllers.size(); i++) {
		out.push_back(String(controllers[i].name));
	}
	return out;
}

// ---------------------------------------------------------------------------------------------
// Bones, locators, attachments
// ---------------------------------------------------------------------------------------------

Node3D *ModelRig::get_bone(const StringName &p_name) const {
	if (model.is_null()) {
		return nullptr;
	}
	const int i = model->find_bone(p_name);
	return (i >= 0 && i < bones.size()) ? bones[i].node : nullptr;
}

Node3D *ModelRig::get_locator(const StringName &p_name) const {
	Node3D *const *loc = locators.getptr(p_name);
	return loc ? *loc : nullptr;
}

PackedStringArray ModelRig::get_bone_names() const {
	return model.is_valid() ? model->get_bone_names() : PackedStringArray();
}

PackedStringArray ModelRig::get_locator_names() const {
	return model.is_valid() ? model->get_locator_names() : PackedStringArray();
}

Node3D *ModelRig::_find_attach_target(const StringName &p_target) const {
	Node3D *loc = get_locator(p_target);
	return loc ? loc : get_bone(p_target);
}

Node *ModelRig::attach_scene(const StringName &p_target, const Ref<PackedScene> &p_scene) {
	ERR_FAIL_COND_V(p_scene.is_null(), nullptr);
	ERR_FAIL_COND_V_MSG(!_find_attach_target(p_target), nullptr, vformat("ModelRig: no bone or locator named '%s'.", p_target));
	Node *inst = p_scene->instantiate();
	ERR_FAIL_NULL_V(inst, nullptr);
	if (!attach_node(p_target, inst)) {
		memdelete(inst);
		return nullptr;
	}
	return inst;
}

bool ModelRig::attach_node(const StringName &p_target, Node *p_node) {
	ERR_FAIL_NULL_V(p_node, false);
	Node3D *target = _find_attach_target(p_target);
	ERR_FAIL_NULL_V_MSG(target, false, vformat("ModelRig: no bone or locator named '%s'.", p_target));
	if (p_node->get_parent()) {
		p_node->get_parent()->remove_child(p_node);
	}
	target->add_child(p_node);
	Attachment a;
	a.target = p_target;
	a.node = p_node->get_instance_id();
	attachments.push_back(a);
	return true;
}

void ModelRig::detach_attachments(const StringName &p_target) {
	for (int i = attachments.size() - 1; i >= 0; i--) {
		if (p_target != StringName() && attachments[i].target != p_target) {
			continue;
		}
		Node *n = Object::cast_to<Node>(ObjectDB::get_instance(attachments[i].node));
		if (n) {
			if (n->get_parent()) {
				n->get_parent()->remove_child(n);
			}
			n->queue_free();
		}
		attachments.remove_at(i);
	}
}

// ---------------------------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------------------------

void ModelRig::advance(float p_delta) {
	if (bones.is_empty()) {
		return;
	}
	static const StringName q_life_time = StringName("query.life_time");
	static const StringName q_delta_time = StringName("query.delta_time");

	const float dt = MAX(0.0f, p_delta) * speed_scale;
	life_time += dt;
	ctx.set(q_life_time, life_time);
	ctx.set(q_delta_time, dt);

	// 1. Advance clip time and fades.
	for (int i = tracks.size() - 1; i >= 0; i--) {
		Track &t = tracks.write[i];
		const RigAnimationData::Clip *clip = t.data->find_clip(t.clip);
		if (!clip) {
			tracks.remove_at(i);
			continue;
		}
		const float step = dt * t.speed;
		if (clip->loop == RigAnimationData::LOOP_REPEAT) {
			t.time = clip->length > 0.0f ? Math::fposmod(t.time + step, clip->length) : 0.0f;
			t.finished = false;
		} else {
			t.time += step;
			if (t.time >= clip->length) {
				t.time = clip->length;
				t.finished = true;
				if (!t.finished_reported && t.owner == -1) {
					t.finished_reported = true;
					pending_finished.push_back(t.clip);
				}
			}
		}

		switch (t.phase) {
			case FADE_IN:
				t.fade_time += dt;
				t.fade = t.fade_duration > 0.0f ? MIN(1.0f, t.fade_time / t.fade_duration) : 1.0f;
				if (t.fade >= 1.0f) {
					t.fade = 1.0f;
					t.phase = FADE_STEADY;
				}
				break;
			case FADE_OUT:
				t.fade_time += dt;
				t.fade = t.fade_duration > 0.0f ? t.fade_start * (1.0f - t.fade_time / t.fade_duration) : 0.0f;
				break;
			case FADE_STEADY:
				break;
		}
	}

	// 2. Controllers.
	_step_controllers(dt);

	// 3. Drop faded-out tracks.
	for (int i = tracks.size() - 1; i >= 0; i--) {
		if (tracks[i].phase == FADE_OUT && tracks[i].fade <= 0.0f) {
			tracks.remove_at(i);
		}
	}

	// 4. Pose.
	_sample_pose();
	_flush_events();
}

void ModelRig::_sample_pose() {
	static const StringName q_anim_time = StringName("query.anim_time");
	const int n = bones.size();
	for (int i = 0; i < n; i++) {
		acc_rotation.write[i] = Vector3();
		acc_position.write[i] = Vector3();
		acc_scale.write[i] = Vector3();
		touched.write[i] = 0;
	}

	// Pass 1: pre_animation scripts (shared variables, smoothing).
	for (int ti = 0; ti < tracks.size(); ti++) {
		const Track &t = tracks[ti];
		if (t.phase == FADE_OUT && t.fade <= 0.0f) {
			continue;
		}
		const RigAnimationData::Clip *clip = t.data->find_clip(t.clip);
		if (!clip || clip->pre_animation.is_empty()) {
			continue;
		}
		ctx.set(q_anim_time, t.time);
		for (int k = 0; k < clip->pre_animation.size(); k++) {
			clip->pre_animation[k].evaluate(ctx);
		}
	}

	// Pass 2: sampling.
	for (int ti = 0; ti < tracks.size(); ti++) {
		Track &t = tracks.write[ti];
		const RigAnimationData::Clip *clip = t.data->find_clip(t.clip);
		if (!clip) {
			continue;
		}
		if (t.data_revision != t.data->get_revision() || t.generation != generation) {
			_resolve_track(t, *clip);
		}
		ctx.set(q_anim_time, t.time);
		float w = t.fade * t.weight;
		if (t.has_blend) {
			w *= t.blend.evaluate(ctx);
		}
		if (w <= 0.00001f) {
			continue;
		}
		for (int k = 0; k < clip->tracks.size(); k++) {
			const int b = t.bone_idx[k];
			if (b < 0) {
				continue;
			}
			const RigAnimationData::BoneTrack &bt = clip->tracks[k];
			if (!bt.rotation.is_empty()) {
				acc_rotation.write[b] += bt.rotation.sample(t.time, ctx) * w;
			}
			if (!bt.position.is_empty()) {
				acc_position.write[b] += bt.position.sample(t.time, ctx) * w;
			}
			if (!bt.scale.is_empty()) {
				acc_scale.write[b] += (bt.scale.sample(t.time, ctx) - Vector3(1, 1, 1)) * w;
			}
			touched.write[b] = 1;
		}
	}

	const float unit = model->get_unit_scale();
	for (int b = 0; b < n; b++) {
		if (!touched[b] && !prev_touched[b]) {
			continue;
		}
		const BoneRT &rt = bones[b];
		Vector3 rotation = rt.rest_rotation;
		Vector3 position = rt.rest_position;
		Vector3 scale(1, 1, 1);
		if (touched[b]) {
			rotation += acc_rotation[b];
			position += bedrock_position_to_engine(acc_position[b]) * unit;
			scale += acc_scale[b];
		}
		Basis basis = Basis::from_euler(RigModelData::bedrock_rotation_to_engine(rotation), EulerOrder::XYZ);
		basis.scale_local(scale);
		rt.node->set_transform(Transform3D(basis, position));
	}
	for (int b = 0; b < n; b++) {
		prev_touched.write[b] = touched[b];
	}
}

void ModelRig::_flush_events() {
	Vector<StringName> finished = pending_finished;
	Vector<StateEvent> events = pending_state_events;
	pending_finished.clear();
	pending_state_events.clear();
	for (int i = 0; i < events.size(); i++) {
		emit_signal(SNAME("controller_state_changed"), events[i].controller, events[i].state);
	}
	for (int i = 0; i < finished.size(); i++) {
		emit_signal(SNAME("animation_finished"), finished[i]);
	}
}

void ModelRig::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_restart_animation_state();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (active) {
				advance((float)get_process_delta_time());
			}
		} break;
	}
}

void ModelRig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_model", "model"), &ModelRig::set_model);
	ClassDB::bind_method(D_METHOD("get_model"), &ModelRig::get_model);
	ClassDB::bind_method(D_METHOD("set_animations", "animations"), &ModelRig::set_animations);
	ClassDB::bind_method(D_METHOD("get_animations"), &ModelRig::get_animations);
	ClassDB::bind_method(D_METHOD("set_controllers", "controllers"), &ModelRig::set_controllers);
	ClassDB::bind_method(D_METHOD("get_controllers"), &ModelRig::get_controllers);
	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &ModelRig::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &ModelRig::get_texture);
	ClassDB::bind_method(D_METHOD("set_double_sided", "double_sided"), &ModelRig::set_double_sided);
	ClassDB::bind_method(D_METHOD("is_double_sided"), &ModelRig::is_double_sided);
	ClassDB::bind_method(D_METHOD("set_alpha_cutoff", "cutoff"), &ModelRig::set_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("get_alpha_cutoff"), &ModelRig::get_alpha_cutoff);
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &ModelRig::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &ModelRig::get_material_override);
	ClassDB::bind_method(D_METHOD("set_autoplay_animation", "name"), &ModelRig::set_autoplay_animation);
	ClassDB::bind_method(D_METHOD("get_autoplay_animation"), &ModelRig::get_autoplay_animation);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &ModelRig::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &ModelRig::is_active);
	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &ModelRig::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &ModelRig::get_speed_scale);
	ClassDB::bind_method(D_METHOD("set_cast_shadows", "mode"), &ModelRig::set_cast_shadows);
	ClassDB::bind_method(D_METHOD("get_cast_shadows"), &ModelRig::get_cast_shadows);
	ClassDB::bind_method(D_METHOD("set_visual_layers", "layers"), &ModelRig::set_visual_layers);
	ClassDB::bind_method(D_METHOD("get_visual_layers"), &ModelRig::get_visual_layers);

	ClassDB::bind_method(D_METHOD("set_query", "name", "value"), &ModelRig::set_query);
	ClassDB::bind_method(D_METHOD("set_queries", "values"), &ModelRig::set_queries);
	ClassDB::bind_method(D_METHOD("get_query", "name"), &ModelRig::get_query);
	ClassDB::bind_method(D_METHOD("set_variable", "name", "value"), &ModelRig::set_variable);
	ClassDB::bind_method(D_METHOD("get_variable", "name"), &ModelRig::get_variable);

	ClassDB::bind_method(D_METHOD("play_animation", "name", "blend_in", "weight", "speed"), &ModelRig::play_animation, DEFVAL(0.0f), DEFVAL(1.0f), DEFVAL(1.0f));
	ClassDB::bind_method(D_METHOD("stop_animation", "name", "blend_out"), &ModelRig::stop_animation, DEFVAL(StringName()), DEFVAL(0.0f));
	ClassDB::bind_method(D_METHOD("is_animation_playing", "name"), &ModelRig::is_animation_playing);
	ClassDB::bind_method(D_METHOD("has_animation", "name"), &ModelRig::has_animation);
	ClassDB::bind_method(D_METHOD("get_animation_names"), &ModelRig::get_animation_names);

	ClassDB::bind_method(D_METHOD("get_controller_state", "controller"), &ModelRig::get_controller_state);
	ClassDB::bind_method(D_METHOD("set_controller_state", "controller", "state"), &ModelRig::set_controller_state);
	ClassDB::bind_method(D_METHOD("set_controller_enabled", "controller", "enabled"), &ModelRig::set_controller_enabled);
	ClassDB::bind_method(D_METHOD("get_controller_names"), &ModelRig::get_controller_names);

	ClassDB::bind_method(D_METHOD("get_bone", "name"), &ModelRig::get_bone);
	ClassDB::bind_method(D_METHOD("get_locator", "name"), &ModelRig::get_locator);
	ClassDB::bind_method(D_METHOD("get_bone_names"), &ModelRig::get_bone_names);
	ClassDB::bind_method(D_METHOD("get_locator_names"), &ModelRig::get_locator_names);
	ClassDB::bind_method(D_METHOD("attach_scene", "target", "scene"), &ModelRig::attach_scene);
	ClassDB::bind_method(D_METHOD("attach_node", "target", "node"), &ModelRig::attach_node);
	ClassDB::bind_method(D_METHOD("detach_attachments", "target"), &ModelRig::detach_attachments, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("advance", "delta"), &ModelRig::advance);
	ClassDB::bind_method(D_METHOD("rebuild"), &ModelRig::rebuild);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "model", PROPERTY_HINT_RESOURCE_TYPE, "RigModelData"), "set_model", "get_model");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "animations", PROPERTY_HINT_TYPE_STRING, String::num_int64(Variant::OBJECT) + "/" + String::num_int64(PROPERTY_HINT_RESOURCE_TYPE) + ":RigAnimationData"), "set_animations", "get_animations");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "controllers", PROPERTY_HINT_TYPE_STRING, String::num_int64(Variant::OBJECT) + "/" + String::num_int64(PROPERTY_HINT_RESOURCE_TYPE) + ":RigControllerData"), "set_controllers", "get_controllers");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "double_sided"), "set_double_sided", "is_double_sided");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha_cutoff", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_alpha_cutoff", "get_alpha_cutoff");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material_override", "get_material_override");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "autoplay_animation"), "set_autoplay_animation", "get_autoplay_animation");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "0,4,0.01,or_greater"), "set_speed_scale", "get_speed_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadows", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows", "get_cast_shadows");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visual_layers", PROPERTY_HINT_LAYERS_3D_RENDER), "set_visual_layers", "get_visual_layers");

	ADD_SIGNAL(MethodInfo("animation_finished", PropertyInfo(Variant::STRING_NAME, "animation")));
	ADD_SIGNAL(MethodInfo("controller_state_changed", PropertyInfo(Variant::STRING_NAME, "controller"), PropertyInfo(Variant::STRING_NAME, "state")));
}
