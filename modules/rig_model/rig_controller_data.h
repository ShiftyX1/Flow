#pragma once

#include "rig_expression.h"

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

// Animation state machines, parsed from a Bedrock-style `*.controller.json`:
//
//   { "format_version": "1.10.0",
//     "animation_controllers": {
//       "controller.animation.player.locomotion": {
//         "initial_state": "idle",
//         "states": {
//           "idle": { "animations": ["idle"],
//                     "transitions": [ { "walk": "query.is_moving" } ],
//                     "blend_transition": 0.15 },
//           "walk": { "animations": [ { "walk": "query.ground_speed / 4" } ],
//                     "transitions": [ { "idle": "!query.is_moving" } ] } } } } }
//
// Animation entries are either a clip name or { clip: blend_weight_expression }.
// Transitions are tested in order; the first one whose expression is non-zero wins.
// Besides the game-provided queries, transition expressions can read:
//   query.state_time, query.all_animations_finished, query.any_animation_finished,
//   query.anim_phase.<short_clip_name> (0..1 position inside the state's clip).
// "on_entry"/"on_exit" are optional arrays of expression scripts (e.g. variable assignments).
class RigControllerData : public Resource {
	GDCLASS(RigControllerData, Resource);

public:
	struct AnimRef {
		StringName name;
		RigExpression blend;
	};

	struct Transition {
		StringName target;
		int target_index = -1;
		RigExpression condition;
	};

	struct State {
		StringName name;
		Vector<AnimRef> animations;
		Vector<Transition> transitions;
		Vector<RigExpression> on_entry;
		Vector<RigExpression> on_exit;
		float blend_transition = 0.0f;
	};

	struct Controller {
		StringName name;
		int initial_state = 0;
		Vector<State> states;
		HashMap<StringName, int> state_lookup;
	};

private:
	Vector<Controller> controllers;
	uint32_t revision = 0;

protected:
	static void _bind_methods();

public:
	Error parse_json(const String &p_json_text);
	Error load_from_file(const String &p_path);

	int get_controller_count() const { return controllers.size(); }
	const Controller &get_controller(int p_index) const { return controllers[p_index]; }
	// Exact name first, then any controller whose name ends with ".<p_name>".
	int find_controller(const StringName &p_name) const;
	uint32_t get_revision() const { return revision; }

	PackedStringArray get_controller_names() const;
	bool has_controller(const StringName &p_name) const { return find_controller(p_name) >= 0; }
	PackedStringArray get_state_names(const StringName &p_controller) const;
};
