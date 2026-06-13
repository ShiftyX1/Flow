/**************************************************************************/
/*  multiplayer_predicted_body_3d.cpp                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "multiplayer_predicted_body_3d.h"

#include "core/object/class_db.h"

Dictionary MultiplayerPredictedBody3D::_call_get_state() {
	Dictionary state;
	if (GDVIRTUAL_CALL(_network_get_state, state)) {
		return state;
	}
	return _network_get_state();
}

void MultiplayerPredictedBody3D::_call_set_state(const Dictionary &p_state) {
	if (!GDVIRTUAL_CALL(_network_set_state, p_state)) {
		_network_set_state(p_state);
	}
}

Dictionary MultiplayerPredictedBody3D::_call_collect_input(int64_t p_tick) {
	Dictionary input;
	if (GDVIRTUAL_CALL(_network_collect_input, p_tick, input)) {
		return input;
	}
	return _network_collect_input(p_tick);
}

Dictionary MultiplayerPredictedBody3D::_call_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta) {
	Dictionary simulated_state;
	if (GDVIRTUAL_CALL(_network_simulate, p_state, p_input, p_delta, simulated_state)) {
		return simulated_state;
	}
	return _network_simulate(p_state, p_input, p_delta);
}

void MultiplayerPredictedBody3D::_push_input_frame(int64_t p_tick, const Dictionary &p_input, const Dictionary &p_predicted_state) {
	for (int i = 0; i < input_history.size(); i++) {
		if (input_history[i].tick == p_tick) {
			input_history.write[i].input = p_input;
			input_history.write[i].predicted_state = p_predicted_state;
			return;
		}
	}

	InputFrame frame;
	frame.tick = p_tick;
	frame.input = p_input;
	frame.predicted_state = p_predicted_state;
	input_history.push_back(frame);

	while (input_history.size() > max_replay_ticks) {
		input_history.remove_at(0);
	}
}

int MultiplayerPredictedBody3D::_find_input_frame(int64_t p_tick) const {
	for (int i = 0; i < input_history.size(); i++) {
		if (input_history[i].tick == p_tick) {
			return i;
		}
	}
	return -1;
}

Vector3 MultiplayerPredictedBody3D::_get_state_position(const Dictionary &p_state, bool &r_found) const {
	r_found = false;
	if (!p_state.has("position")) {
		return Vector3();
	}

	Variant position = p_state["position"];
	if (position.get_type() != Variant::VECTOR3) {
		return Vector3();
	}

	r_found = true;
	return position;
}

void MultiplayerPredictedBody3D::_trim_history(int64_t p_last_processed_tick) {
	while (!input_history.is_empty() && input_history[0].tick <= p_last_processed_tick) {
		input_history.remove_at(0);
	}
	while (input_history.size() > max_replay_ticks) {
		input_history.remove_at(0);
	}
}

void MultiplayerPredictedBody3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tick_rate", "tick_rate"), &MultiplayerPredictedBody3D::set_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_rate"), &MultiplayerPredictedBody3D::get_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_delta"), &MultiplayerPredictedBody3D::get_tick_delta);

	ClassDB::bind_method(D_METHOD("set_max_replay_ticks", "max_ticks"), &MultiplayerPredictedBody3D::set_max_replay_ticks);
	ClassDB::bind_method(D_METHOD("get_max_replay_ticks"), &MultiplayerPredictedBody3D::get_max_replay_ticks);

	ClassDB::bind_method(D_METHOD("set_correction_smooth_time", "time"), &MultiplayerPredictedBody3D::set_correction_smooth_time);
	ClassDB::bind_method(D_METHOD("get_correction_smooth_time"), &MultiplayerPredictedBody3D::get_correction_smooth_time);

	ClassDB::bind_method(D_METHOD("set_correction_snap_threshold", "threshold"), &MultiplayerPredictedBody3D::set_correction_snap_threshold);
	ClassDB::bind_method(D_METHOD("get_correction_snap_threshold"), &MultiplayerPredictedBody3D::get_correction_snap_threshold);

	ClassDB::bind_method(D_METHOD("set_prediction_enabled", "enabled"), &MultiplayerPredictedBody3D::set_prediction_enabled);
	ClassDB::bind_method(D_METHOD("is_prediction_enabled"), &MultiplayerPredictedBody3D::is_prediction_enabled);

	ClassDB::bind_method(D_METHOD("reset_prediction", "state"), &MultiplayerPredictedBody3D::reset_prediction, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("predict_tick", "tick"), &MultiplayerPredictedBody3D::predict_tick, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("record_predicted_state", "tick", "input", "predicted_state"), &MultiplayerPredictedBody3D::record_predicted_state);
	ClassDB::bind_method(D_METHOD("push_authoritative_state", "server_tick", "last_processed_input_tick", "state"), &MultiplayerPredictedBody3D::push_authoritative_state);
	ClassDB::bind_method(D_METHOD("consume_render_correction", "delta"), &MultiplayerPredictedBody3D::consume_render_correction);
	ClassDB::bind_method(D_METHOD("get_render_correction_offset"), &MultiplayerPredictedBody3D::get_render_correction_offset);
	ClassDB::bind_method(D_METHOD("get_pending_input_count"), &MultiplayerPredictedBody3D::get_pending_input_count);
	ClassDB::bind_method(D_METHOD("get_current_tick"), &MultiplayerPredictedBody3D::get_current_tick);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_rate", PROPERTY_HINT_RANGE, "1,240,1,or_greater"), "set_tick_rate", "get_tick_rate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_replay_ticks", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), "set_max_replay_ticks", "get_max_replay_ticks");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "correction_smooth_time", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater,suffix:s"), "set_correction_smooth_time", "get_correction_smooth_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "correction_snap_threshold", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater,suffix:m"), "set_correction_snap_threshold", "get_correction_snap_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "prediction_enabled"), "set_prediction_enabled", "is_prediction_enabled");

	GDVIRTUAL_BIND(_network_get_state);
	GDVIRTUAL_BIND(_network_set_state, "state");
	GDVIRTUAL_BIND(_network_collect_input, "tick");
	GDVIRTUAL_BIND(_network_simulate, "state", "input", "delta");
}

Dictionary MultiplayerPredictedBody3D::_network_get_state() {
	return Dictionary();
}

void MultiplayerPredictedBody3D::_network_set_state(const Dictionary &p_state) {
	(void)p_state;
}

Dictionary MultiplayerPredictedBody3D::_network_collect_input(int64_t p_tick) {
	(void)p_tick;
	return Dictionary();
}

Dictionary MultiplayerPredictedBody3D::_network_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta) {
	(void)p_input;
	(void)p_delta;
	return p_state;
}

void MultiplayerPredictedBody3D::set_tick_rate(int p_tick_rate) {
	ERR_FAIL_COND_MSG(p_tick_rate <= 0, "Tick rate must be greater than 0.");
	tick_rate = p_tick_rate;
}

int MultiplayerPredictedBody3D::get_tick_rate() const {
	return tick_rate;
}

double MultiplayerPredictedBody3D::get_tick_delta() const {
	return 1.0 / double(tick_rate);
}

void MultiplayerPredictedBody3D::set_max_replay_ticks(int p_max_ticks) {
	ERR_FAIL_COND_MSG(p_max_ticks <= 0, "Maximum replay ticks must be greater than 0.");
	max_replay_ticks = p_max_ticks;
	while (input_history.size() > max_replay_ticks) {
		input_history.remove_at(0);
	}
}

int MultiplayerPredictedBody3D::get_max_replay_ticks() const {
	return max_replay_ticks;
}

void MultiplayerPredictedBody3D::set_correction_smooth_time(double p_time) {
	ERR_FAIL_COND_MSG(p_time < 0.0, "Correction smooth time must be non-negative.");
	correction_smooth_time = p_time;
}

double MultiplayerPredictedBody3D::get_correction_smooth_time() const {
	return correction_smooth_time;
}

void MultiplayerPredictedBody3D::set_correction_snap_threshold(double p_threshold) {
	ERR_FAIL_COND_MSG(p_threshold < 0.0, "Correction snap threshold must be non-negative.");
	correction_snap_threshold = p_threshold;
}

double MultiplayerPredictedBody3D::get_correction_snap_threshold() const {
	return correction_snap_threshold;
}

void MultiplayerPredictedBody3D::set_prediction_enabled(bool p_enabled) {
	prediction_enabled = p_enabled;
}

bool MultiplayerPredictedBody3D::is_prediction_enabled() const {
	return prediction_enabled;
}

void MultiplayerPredictedBody3D::reset_prediction(const Dictionary &p_state) {
	input_history.clear();
	render_correction_offset = Vector3();
	current_tick = 0;
	if (!p_state.is_empty()) {
		_call_set_state(p_state);
	}
}

Dictionary MultiplayerPredictedBody3D::predict_tick(int64_t p_tick) {
	Dictionary result;
	if (!prediction_enabled) {
		result["ok"] = false;
		result["reason"] = StringName("disabled");
		return result;
	}

	if (p_tick < 0) {
		p_tick = current_tick + 1;
	}
	ERR_FAIL_COND_V_MSG(p_tick < 0, result, "Prediction tick must be non-negative.");

	Dictionary input = _call_collect_input(p_tick);
	Dictionary state = _call_get_state();
	Dictionary predicted_state = _call_simulate(state, input, get_tick_delta());
	_call_set_state(predicted_state);
	_push_input_frame(p_tick, input, predicted_state);
	current_tick = MAX(current_tick, p_tick);

	result["ok"] = true;
	result["tick"] = p_tick;
	result["input"] = input;
	result["state"] = predicted_state;
	return result;
}

void MultiplayerPredictedBody3D::record_predicted_state(int64_t p_tick, const Dictionary &p_input, const Dictionary &p_predicted_state) {
	ERR_FAIL_COND_MSG(p_tick < 0, "Prediction tick must be non-negative.");
	_push_input_frame(p_tick, p_input, p_predicted_state);
	current_tick = MAX(current_tick, p_tick);
}

Dictionary MultiplayerPredictedBody3D::push_authoritative_state(int64_t p_server_tick, int64_t p_last_processed_input_tick, const Dictionary &p_state) {
	Dictionary result;
	result["ok"] = true;
	result["server_tick"] = p_server_tick;
	result["last_processed_input_tick"] = p_last_processed_input_tick;

	Dictionary previous_current_state = _call_get_state();
	const int matched_frame_index = _find_input_frame(p_last_processed_input_tick);
	Dictionary matched_predicted_state;
	if (matched_frame_index >= 0) {
		matched_predicted_state = input_history[matched_frame_index].predicted_state;
	}

	bool has_auth_position = false;
	bool has_predicted_position = false;
	const Vector3 auth_position = _get_state_position(p_state, has_auth_position);
	const Vector3 predicted_position = _get_state_position(matched_predicted_state, has_predicted_position);
	const double error = has_auth_position && has_predicted_position ? auth_position.distance_to(predicted_position) : 0.0;
	const bool snap = has_auth_position && has_predicted_position && error > correction_snap_threshold;

	_call_set_state(p_state);

	for (int i = 0; i < input_history.size(); i++) {
		if (input_history[i].tick <= p_last_processed_input_tick) {
			continue;
		}

		Dictionary replay_state = _call_get_state();
		Dictionary replayed_state = _call_simulate(replay_state, input_history[i].input, get_tick_delta());
		_call_set_state(replayed_state);
		input_history.write[i].predicted_state = replayed_state;
		current_tick = MAX(current_tick, input_history[i].tick);
	}

	Dictionary corrected_current_state = _call_get_state();
	bool has_previous_position = false;
	bool has_corrected_position = false;
	const Vector3 previous_position = _get_state_position(previous_current_state, has_previous_position);
	const Vector3 corrected_position = _get_state_position(corrected_current_state, has_corrected_position);

	if (!snap && has_previous_position && has_corrected_position) {
		render_correction_offset += previous_position - corrected_position;
	} else if (snap) {
		render_correction_offset = Vector3();
	}

	_trim_history(p_last_processed_input_tick);

	result["matched_prediction"] = matched_frame_index >= 0;
	result["position_error"] = error;
	result["snapped"] = snap;
	result["pending_input_count"] = input_history.size();
	result["render_correction_offset"] = render_correction_offset;
	return result;
}

Vector3 MultiplayerPredictedBody3D::consume_render_correction(double p_delta) {
	ERR_FAIL_COND_V_MSG(p_delta < 0.0, render_correction_offset, "Delta must be non-negative.");

	const Vector3 offset = render_correction_offset;
	if (correction_smooth_time <= 0.0 || p_delta >= correction_smooth_time) {
		render_correction_offset = Vector3();
		return offset;
	}

	const double keep_fraction = CLAMP(1.0 - (p_delta / correction_smooth_time), 0.0, 1.0);
	render_correction_offset *= keep_fraction;
	return offset;
}

Vector3 MultiplayerPredictedBody3D::get_render_correction_offset() const {
	return render_correction_offset;
}

int MultiplayerPredictedBody3D::get_pending_input_count() const {
	return input_history.size();
}

int64_t MultiplayerPredictedBody3D::get_current_tick() const {
	return current_tick;
}
