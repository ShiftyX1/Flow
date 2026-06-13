/**************************************************************************/
/*  multiplayer_predicted_body_3d.h                                       */
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

#pragma once

#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"

class MultiplayerPredictedBody3D : public Node {
	GDCLASS(MultiplayerPredictedBody3D, Node);

	struct InputFrame {
		int64_t tick = 0;
		Dictionary input;
		Dictionary predicted_state;
	};

	Vector<InputFrame> input_history;
	int tick_rate = 30;
	int64_t current_tick = 0;
	int max_replay_ticks = 120;
	double correction_smooth_time = 0.1;
	double correction_snap_threshold = 1.0;
	bool prediction_enabled = true;
	Vector3 render_correction_offset;

	Dictionary _call_get_state();
	void _call_set_state(const Dictionary &p_state);
	Dictionary _call_collect_input(int64_t p_tick);
	Dictionary _call_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta);

	void _push_input_frame(int64_t p_tick, const Dictionary &p_input, const Dictionary &p_predicted_state);
	int _find_input_frame(int64_t p_tick) const;
	Vector3 _get_state_position(const Dictionary &p_state, bool &r_found) const;
	void _trim_history(int64_t p_last_processed_tick);

protected:
	static void _bind_methods();

	GDVIRTUAL0R(Dictionary, _network_get_state);
	GDVIRTUAL1(_network_set_state, Dictionary);
	GDVIRTUAL1R(Dictionary, _network_collect_input, int64_t);
	GDVIRTUAL3R(Dictionary, _network_simulate, Dictionary, Dictionary, double);

	virtual Dictionary _network_get_state();
	virtual void _network_set_state(const Dictionary &p_state);
	virtual Dictionary _network_collect_input(int64_t p_tick);
	virtual Dictionary _network_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta);

public:
	void set_tick_rate(int p_tick_rate);
	int get_tick_rate() const;
	double get_tick_delta() const;

	void set_max_replay_ticks(int p_max_ticks);
	int get_max_replay_ticks() const;

	void set_correction_smooth_time(double p_time);
	double get_correction_smooth_time() const;

	void set_correction_snap_threshold(double p_threshold);
	double get_correction_snap_threshold() const;

	void set_prediction_enabled(bool p_enabled);
	bool is_prediction_enabled() const;

	void reset_prediction(const Dictionary &p_state = Dictionary());
	Dictionary predict_tick(int64_t p_tick = -1);
	void record_predicted_state(int64_t p_tick, const Dictionary &p_input, const Dictionary &p_predicted_state);
	Dictionary push_authoritative_state(int64_t p_server_tick, int64_t p_last_processed_input_tick, const Dictionary &p_state);

	Vector3 consume_render_correction(double p_delta);
	Vector3 get_render_correction_offset() const;
	int get_pending_input_count() const;
	int64_t get_current_tick() const;
};
