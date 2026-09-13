/**************************************************************************/
/*  multiplayer_snapshot_buffer_3d.cpp                                    */
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

#include "multiplayer_snapshot_buffer_3d.h"

#include "core/math/transform_interpolator.h"
#include "core/object/class_db.h"

Dictionary MultiplayerSnapshotBuffer3D::_sample_empty() const {
	Dictionary result;
	result["ok"] = false;
	result["mode"] = StringName("empty");
	return result;
}

Dictionary MultiplayerSnapshotBuffer3D::_sample_snapshot(const Snapshot &p_snapshot, const StringName &p_mode) const {
	Dictionary result;
	result["ok"] = true;
	result["mode"] = p_mode;
	result["tick"] = p_snapshot.tick;
	result["transform"] = p_snapshot.transform;
	result["linear_velocity"] = p_snapshot.linear_velocity;
	result["teleport"] = p_snapshot.teleport;
	return result;
}

Vector<MultiplayerSnapshotBuffer3D::Snapshot> *MultiplayerSnapshotBuffer3D::_get_entity_snapshots(int64_t p_entity_id) {
	HashMap<int64_t, Vector<Snapshot>>::Iterator entity = snapshots.find(p_entity_id);
	if (!entity) {
		return nullptr;
	}
	return &entity->value;
}

const Vector<MultiplayerSnapshotBuffer3D::Snapshot> *MultiplayerSnapshotBuffer3D::_get_entity_snapshots(int64_t p_entity_id) const {
	HashMap<int64_t, Vector<Snapshot>>::ConstIterator entity = snapshots.find(p_entity_id);
	if (!entity) {
		return nullptr;
	}
	return &entity->value;
}

void MultiplayerSnapshotBuffer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tick_rate", "tick_rate"), &MultiplayerSnapshotBuffer3D::set_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_rate"), &MultiplayerSnapshotBuffer3D::get_tick_rate);
	ClassDB::bind_method(D_METHOD("get_tick_delta"), &MultiplayerSnapshotBuffer3D::get_tick_delta);

	ClassDB::bind_method(D_METHOD("set_max_snapshots", "max_snapshots"), &MultiplayerSnapshotBuffer3D::set_max_snapshots);
	ClassDB::bind_method(D_METHOD("get_max_snapshots"), &MultiplayerSnapshotBuffer3D::get_max_snapshots);

	ClassDB::bind_method(D_METHOD("set_max_extrapolation_ticks", "max_ticks"), &MultiplayerSnapshotBuffer3D::set_max_extrapolation_ticks);
	ClassDB::bind_method(D_METHOD("get_max_extrapolation_ticks"), &MultiplayerSnapshotBuffer3D::get_max_extrapolation_ticks);

	ClassDB::bind_method(D_METHOD("clear", "entity_id"), &MultiplayerSnapshotBuffer3D::clear, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("push_transform", "entity_id", "tick", "transform", "linear_velocity", "teleport"), &MultiplayerSnapshotBuffer3D::push_transform, DEFVAL(Vector3()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("sample_transform", "entity_id", "render_tick"), &MultiplayerSnapshotBuffer3D::sample_transform);
	ClassDB::bind_method(D_METHOD("get_snapshot_count", "entity_id"), &MultiplayerSnapshotBuffer3D::get_snapshot_count);
	ClassDB::bind_method(D_METHOD("get_latest_tick", "entity_id"), &MultiplayerSnapshotBuffer3D::get_latest_tick);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "tick_rate", PROPERTY_HINT_RANGE, "1,240,1,or_greater"), "set_tick_rate", "get_tick_rate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_snapshots", PROPERTY_HINT_RANGE, "2,256,1,or_greater"), "set_max_snapshots", "get_max_snapshots");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_extrapolation_ticks", PROPERTY_HINT_RANGE, "0,120,1,or_greater"), "set_max_extrapolation_ticks", "get_max_extrapolation_ticks");
}

void MultiplayerSnapshotBuffer3D::set_tick_rate(int p_tick_rate) {
	ERR_FAIL_COND_MSG(p_tick_rate <= 0, "Tick rate must be greater than 0.");
	tick_rate = p_tick_rate;
}

int MultiplayerSnapshotBuffer3D::get_tick_rate() const {
	return tick_rate;
}

double MultiplayerSnapshotBuffer3D::get_tick_delta() const {
	return 1.0 / double(tick_rate);
}

void MultiplayerSnapshotBuffer3D::set_max_snapshots(int p_max_snapshots) {
	ERR_FAIL_COND_MSG(p_max_snapshots < 2, "At least two snapshots are required for interpolation.");
	max_snapshots = p_max_snapshots;
	for (KeyValue<int64_t, Vector<Snapshot>> &E : snapshots) {
		while (E.value.size() > max_snapshots) {
			E.value.remove_at(0);
		}
	}
}

int MultiplayerSnapshotBuffer3D::get_max_snapshots() const {
	return max_snapshots;
}

void MultiplayerSnapshotBuffer3D::set_max_extrapolation_ticks(int p_max_ticks) {
	ERR_FAIL_COND_MSG(p_max_ticks < 0, "Maximum extrapolation ticks must be non-negative.");
	max_extrapolation_ticks = p_max_ticks;
}

int MultiplayerSnapshotBuffer3D::get_max_extrapolation_ticks() const {
	return max_extrapolation_ticks;
}

void MultiplayerSnapshotBuffer3D::clear(int64_t p_entity_id) {
	if (p_entity_id < 0) {
		snapshots.clear();
		return;
	}
	snapshots.erase(p_entity_id);
}

bool MultiplayerSnapshotBuffer3D::push_transform(int64_t p_entity_id, int64_t p_tick, const Transform3D &p_transform, const Vector3 &p_linear_velocity, bool p_teleport) {
	ERR_FAIL_COND_V_MSG(p_tick < 0, false, "Snapshot tick must be non-negative.");

	Vector<Snapshot> *entity_snapshots = _get_entity_snapshots(p_entity_id);
	if (!entity_snapshots) {
		Vector<Snapshot> created;
		snapshots.insert(p_entity_id, created);
		entity_snapshots = _get_entity_snapshots(p_entity_id);
	}

	if (!entity_snapshots->is_empty() && p_tick <= entity_snapshots->get(entity_snapshots->size() - 1).tick) {
		return false;
	}

	Snapshot snapshot;
	snapshot.tick = p_tick;
	snapshot.transform = p_transform;
	snapshot.linear_velocity = p_linear_velocity;
	snapshot.teleport = p_teleport;
	entity_snapshots->push_back(snapshot);

	while (entity_snapshots->size() > max_snapshots) {
		entity_snapshots->remove_at(0);
	}
	return true;
}

Dictionary MultiplayerSnapshotBuffer3D::sample_transform(int64_t p_entity_id, double p_render_tick) const {
	const Vector<Snapshot> *entity_snapshots = _get_entity_snapshots(p_entity_id);
	if (!entity_snapshots || entity_snapshots->is_empty()) {
		return _sample_empty();
	}

	const Snapshot &first = entity_snapshots->get(0);
	if (p_render_tick <= double(first.tick)) {
		return _sample_snapshot(first, SNAME("held"));
	}

	const Snapshot &latest = entity_snapshots->get(entity_snapshots->size() - 1);
	if (p_render_tick >= double(latest.tick)) {
		const double ticks_ahead = MIN(p_render_tick - double(latest.tick), double(max_extrapolation_ticks));
		if (ticks_ahead <= CMP_EPSILON || latest.linear_velocity.is_zero_approx()) {
			return _sample_snapshot(latest, SNAME("held"));
		}

		Snapshot extrapolated = latest;
		extrapolated.transform.origin += latest.linear_velocity * (ticks_ahead * get_tick_delta());
		Dictionary result = _sample_snapshot(extrapolated, SNAME("extrapolated"));
		result["source_tick"] = latest.tick;
		result["ticks_ahead"] = ticks_ahead;
		return result;
	}

	for (int i = 0; i < entity_snapshots->size() - 1; i++) {
		const Snapshot &from = entity_snapshots->get(i);
		const Snapshot &to = entity_snapshots->get(i + 1);
		if (p_render_tick < double(from.tick) || p_render_tick > double(to.tick)) {
			continue;
		}

		if (to.teleport) {
			return _sample_snapshot(from, SNAME("held"));
		}

		const double span = double(to.tick - from.tick);
		const double fraction = span > 0.0 ? CLAMP((p_render_tick - double(from.tick)) / span, 0.0, 1.0) : 0.0;
		Snapshot interpolated = from;
		TransformInterpolator::interpolate_transform_3d(from.transform, to.transform, interpolated.transform, fraction);
		interpolated.linear_velocity = from.linear_velocity.lerp(to.linear_velocity, fraction);
		interpolated.teleport = false;

		Dictionary result = _sample_snapshot(interpolated, SNAME("interpolated"));
		result["tick_from"] = from.tick;
		result["tick_to"] = to.tick;
		result["fraction"] = fraction;
		return result;
	}

	return _sample_snapshot(latest, SNAME("held"));
}

int MultiplayerSnapshotBuffer3D::get_snapshot_count(int64_t p_entity_id) const {
	const Vector<Snapshot> *entity_snapshots = _get_entity_snapshots(p_entity_id);
	return entity_snapshots ? entity_snapshots->size() : 0;
}

int64_t MultiplayerSnapshotBuffer3D::get_latest_tick(int64_t p_entity_id) const {
	const Vector<Snapshot> *entity_snapshots = _get_entity_snapshots(p_entity_id);
	if (!entity_snapshots || entity_snapshots->is_empty()) {
		return -1;
	}
	return entity_snapshots->get(entity_snapshots->size() - 1).tick;
}
