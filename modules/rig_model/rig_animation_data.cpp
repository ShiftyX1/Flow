#include "rig_animation_data.h"

#include "core/object/class_db.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/variant/typed_array.h"

namespace {

void set_component(RigExpression &r_expr, const Variant &p_value, const String &p_where) {
	switch (p_value.get_type()) {
		case Variant::FLOAT:
		case Variant::INT:
			r_expr.set_constant((float)(double)p_value);
			break;
		case Variant::STRING: {
			const String text = p_value;
			if (r_expr.parse(text) != OK) {
				WARN_PRINT(vformat("RigAnimationData: bad expression '%s' in %s: %s", text, p_where, r_expr.get_error()));
			}
			break;
		}
		default:
			r_expr.set_constant(0.0f);
			break;
	}
}

// Reads "[x, y, z]" or a scalar (replicated to all three components).
bool read_vector(const Variant &p_value, RigExpression r_out[3], const String &p_where) {
	if (p_value.get_type() == Variant::ARRAY) {
		const Array a = p_value;
		if (a.size() < 3) {
			return false;
		}
		for (int i = 0; i < 3; i++) {
			set_component(r_out[i], a[i], p_where);
		}
		return true;
	}
	if (p_value.get_type() == Variant::FLOAT || p_value.get_type() == Variant::INT || p_value.get_type() == Variant::STRING) {
		for (int i = 0; i < 3; i++) {
			set_component(r_out[i], p_value, p_where);
		}
		return true;
	}
	return false;
}

struct KeyframeTimeComparator {
	bool operator()(const RigAnimationData::Keyframe &p_a, const RigAnimationData::Keyframe &p_b) const {
		return p_a.time < p_b.time;
	}
};

} // namespace

Vector3 RigAnimationData::Channel::sample(float p_time, RigExpressionContext &p_ctx) const {
	const int n = keys.size();
	ERR_FAIL_COND_V(n == 0, Vector3());

	auto eval3 = [&p_ctx](const RigExpression *e) {
		return Vector3(e[0].evaluate(p_ctx), e[1].evaluate(p_ctx), e[2].evaluate(p_ctx));
	};

	if (p_time < keys[0].time) {
		return eval3(keys[0].pre);
	}
	if (n == 1 || p_time >= keys[n - 1].time) {
		return eval3(keys[n - 1].post);
	}

	// Largest index with time <= p_time.
	int lo = 0;
	int hi = n - 1;
	while (hi - lo > 1) {
		const int mid = (lo + hi) / 2;
		if (keys[mid].time <= p_time) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	const Keyframe &k0 = keys[lo];
	const Keyframe &k1 = keys[lo + 1];
	const float span = k1.time - k0.time;
	const float a = span > 0.0f ? (p_time - k0.time) / span : 1.0f;
	const Vector3 v0 = eval3(k0.post);
	const Vector3 v1 = eval3(k1.pre);
	if (k0.smooth || k1.smooth) {
		const Vector3 before = lo > 0 ? eval3(keys[lo - 1].post) : v0;
		const Vector3 after = lo + 2 < n ? eval3(keys[lo + 2].pre) : v1;
		return v0.cubic_interpolate(v1, before, after, a);
	}
	return v0.lerp(v1, a);
}

void RigAnimationData::_parse_channel(const Variant &p_value, Channel &r_channel, const String &p_where) {
	r_channel.keys.clear();

	auto parse_key_value = [&p_where](const Variant &p_entry, Keyframe &r_key) -> bool {
		if (p_entry.get_type() == Variant::DICTIONARY) {
			const Dictionary d = p_entry;
			bool has_pre = d.has("pre") && read_vector(d["pre"], r_key.pre, p_where);
			bool has_post = d.has("post") && read_vector(d["post"], r_key.post, p_where);
			if (!has_pre && !has_post && d.has("vector")) {
				has_post = read_vector(d["vector"], r_key.post, p_where);
			}
			if (!has_pre && !has_post) {
				return false;
			}
			if (!has_pre) {
				for (int i = 0; i < 3; i++) {
					r_key.pre[i] = r_key.post[i];
				}
			}
			if (!has_post) {
				for (int i = 0; i < 3; i++) {
					r_key.post[i] = r_key.pre[i];
				}
			}
			if (d.has("lerp_mode")) {
				r_key.smooth = String(d["lerp_mode"]).to_lower() == "catmullrom";
			}
			return true;
		}
		if (!read_vector(p_entry, r_key.post, p_where)) {
			return false;
		}
		for (int i = 0; i < 3; i++) {
			r_key.pre[i] = r_key.post[i];
		}
		return true;
	};

	if (p_value.get_type() == Variant::DICTIONARY) {
		const Dictionary d = p_value;
		const Array times = d.keys();
		for (int i = 0; i < times.size(); i++) {
			Keyframe key;
			key.time = String(times[i]).to_float();
			if (parse_key_value(d[times[i]], key)) {
				r_channel.keys.push_back(key);
			} else {
				WARN_PRINT(vformat("RigAnimationData: unreadable keyframe '%s' in %s.", String(times[i]), p_where));
			}
		}
		r_channel.keys.sort_custom<KeyframeTimeComparator>();
		return;
	}

	Keyframe key;
	if (parse_key_value(p_value, key)) {
		r_channel.keys.push_back(key);
	}
}

Error RigAnimationData::parse_json(const String &p_json_text) {
	clips.clear();
	clip_names.clear();
	revision++;

	Ref<JSON> json;
	json.instantiate();
	const Error err = json->parse(p_json_text);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigAnimationData: JSON error at line %d: %s", json->get_error_line(), json->get_error_message()));
	const Variant data = json->get_data();
	ERR_FAIL_COND_V_MSG(data.get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigAnimationData: root must be an object.");
	const Dictionary root = data;
	ERR_FAIL_COND_V_MSG(!root.has("animations") || root["animations"].get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigAnimationData: missing 'animations' object.");

	const Dictionary anims = root["animations"];
	const Array names = anims.keys();
	for (int ai = 0; ai < names.size(); ai++) {
		const String clip_name = names[ai];
		ERR_CONTINUE(anims[names[ai]].get_type() != Variant::DICTIONARY);
		const Dictionary cd = anims[names[ai]];

		Clip clip;
		clip.name = StringName(clip_name);
		if (cd.has("loop")) {
			const Variant loop = cd["loop"];
			if (loop.get_type() == Variant::BOOL) {
				clip.loop = (bool)loop ? LOOP_REPEAT : LOOP_ONCE;
			} else if (loop.get_type() == Variant::STRING && String(loop).to_lower() == "hold_on_last_frame") {
				clip.loop = LOOP_HOLD;
			}
		}

		float max_time = 0.0f;
		if (cd.has("bones") && cd["bones"].get_type() == Variant::DICTIONARY) {
			const Dictionary bones = cd["bones"];
			const Array bone_names = bones.keys();
			for (int bi = 0; bi < bone_names.size(); bi++) {
				ERR_CONTINUE(bones[bone_names[bi]].get_type() != Variant::DICTIONARY);
				const Dictionary bd = bones[bone_names[bi]];
				BoneTrack track;
				track.bone = StringName(String(bone_names[bi]));
				const String where = vformat("%s/%s", clip_name, String(bone_names[bi]));
				if (bd.has("rotation")) {
					_parse_channel(bd["rotation"], track.rotation, where + "/rotation");
				}
				if (bd.has("position")) {
					_parse_channel(bd["position"], track.position, where + "/position");
				}
				if (bd.has("scale")) {
					_parse_channel(bd["scale"], track.scale, where + "/scale");
				}
				const Channel *channels[3] = { &track.rotation, &track.position, &track.scale };
				for (const Channel *c : channels) {
					if (!c->keys.is_empty()) {
						max_time = MAX(max_time, c->keys[c->keys.size() - 1].time);
					}
				}
				clip.tracks.push_back(track);
			}
		}

		if (cd.has("pre_animation")) {
			Vector<String> scripts;
			if (cd["pre_animation"].get_type() == Variant::ARRAY) {
				const Array arr = cd["pre_animation"];
				for (int i = 0; i < arr.size(); i++) {
					scripts.push_back(String(arr[i]));
				}
			} else {
				scripts.push_back(String(cd["pre_animation"]));
			}
			for (int i = 0; i < scripts.size(); i++) {
				RigExpression expr;
				if (expr.parse(scripts[i]) != OK) {
					WARN_PRINT(vformat("RigAnimationData: %s/pre_animation: %s", clip_name, expr.get_error()));
				}
				clip.pre_animation.push_back(expr);
			}
		}

		clip.length = cd.has("animation_length") ? (float)(double)cd["animation_length"] : max_time;
		if (clip.length <= 0.0f) {
			clip.length = max_time;
		}
		clip_names.push_back(clip.name);
		clips[clip.name] = clip;
	}
	return OK;
}

Error RigAnimationData::load_from_file(const String &p_path) {
	Error err;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigAnimationData: cannot read '%s'.", p_path));
	return parse_json(text);
}

const RigAnimationData::Clip *RigAnimationData::find_clip(const StringName &p_name) const {
	const Clip *exact = clips.getptr(p_name);
	if (exact) {
		return exact;
	}
	const String suffix = "." + String(p_name);
	for (int i = 0; i < clip_names.size(); i++) {
		if (String(clip_names[i]).ends_with(suffix)) {
			return clips.getptr(clip_names[i]);
		}
	}
	return nullptr;
}

StringName RigAnimationData::resolve_name(const StringName &p_name) const {
	const Clip *clip = find_clip(p_name);
	return clip ? clip->name : StringName();
}

PackedStringArray RigAnimationData::get_animation_names() const {
	PackedStringArray out;
	for (int i = 0; i < clip_names.size(); i++) {
		out.push_back(String(clip_names[i]));
	}
	return out;
}

float RigAnimationData::get_animation_length(const StringName &p_name) const {
	const Clip *clip = find_clip(p_name);
	ERR_FAIL_NULL_V_MSG(clip, 0.0f, vformat("RigAnimationData: unknown animation '%s'.", p_name));
	return clip->length;
}

RigAnimationData::LoopMode RigAnimationData::get_animation_loop_mode(const StringName &p_name) const {
	const Clip *clip = find_clip(p_name);
	ERR_FAIL_NULL_V_MSG(clip, LOOP_ONCE, vformat("RigAnimationData: unknown animation '%s'.", p_name));
	return clip->loop;
}

void RigAnimationData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_json", "json_text"), &RigAnimationData::parse_json);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &RigAnimationData::load_from_file);
	ClassDB::bind_method(D_METHOD("has_animation", "name"), &RigAnimationData::has_animation);
	ClassDB::bind_method(D_METHOD("get_animation_names"), &RigAnimationData::get_animation_names);
	ClassDB::bind_method(D_METHOD("get_animation_length", "name"), &RigAnimationData::get_animation_length);
	ClassDB::bind_method(D_METHOD("get_animation_loop_mode", "name"), &RigAnimationData::get_animation_loop_mode);

	BIND_ENUM_CONSTANT(LOOP_ONCE);
	BIND_ENUM_CONSTANT(LOOP_HOLD);
	BIND_ENUM_CONSTANT(LOOP_REPEAT);
}
