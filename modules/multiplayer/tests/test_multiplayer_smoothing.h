/**************************************************************************/
/*  test_multiplayer_smoothing.h                                          */
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

#include "../multiplayer_predicted_body_3d.h"
#include "../multiplayer_simulation_clock.h"
#include "../multiplayer_snapshot_buffer_3d.h"

#include "tests/test_macros.h"

namespace TestMultiplayerSmoothing {

class TestPredictedBody3D : public MultiplayerPredictedBody3D {
	GDCLASS(TestPredictedBody3D, MultiplayerPredictedBody3D);

	Dictionary state;

protected:
	static void _bind_methods() {}

public:
	Dictionary test_get_state() {
		return state;
	}

	void test_set_state(const Dictionary &p_state) {
		state = p_state;
	}

	Dictionary test_collect_input(int64_t p_tick) {
		Dictionary input;
		input["move"] = Vector3(1, 0, 0);
		input["tick"] = p_tick;
		return input;
	}

	Dictionary test_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta) {
		Dictionary next = p_state;
		Vector3 position;
		if (next.has("position")) {
			position = next["position"];
		}
		Vector3 move;
		if (p_input.has("move")) {
			move = p_input["move"];
		}
		position += move * p_delta;
		next["position"] = position;
		return next;
	}

	Dictionary _network_get_state() override {
		return test_get_state();
	}

	void _network_set_state(const Dictionary &p_state) override {
		test_set_state(p_state);
	}

	Dictionary _network_collect_input(int64_t p_tick) override {
		return test_collect_input(p_tick);
	}

	Dictionary _network_simulate(const Dictionary &p_state, const Dictionary &p_input, double p_delta) override {
		return test_simulate(p_state, p_input, p_delta);
	}
};

TEST_CASE("[Multiplayer][Smoothing] Simulation clock exposes delayed render ticks") {
	Ref<MultiplayerSimulationClock> clock;
	clock.instantiate();
	clock->set_tick_rate(30);
	clock->set_interpolation_delay_ticks(2);

	CHECK_EQ(clock->advance(1.0 / 30.0), 1);
	CHECK_EQ(clock->get_local_tick(), 1);
	CHECK_EQ(clock->get_render_tick(), 0);

	clock->observe_server_tick(12);
	CHECK_EQ(clock->get_observed_server_tick(), 12);
	CHECK_EQ(clock->get_render_tick(), 10);
	CHECK_EQ(clock->get_render_fraction(), doctest::Approx(0.0));

	clock->advance(1.0 / 60.0);
	CHECK_EQ(clock->get_render_fraction(), doctest::Approx(0.5));
	CHECK_EQ(clock->get_render_tick_time(), doctest::Approx(10.5));

	clock->observe_server_tick(12);
	clock->observe_server_tick(11);
	CHECK_EQ(clock->get_render_fraction(), doctest::Approx(0.5));
	CHECK_EQ(clock->get_render_tick_time(), doctest::Approx(10.5));

	clock->observe_server_tick(13);
	CHECK_EQ(clock->get_render_fraction(), doctest::Approx(0.0));
	CHECK_EQ(clock->get_render_tick_time(), doctest::Approx(11.0));
}

TEST_CASE("[Multiplayer][Smoothing] Snapshot buffer samples midpoint interpolation") {
	Ref<MultiplayerSnapshotBuffer3D> buffer;
	buffer.instantiate();

	const int64_t entity_id = 42;
	buffer->push_transform(entity_id, 10, Transform3D(Basis(), Vector3(0, 0, 0)));
	buffer->push_transform(entity_id, 12, Transform3D(Basis(Vector3(0, 1, 0), Math::PI / 2.0), Vector3(10, 0, 0)));

	Dictionary sample = buffer->sample_transform(entity_id, 11.0);
	CHECK(bool(sample["ok"]));
	CHECK_EQ(StringName(sample["mode"]), SNAME("interpolated"));

	Transform3D transform = sample["transform"];
	CHECK(transform.origin.is_equal_approx(Vector3(5, 0, 0)));
	CHECK(transform.basis.get_euler().y == doctest::Approx(Math::PI / 4.0));
}

TEST_CASE("[Multiplayer][Smoothing] Snapshot buffer snaps across teleports") {
	Ref<MultiplayerSnapshotBuffer3D> buffer;
	buffer.instantiate();

	const int64_t entity_id = 42;
	buffer->push_transform(entity_id, 10, Transform3D(Basis(), Vector3(1, 0, 0)));
	buffer->push_transform(entity_id, 11, Transform3D(Basis(), Vector3(100, 0, 0)), Vector3(), true);

	Dictionary before_teleport = buffer->sample_transform(entity_id, 10.5);
	Transform3D before_transform = before_teleport["transform"];
	CHECK_EQ(StringName(before_teleport["mode"]), SNAME("held"));
	CHECK(before_transform.origin.is_equal_approx(Vector3(1, 0, 0)));

	Dictionary after_teleport = buffer->sample_transform(entity_id, 11.0);
	Transform3D after_transform = after_teleport["transform"];
	CHECK(after_transform.origin.is_equal_approx(Vector3(100, 0, 0)));
}

TEST_CASE("[Multiplayer][Smoothing] Snapshot buffer rejects stale snapshots") {
	Ref<MultiplayerSnapshotBuffer3D> buffer;
	buffer.instantiate();

	const int64_t entity_id = 42;
	CHECK(buffer->push_transform(entity_id, 10, Transform3D(Basis(), Vector3(0, 0, 0))));
	CHECK_FALSE(buffer->push_transform(entity_id, 9, Transform3D(Basis(), Vector3(100, 0, 0))));
	CHECK_FALSE(buffer->push_transform(entity_id, 10, Transform3D(Basis(), Vector3(200, 0, 0))));
	CHECK_EQ(buffer->get_snapshot_count(entity_id), 1);
	CHECK_EQ(buffer->get_latest_tick(entity_id), 10);
}

TEST_CASE("[Multiplayer][Smoothing] Snapshot buffer caps extrapolation") {
	Ref<MultiplayerSnapshotBuffer3D> buffer;
	buffer.instantiate();
	buffer->set_tick_rate(10);
	buffer->set_max_extrapolation_ticks(2);

	const int64_t entity_id = 42;
	buffer->push_transform(entity_id, 10, Transform3D(Basis(), Vector3(0, 0, 0)), Vector3(10, 0, 0));

	Dictionary sample = buffer->sample_transform(entity_id, 20.0);
	CHECK_EQ(StringName(sample["mode"]), SNAME("extrapolated"));

	Transform3D transform = sample["transform"];
	CHECK(transform.origin.is_equal_approx(Vector3(2, 0, 0)));
	CHECK(double(sample["ticks_ahead"]) == doctest::Approx(2.0));
}

TEST_CASE("[Multiplayer][Smoothing] Predicted body replays input after authoritative correction") {
	TestPredictedBody3D *body = memnew(TestPredictedBody3D);
	body->set_tick_rate(10);
	body->set_correction_snap_threshold(10.0);

	Dictionary initial;
	initial["position"] = Vector3();
	body->reset_prediction(initial);

	body->predict_tick(1);
	body->predict_tick(2);
	body->predict_tick(3);

	Dictionary authoritative;
	authoritative["position"] = Vector3(0.05, 0, 0);
	Dictionary result = body->push_authoritative_state(20, 1, authoritative);

	CHECK(bool(result["ok"]));
	CHECK_FALSE(bool(result["snapped"]));
	CHECK_EQ(body->get_pending_input_count(), 2);

	Dictionary corrected = body->test_get_state();
	Vector3 corrected_position = corrected["position"];
	CHECK(corrected_position.is_equal_approx(Vector3(0.25, 0, 0)));
	CHECK(body->get_render_correction_offset().is_equal_approx(Vector3(0.05, 0, 0)));

	memdelete(body);
}

TEST_CASE("[Multiplayer][Smoothing] Predicted body snaps on large errors") {
	TestPredictedBody3D *body = memnew(TestPredictedBody3D);
	body->set_tick_rate(10);
	body->set_correction_snap_threshold(0.25);

	Dictionary initial;
	initial["position"] = Vector3();
	body->reset_prediction(initial);
	body->predict_tick(1);

	Dictionary authoritative;
	authoritative["position"] = Vector3(10, 0, 0);
	Dictionary result = body->push_authoritative_state(20, 1, authoritative);

	CHECK(bool(result["snapped"]));
	CHECK(body->get_render_correction_offset().is_zero_approx());

	Dictionary corrected = body->test_get_state();
	Vector3 corrected_position = corrected["position"];
	CHECK(corrected_position.is_equal_approx(Vector3(10, 0, 0)));

	memdelete(body);
}

} // namespace TestMultiplayerSmoothing
