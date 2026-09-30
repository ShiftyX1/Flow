#pragma once

#include "rig_animation_data.h"
#include "rig_controller_data.h"
#include "rig_expression.h"
#include "rig_model_data.h"

#include "core/variant/typed_array.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

// Runtime for a data-driven cube rig.
//
// ModelRig builds a hierarchy of Node3D bones (one MeshInstance3D per bone that has cubes) from a
// RigModelData, then every frame blends the active animation tracks into bone transforms.
//
// Animation sources:
//   * RigControllerData state machines (all controllers of all assigned resources run),
//   * manual play_animation() tracks,
//   * both may be active at the same time; bone values are summed with their weights
//     (Bedrock additive semantics), so a "look at" clip written with
//     `query.head_pitch` / `query.head_yaw` expressions works as a procedural layer.
//
// Gameplay code only feeds queries: set_query("ground_speed", v) -> expression `query.ground_speed`.
class ModelRig : public Node3D {
	GDCLASS(ModelRig, Node3D);

private:
	struct BoneRT {
		Node3D *node = nullptr;
		MeshInstance3D *mesh = nullptr;
		Vector3 rest_position;
		Vector3 rest_rotation; // degrees, Bedrock convention.
	};

	enum FadePhase {
		FADE_IN,
		FADE_STEADY,
		FADE_OUT,
	};

	struct Track {
		Ref<RigAnimationData> data;
		StringName clip;
		StringName phase_key;
		int owner = -1; // controller index, -1 for manual tracks.
		float time = 0.0f;
		float weight = 1.0f;
		float speed = 1.0f;
		bool finished = false;
		bool finished_reported = false;
		FadePhase phase = FADE_STEADY;
		float fade = 1.0f;
		float fade_time = 0.0f;
		float fade_duration = 0.0f;
		float fade_start = 1.0f;
		RigExpression blend;
		bool has_blend = false;
		Vector<int> bone_idx;
		uint32_t data_revision = 0;
		uint32_t generation = 0;
	};

	struct ControllerRT {
		Ref<RigControllerData> data;
		int index = 0;
		StringName name;
		int state = -1;
		float state_time = 0.0f;
		bool enabled = true;
		uint32_t revision = 0;
	};

	struct Attachment {
		StringName target;
		ObjectID node;
	};

	struct StateEvent {
		StringName controller;
		StringName state;
	};

	Ref<RigModelData> model;
	Vector<Ref<RigAnimationData>> animation_sets;
	Vector<Ref<RigControllerData>> controller_sets;
	Ref<Texture2D> texture;
	Ref<Material> material_override;
	HashMap<String, Ref<StandardMaterial3D>> auto_materials; // keyed by texture path ("" = default).
	bool double_sided = false;
	float alpha_cutoff = 0.5f;
	StringName autoplay_animation;
	bool active = true;
	float speed_scale = 1.0f;
	int cast_shadows = 1; // GeometryInstance3D::SHADOW_CASTING_SETTING_ON
	uint32_t visual_layers = 1;

	Node3D *rig_root = nullptr;
	Vector<BoneRT> bones;
	HashMap<StringName, Node3D *> locators;
	Vector<Attachment> attachments;
	uint32_t generation = 1;

	RigExpressionContext ctx;
	HashMap<StringName, StringName> query_key_cache;
	float life_time = 0.0f;

	Vector<Track> tracks;
	Vector<ControllerRT> controllers;
	Vector<StateEvent> pending_state_events;
	Vector<StringName> pending_finished;

	Vector<Vector3> acc_rotation;
	Vector<Vector3> acc_position;
	Vector<Vector3> acc_scale;
	Vector<uint8_t> touched;
	Vector<uint8_t> prev_touched;

	bool building = false;

	void _clear_rig();
	void _rebuild();
	Ref<Material> _get_bone_material(int p_bone);
	void _apply_render_settings();
	void _restart_animation_state();
	void _start_controllers();
	void _apply_autoplay();

	bool _find_clip(const StringName &p_name, Ref<RigAnimationData> &r_data, StringName &r_clip) const;
	void _resolve_track(Track &r_track, const RigAnimationData::Clip &p_clip);
	void _begin_fade_out(Track &r_track, float p_duration);
	void _enter_state(int p_controller, int p_state, bool p_initial);
	void _run_scripts(const Vector<RigExpression> &p_scripts);
	void _step_controllers(float p_delta);
	void _sample_pose();
	StringName _query_key(const StringName &p_name);
	Node3D *_find_attach_target(const StringName &p_target) const;
	void _flush_events();

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	// Resources.
	void set_model(const Ref<RigModelData> &p_model);
	Ref<RigModelData> get_model() const { return model; }
	void set_animations(const TypedArray<RigAnimationData> &p_animations);
	TypedArray<RigAnimationData> get_animations() const;
	void set_controllers(const TypedArray<RigControllerData> &p_controllers);
	TypedArray<RigControllerData> get_controllers() const;
	void set_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_texture() const { return texture; }
	void set_double_sided(bool p_double_sided);
	bool is_double_sided() const { return double_sided; }
	void set_alpha_cutoff(float p_cutoff);
	float get_alpha_cutoff() const { return alpha_cutoff; }
	void set_material_override(const Ref<Material> &p_material);
	Ref<Material> get_material_override() const { return material_override; }
	void set_autoplay_animation(const StringName &p_name);
	StringName get_autoplay_animation() const { return autoplay_animation; }

	// Runtime state.
	void set_active(bool p_active) { active = p_active; }
	bool is_active() const { return active; }
	void set_speed_scale(float p_scale) { speed_scale = MAX(0.0f, p_scale); }
	float get_speed_scale() const { return speed_scale; }
	void set_cast_shadows(int p_mode);
	int get_cast_shadows() const { return cast_shadows; }
	void set_visual_layers(uint32_t p_layers);
	uint32_t get_visual_layers() const { return visual_layers; }

	// Queries / variables (inputs of expressions).
	void set_query(const StringName &p_name, float p_value);
	void set_queries(const Dictionary &p_values);
	float get_query(const StringName &p_name);
	void set_variable(const StringName &p_name, float p_value);
	float get_variable(const StringName &p_name) const;

	// Manual animation control.
	bool play_animation(const StringName &p_name, float p_blend_in = 0.0f, float p_weight = 1.0f, float p_speed = 1.0f);
	void stop_animation(const StringName &p_name = StringName(), float p_blend_out = 0.0f);
	bool is_animation_playing(const StringName &p_name) const;
	bool has_animation(const StringName &p_name) const;
	PackedStringArray get_animation_names() const;

	// Controllers.
	StringName get_controller_state(const StringName &p_controller) const;
	bool set_controller_state(const StringName &p_controller, const StringName &p_state);
	void set_controller_enabled(const StringName &p_controller, bool p_enabled);
	PackedStringArray get_controller_names() const;

	// Bones, locators and attachments.
	Node3D *get_bone(const StringName &p_name) const;
	Node3D *get_locator(const StringName &p_name) const;
	PackedStringArray get_bone_names() const;
	PackedStringArray get_locator_names() const;
	Node *attach_scene(const StringName &p_target, const Ref<PackedScene> &p_scene);
	bool attach_node(const StringName &p_target, Node *p_node);
	void detach_attachments(const StringName &p_target = StringName());

	// Advances animation by p_delta seconds (called automatically while active).
	void advance(float p_delta);
	void rebuild() { _rebuild(); }

	ModelRig();
};
