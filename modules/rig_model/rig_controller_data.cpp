#include "rig_controller_data.h"

#include "core/object/class_db.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/variant/typed_array.h"

namespace {

void parse_expression(RigExpression &r_expr, const Variant &p_value, const String &p_where) {
	if (p_value.get_type() == Variant::FLOAT || p_value.get_type() == Variant::INT) {
		r_expr.set_constant((float)(double)p_value);
		return;
	}
	if (p_value.get_type() == Variant::BOOL) {
		r_expr.set_constant((bool)p_value ? 1.0f : 0.0f);
		return;
	}
	const String text = p_value;
	if (r_expr.parse(text) != OK) {
		WARN_PRINT(vformat("RigControllerData: bad expression '%s' in %s: %s", text, p_where, r_expr.get_error()));
	}
}

void parse_scripts(Vector<RigExpression> &r_out, const Variant &p_value, const String &p_where) {
	if (p_value.get_type() != Variant::ARRAY) {
		return;
	}
	const Array a = p_value;
	for (int i = 0; i < a.size(); i++) {
		RigExpression e;
		parse_expression(e, a[i], p_where);
		r_out.push_back(e);
	}
}

} // namespace

Error RigControllerData::parse_json(const String &p_json_text) {
	controllers.clear();
	revision++;

	Ref<JSON> json;
	json.instantiate();
	const Error err = json->parse(p_json_text);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigControllerData: JSON error at line %d: %s", json->get_error_line(), json->get_error_message()));
	const Variant data = json->get_data();
	ERR_FAIL_COND_V_MSG(data.get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigControllerData: root must be an object.");
	const Dictionary root = data;
	ERR_FAIL_COND_V_MSG(!root.has("animation_controllers") || root["animation_controllers"].get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigControllerData: missing 'animation_controllers' object.");

	const Dictionary ctrls = root["animation_controllers"];
	const Array ctrl_names = ctrls.keys();
	for (int ci = 0; ci < ctrl_names.size(); ci++) {
		const String ctrl_name = ctrl_names[ci];
		ERR_CONTINUE(ctrls[ctrl_names[ci]].get_type() != Variant::DICTIONARY);
		const Dictionary cd = ctrls[ctrl_names[ci]];

		Controller ctrl;
		ctrl.name = StringName(ctrl_name);

		ERR_CONTINUE_MSG(!cd.has("states") || cd["states"].get_type() != Variant::DICTIONARY, vformat("RigControllerData: controller '%s' has no states.", ctrl_name));
		const Dictionary states = cd["states"];
		const Array state_names = states.keys();
		for (int si = 0; si < state_names.size(); si++) {
			const String state_name = state_names[si];
			ERR_CONTINUE(states[state_names[si]].get_type() != Variant::DICTIONARY);
			const Dictionary sd = states[state_names[si]];
			const String where = vformat("%s/%s", ctrl_name, state_name);

			State state;
			state.name = StringName(state_name);

			if (sd.has("animations") && sd["animations"].get_type() == Variant::ARRAY) {
				const Array anims = sd["animations"];
				for (int ai = 0; ai < anims.size(); ai++) {
					AnimRef ref;
					if (anims[ai].get_type() == Variant::DICTIONARY) {
						const Dictionary ad = anims[ai];
						const Array keys = ad.keys();
						if (keys.is_empty()) {
							continue;
						}
						ref.name = StringName(String(keys[0]));
						parse_expression(ref.blend, ad[keys[0]], where + "/animations");
					} else {
						ref.name = StringName(String(anims[ai]));
						ref.blend.set_constant(1.0f);
					}
					state.animations.push_back(ref);
				}
			}

			if (sd.has("transitions") && sd["transitions"].get_type() == Variant::ARRAY) {
				const Array trans = sd["transitions"];
				for (int ti = 0; ti < trans.size(); ti++) {
					ERR_CONTINUE(trans[ti].get_type() != Variant::DICTIONARY);
					const Dictionary td = trans[ti];
					const Array keys = td.keys();
					for (int ki = 0; ki < keys.size(); ki++) {
						Transition t;
						t.target = StringName(String(keys[ki]));
						parse_expression(t.condition, td[keys[ki]], where + "/transitions");
						state.transitions.push_back(t);
					}
				}
			}

			parse_scripts(state.on_entry, sd.get("on_entry", Variant()), where + "/on_entry");
			parse_scripts(state.on_exit, sd.get("on_exit", Variant()), where + "/on_exit");

			if (sd.has("blend_transition")) {
				const Variant bt = sd["blend_transition"];
				if (bt.get_type() == Variant::FLOAT || bt.get_type() == Variant::INT) {
					state.blend_transition = MAX(0.0f, (float)(double)bt);
				} else if (bt.get_type() == Variant::DICTIONARY) {
					// Bedrock allows a blend curve; use its duration (largest key).
					const Dictionary curve = bt;
					const Array keys = curve.keys();
					for (int ki = 0; ki < keys.size(); ki++) {
						state.blend_transition = MAX(state.blend_transition, String(keys[ki]).to_float());
					}
				}
			}

			ctrl.state_lookup[state.name] = ctrl.states.size();
			ctrl.states.push_back(state);
		}
		ERR_CONTINUE_MSG(ctrl.states.is_empty(), vformat("RigControllerData: controller '%s' has no valid states.", ctrl_name));

		const StringName wanted_initial = cd.has("initial_state") ? StringName(String(cd["initial_state"])) : StringName("default");
		const int *initial = ctrl.state_lookup.getptr(wanted_initial);
		ctrl.initial_state = initial ? *initial : 0;

		for (int si = 0; si < ctrl.states.size(); si++) {
			for (int ti = 0; ti < ctrl.states[si].transitions.size(); ti++) {
				Transition &t = ctrl.states.write[si].transitions.write[ti];
				const int *target = ctrl.state_lookup.getptr(t.target);
				if (!target) {
					WARN_PRINT(vformat("RigControllerData: transition to unknown state '%s' in %s/%s.", t.target, ctrl_name, ctrl.states[si].name));
					continue;
				}
				t.target_index = *target;
			}
		}
		controllers.push_back(ctrl);
	}
	return OK;
}

Error RigControllerData::load_from_file(const String &p_path) {
	Error err;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigControllerData: cannot read '%s'.", p_path));
	return parse_json(text);
}

int RigControllerData::find_controller(const StringName &p_name) const {
	for (int i = 0; i < controllers.size(); i++) {
		if (controllers[i].name == p_name) {
			return i;
		}
	}
	const String suffix = "." + String(p_name);
	for (int i = 0; i < controllers.size(); i++) {
		if (String(controllers[i].name).ends_with(suffix)) {
			return i;
		}
	}
	return -1;
}

PackedStringArray RigControllerData::get_controller_names() const {
	PackedStringArray out;
	for (int i = 0; i < controllers.size(); i++) {
		out.push_back(String(controllers[i].name));
	}
	return out;
}

PackedStringArray RigControllerData::get_state_names(const StringName &p_controller) const {
	PackedStringArray out;
	const int ci = find_controller(p_controller);
	ERR_FAIL_COND_V_MSG(ci < 0, out, vformat("RigControllerData: unknown controller '%s'.", p_controller));
	for (int i = 0; i < controllers[ci].states.size(); i++) {
		out.push_back(String(controllers[ci].states[i].name));
	}
	return out;
}

void RigControllerData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_json", "json_text"), &RigControllerData::parse_json);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &RigControllerData::load_from_file);
	ClassDB::bind_method(D_METHOD("get_controller_names"), &RigControllerData::get_controller_names);
	ClassDB::bind_method(D_METHOD("has_controller", "name"), &RigControllerData::has_controller);
	ClassDB::bind_method(D_METHOD("get_state_names", "controller"), &RigControllerData::get_state_names);
}
