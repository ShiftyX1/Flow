#pragma once

#include "rig_expression.h"

#include "core/io/resource.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

// Keyframe animations for a cube rig, parsed from a Bedrock-style `*.anim.json` /
// `*.animation.json` file (as exported by Blockbench).
//
//   { "format_version": "1.8.0",
//     "animations": {
//       "animation.player.walk": {
//         "loop": true, "animation_length": 1.0,
//         "bones": { "leg_l": { "rotation": { "0.0": [0,0,0], "0.5": [30,0,0] } } } } } }
//
// Channel values may be numbers or expressions (see RigExpression), keyframes may use
// "pre"/"post" and "lerp_mode": "catmullrom". Values are relative to the bone rest pose:
// rotations and positions are added, scale multiplies.
//
// Deviations from Bedrock: a non-looping clip holds its last pose after finishing
// (both `false` and "hold_on_last_frame" behave the same); sound/particle events are ignored.
class RigAnimationData : public Resource {
	GDCLASS(RigAnimationData, Resource);

public:
	enum LoopMode {
		LOOP_ONCE,
		LOOP_HOLD,
		LOOP_REPEAT,
	};

	struct Keyframe {
		float time = 0.0f;
		RigExpression pre[3];
		RigExpression post[3];
		bool smooth = false;
	};

	struct Channel {
		Vector<Keyframe> keys;

		bool is_empty() const { return keys.is_empty(); }
		Vector3 sample(float p_time, RigExpressionContext &p_ctx) const;
	};

	struct BoneTrack {
		StringName bone;
		Channel rotation;
		Channel position;
		Channel scale;
	};

	struct Clip {
		StringName name;
		LoopMode loop = LOOP_ONCE;
		float length = 0.0f;
		Vector<BoneTrack> tracks;
	};

private:
	HashMap<StringName, Clip> clips;
	Vector<StringName> clip_names;
	uint32_t revision = 0;

	static void _parse_channel(const Variant &p_value, Channel &r_channel, const String &p_where);

protected:
	static void _bind_methods();

public:
	Error parse_json(const String &p_json_text);
	Error load_from_file(const String &p_path);

	// Exact name first, then any clip whose name ends with ".<p_name>".
	const Clip *find_clip(const StringName &p_name) const;
	StringName resolve_name(const StringName &p_name) const;
	uint32_t get_revision() const { return revision; }

	bool has_animation(const StringName &p_name) const { return find_clip(p_name) != nullptr; }
	PackedStringArray get_animation_names() const;
	float get_animation_length(const StringName &p_name) const;
	LoopMode get_animation_loop_mode(const StringName &p_name) const;
};

VARIANT_ENUM_CAST(RigAnimationData::LoopMode);
