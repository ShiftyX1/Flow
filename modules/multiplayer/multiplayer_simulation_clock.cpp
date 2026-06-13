/**************************************************************************/
/*  multiplayer_simulation_clock.cpp                                      */
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

#include "multiplayer_simulation_clock.h"

#include "core/object/class_db.h"

void MultiplayerSimulationClock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tick_rate", "tick_rate"), &MultiplayerSimulationClock::set_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_rate"), &MultiplayerSimulationClock::get_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_delta"), &MultiplayerSimulationClock::get_tick_delta);

	ClassDB::bind_method(D_METHOD("set_interpolation_delay_ticks", "delay_ticks"), &MultiplayerSimulationClock::set_interpolation_delay_ticks);
	ClassDB::bind_method(D_METHOD("get_interpolation_delay_ticks"), &MultiplayerSimulationClock::get_interpolation_delay_ticks);

	ClassDB::bind_method(D_METHOD("reset"), &MultiplayerSimulationClock::reset);
	ClassDB::bind_method(D_METHOD("advance", "delta"), &MultiplayerSimulationClock::advance);

	ClassDB::bind_method(D_METHOD("set_local_tick", "tick"), &MultiplayerSimulationClock::set_local_tick);
	ClassDB::bind_method(D_METHOD("get_local_tick"), &MultiplayerSimulationClock::get_local_tick);

	ClassDB::bind_method(D_METHOD("observe_server_tick", "tick"), &MultiplayerSimulationClock::observe_server_tick);
	ClassDB::bind_method(D_METHOD("get_observed_server_tick"), &MultiplayerSimulationClock::get_observed_server_tick);

	ClassDB::bind_method(D_METHOD("get_render_tick"), &MultiplayerSimulationClock::get_render_tick);
	ClassDB::bind_method(D_METHOD("get_render_fraction"), &MultiplayerSimulationClock::get_render_fraction);
	ClassDB::bind_method(D_METHOD("get_render_tick_time"), &MultiplayerSimulationClock::get_render_tick_time);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_rate", PROPERTY_HINT_RANGE, "1,240,1,or_greater"), "set_tick_rate", "get_tick_rate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation_delay_ticks", PROPERTY_HINT_RANGE, "0,120,1,or_greater"), "set_interpolation_delay_ticks", "get_interpolation_delay_ticks");
}

void MultiplayerSimulationClock::set_tick_rate(int p_tick_rate) {
	ERR_FAIL_COND_MSG(p_tick_rate <= 0, "Tick rate must be greater than 0.");
	tick_rate = p_tick_rate;
	const double tick_delta = get_tick_delta();
	if (accumulator >= tick_delta) {
		accumulator = Math::fmod(accumulator, tick_delta);
	}
}

int MultiplayerSimulationClock::get_tick_rate() const {
	return tick_rate;
}

double MultiplayerSimulationClock::get_tick_delta() const {
	return 1.0 / double(tick_rate);
}

void MultiplayerSimulationClock::set_interpolation_delay_ticks(int p_delay) {
	ERR_FAIL_COND_MSG(p_delay < 0, "Interpolation delay must be non-negative.");
	interpolation_delay_ticks = p_delay;
}

int MultiplayerSimulationClock::get_interpolation_delay_ticks() const {
	return interpolation_delay_ticks;
}

void MultiplayerSimulationClock::reset() {
	local_tick = 0;
	observed_server_tick = -1;
	accumulator = 0.0;
}

int MultiplayerSimulationClock::advance(double p_delta) {
	ERR_FAIL_COND_V_MSG(p_delta < 0.0, 0, "Delta must be non-negative.");

	accumulator += p_delta;
	const double tick_delta = get_tick_delta();
	int advanced = 0;
	while (accumulator + CMP_EPSILON >= tick_delta) {
		accumulator -= tick_delta;
		local_tick++;
		advanced++;
	}
	return advanced;
}

void MultiplayerSimulationClock::set_local_tick(int64_t p_tick) {
	ERR_FAIL_COND_MSG(p_tick < 0, "Local tick must be non-negative.");
	local_tick = p_tick;
	accumulator = 0.0;
}

int64_t MultiplayerSimulationClock::get_local_tick() const {
	return local_tick;
}

void MultiplayerSimulationClock::observe_server_tick(int64_t p_tick) {
	if (p_tick > observed_server_tick) {
		observed_server_tick = p_tick;
	}
}

int64_t MultiplayerSimulationClock::get_observed_server_tick() const {
	return observed_server_tick;
}

int64_t MultiplayerSimulationClock::get_render_tick() const {
	const int64_t source_tick = observed_server_tick >= 0 ? observed_server_tick : local_tick;
	return MAX(source_tick - int64_t(interpolation_delay_ticks), int64_t(0));
}

double MultiplayerSimulationClock::get_render_fraction() const {
	if (observed_server_tick >= 0) {
		return 0.0;
	}
	return CLAMP(accumulator / get_tick_delta(), 0.0, 1.0);
}

double MultiplayerSimulationClock::get_render_tick_time() const {
	return double(get_render_tick()) + get_render_fraction();
}
