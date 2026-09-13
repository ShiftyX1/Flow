/**************************************************************************/
/*  multiplayer_snapshot_buffer_3d.h                                      */
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

#include "core/math/transform_3d.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

class MultiplayerSnapshotBuffer3D : public RefCounted {
	GDCLASS(MultiplayerSnapshotBuffer3D, RefCounted);

	struct Snapshot {
		int64_t tick = 0;
		Transform3D transform;
		Vector3 linear_velocity;
		bool teleport = false;
	};

	HashMap<int64_t, Vector<Snapshot>> snapshots;
	int max_snapshots = 32;
	int max_extrapolation_ticks = 2;
	int tick_rate = 30;

	Dictionary _sample_empty() const;
	Dictionary _sample_snapshot(const Snapshot &p_snapshot, const StringName &p_mode) const;
	Vector<Snapshot> *_get_entity_snapshots(int64_t p_entity_id);
	const Vector<Snapshot> *_get_entity_snapshots(int64_t p_entity_id) const;

protected:
	static void _bind_methods();

public:
	void set_tick_rate(int p_tick_rate);
	int get_tick_rate() const;
	double get_tick_delta() const;

	void set_max_snapshots(int p_max_snapshots);
	int get_max_snapshots() const;

	void set_max_extrapolation_ticks(int p_max_ticks);
	int get_max_extrapolation_ticks() const;

	void clear(int64_t p_entity_id = -1);
	bool push_transform(int64_t p_entity_id, int64_t p_tick, const Transform3D &p_transform, const Vector3 &p_linear_velocity = Vector3(), bool p_teleport = false);
	Dictionary sample_transform(int64_t p_entity_id, double p_render_tick) const;

	int get_snapshot_count(int64_t p_entity_id) const;
	int64_t get_latest_tick(int64_t p_entity_id) const;
};
