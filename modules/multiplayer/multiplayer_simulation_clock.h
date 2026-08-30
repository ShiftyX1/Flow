/**************************************************************************/
/*  multiplayer_simulation_clock.h                                        */
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

#include "core/object/ref_counted.h"

class MultiplayerSimulationClock : public RefCounted {
	GDCLASS(MultiplayerSimulationClock, RefCounted);

	int tick_rate = 30;
	int interpolation_delay_ticks = 2;
	int64_t local_tick = 0;
	int64_t observed_server_tick = -1;
	double accumulator = 0.0;
	double observed_tick_elapsed = 0.0;

protected:
	static void _bind_methods();

public:
	void set_tick_rate(int p_tick_rate);
	int get_tick_rate() const;

	double get_tick_delta() const;

	void set_interpolation_delay_ticks(int p_delay);
	int get_interpolation_delay_ticks() const;

	void reset();
	int advance(double p_delta);

	void set_local_tick(int64_t p_tick);
	int64_t get_local_tick() const;

	void observe_server_tick(int64_t p_tick);
	int64_t get_observed_server_tick() const;

	int64_t get_render_tick() const;
	double get_render_fraction() const;
	double get_render_tick_time() const;
};
