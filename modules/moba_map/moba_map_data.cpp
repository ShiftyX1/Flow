#include "moba_map_data.h"

#include "core/math/vector3i.h"
#include "core/object/class_db.h"

MobaMapData::MobaMapData() {
	cells.resize(size.x * size.y * 3);
	cells.fill(0);
	set_local_to_scene(true);
}

void MobaMapData::_changed(const Rect2i &p_region) {
	++revision;
	emit_signal(SNAME("region_changed"), p_region);
	emit_changed();
}

bool MobaMapData::contains(Vector2i p_cell) const {
	return p_cell.x >= 0 && p_cell.y >= 0 && p_cell.x < size.x && p_cell.y < size.y;
}

Vector3i MobaMapData::get_cell(Vector2i p_cell) const {
	if (!contains(p_cell)) {
		return Vector3i();
	}
	int i = (p_cell.y * size.x + p_cell.x) * 3;
	return Vector3i(cells[i], cells[i + 1], cells[i + 2]);
}

void MobaMapData::set_size(Vector2i p_size) {
	ERR_FAIL_COND(p_size.x < 1 || p_size.y < 1 || p_size.x > 1024 || p_size.y > 1024);
	if (p_size == size) {
		return;
	}
	PackedInt32Array next;
	next.resize(p_size.x * p_size.y * 3);
	next.fill(0);
	for (int z = 0; z < MIN(size.y, p_size.y); ++z) {
		for (int x = 0; x < MIN(size.x, p_size.x); ++x) {
			for (int k = 0; k < 3; ++k) {
				next.set((z * p_size.x + x) * 3 + k, cells[(z * size.x + x) * 3 + k]);
			}
		}
	}
	size = p_size;
	cells = next;
	Rect2i region(Vector2i(), size);
	_sanitize_ramps(region);
	_changed(region);
}

void MobaMapData::set_cell_size(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 1.0 || p_value > 64.0);
	if (cell_size != p_value) {
		cell_size = p_value;
		_changed(Rect2i(Vector2i(), size));
	}
}

void MobaMapData::set_height_step(real_t p_value) {
	ERR_FAIL_COND(!Math::is_finite(p_value) || p_value < 0.5 || p_value > 32.0);
	if (height_step != p_value) {
		height_step = p_value;
		_changed(Rect2i(Vector2i(), size));
	}
}

Vector2i MobaMapData::ramp_direction(int p_ramp) {
	switch (p_ramp) {
		case RAMP_NORTH: return Vector2i(0, -1);
		case RAMP_EAST: return Vector2i(1, 0);
		case RAMP_SOUTH: return Vector2i(0, 1);
		case RAMP_WEST: return Vector2i(-1, 0);
		default: return Vector2i();
	}
}

bool MobaMapData::can_place_ramp(Vector2i p_cell, int p_direction, int p_width) const {
	if (p_direction < RAMP_NORTH || p_direction > RAMP_WEST || p_width < 1 || p_width > 64) {
		return false;
	}
	Vector2i dir = ramp_direction(p_direction);
	Vector2i side(-dir.y, dir.x);
	int base_level = get_cell(p_cell).x;
	for (int i = 0; i < p_width; ++i) {
		Vector2i c = p_cell + side * (i - (p_width - 1) / 2);
		if (!contains(c) || !contains(c - dir) || !contains(c + dir)) {
			return false;
		}
		Vector3i current = get_cell(c);
		Vector3i low = get_cell(c - dir);
		Vector3i high = get_cell(c + dir);
		if (current.x != base_level || low.x != current.x || high.x != current.x + 1 || low.z != RAMP_NONE || high.z != RAMP_NONE) {
			return false;
		}
	}
	return true;
}

void MobaMapData::_sanitize_ramps(Rect2i &r_region) {
	// Only an edited cell and its neighbours can invalidate an existing connection.
	Rect2i area = r_region.grow(1).intersection(Rect2i(Vector2i(), size));
	for (int z = area.position.y; z < area.get_end().y; ++z) {
		for (int x = area.position.x; x < area.get_end().x; ++x) {
			Vector2i c(x, z);
			int ramp = get_cell(c).z;
			if (ramp && !can_place_ramp(c, ramp)) {
				cells.set((z * size.x + x) * 3 + 2, RAMP_NONE);
				r_region = r_region.merge(Rect2i(c, Vector2i(1, 1)));
			}
		}
	}
}

void MobaMapData::set_cells(const PackedInt32Array &p_cells) {
	ERR_FAIL_COND(p_cells.size() != size.x * size.y * 3);
	Rect2i region;
	bool changed = false;
	for (int i = 0; i < p_cells.size(); i += 3) {
		ERR_FAIL_COND(p_cells[i] < -128 || p_cells[i] > 128 || p_cells[i + 1] < 0 || p_cells[i + 1] > 255 || p_cells[i + 2] < 0 || p_cells[i + 2] > RAMP_WEST);
		if (cells[i] != p_cells[i] || cells[i + 1] != p_cells[i + 1] || cells[i + 2] != p_cells[i + 2]) {
			Rect2i cell_region(Vector2i((i / 3) % size.x, (i / 3) / size.x), Vector2i(1, 1));
			region = changed ? region.merge(cell_region) : cell_region;
			changed = true;
		}
	}
	if (!changed) {
		return;
	}
	cells = p_cells;
	_sanitize_ramps(region);
	_changed(region);
}

void MobaMapData::apply_cells(const Dictionary &p_patch) {
	PackedInt32Array next = cells;
	Array keys = p_patch.keys();
	for (int i = 0; i < keys.size(); ++i) {
		ERR_FAIL_COND(keys[i].get_type() != Variant::VECTOR2I || p_patch[keys[i]].get_type() != Variant::VECTOR3I);
		Vector2i cell = keys[i];
		ERR_FAIL_COND(!contains(cell));
		Vector3i value = p_patch[keys[i]];
		int offset = (cell.y * size.x + cell.x) * 3;
		next.set(offset, value.x);
		next.set(offset + 1, value.y);
		next.set(offset + 2, value.z);
	}
	set_cells(next);
}

void MobaMapData::set_cell(Vector2i p_cell, int p_level, int p_material, int p_ramp) {
	Dictionary patch;
	patch[p_cell] = Vector3i(p_level, p_material, p_ramp);
	apply_cells(patch);
}

bool MobaMapData::place_ramp(Vector2i p_cell, int p_direction, int p_width) {
	if (!can_place_ramp(p_cell, p_direction, p_width)) {
		return false;
	}
	Vector2i dir = ramp_direction(p_direction);
	Vector2i side(-dir.y, dir.x);
	Dictionary patch;
	for (int i = 0; i < p_width; ++i) {
		Vector2i c = p_cell + side * (i - (p_width - 1) / 2);
		Vector3i value = get_cell(c);
		value.z = p_direction;
		patch[c] = value;
	}
	apply_cells(patch);
	return true;
}

void MobaMapData::get_corners(Vector2i p_cell, Vector3 *r_corners) const {
	Vector3i value = get_cell(p_cell);
	real_t x = p_cell.x * cell_size;
	real_t z = p_cell.y * cell_size;
	real_t h = value.x * height_step;
	r_corners[0] = Vector3(x, h, z);
	r_corners[1] = Vector3(x + cell_size, h, z);
	r_corners[2] = Vector3(x + cell_size, h, z + cell_size);
	r_corners[3] = Vector3(x, h, z + cell_size);
	if (value.z == RAMP_NORTH) { r_corners[0].y += height_step; r_corners[1].y += height_step; }
	if (value.z == RAMP_EAST) { r_corners[1].y += height_step; r_corners[2].y += height_step; }
	if (value.z == RAMP_SOUTH) { r_corners[2].y += height_step; r_corners[3].y += height_step; }
	if (value.z == RAMP_WEST) { r_corners[3].y += height_step; r_corners[0].y += height_step; }
}

real_t MobaMapData::get_surface_height(Vector2 p_position) const {
	Vector2 grid = p_position / cell_size;
	Vector2i c((int)Math::floor(grid.x), (int)Math::floor(grid.y));
	if (!contains(c)) {
		return 0.0;
	}
	Vector3i value = get_cell(c);
	real_t slope = 0.0;
	if (value.z == RAMP_NORTH) { slope = 1.0 - (grid.y - c.y); }
	if (value.z == RAMP_EAST) { slope = grid.x - c.x; }
	if (value.z == RAMP_SOUTH) { slope = grid.y - c.y; }
	if (value.z == RAMP_WEST) { slope = 1.0 - (grid.x - c.x); }
	return (value.x + slope) * height_step;
}

int MobaMapData::get_plateau_level(Vector2 p_position) const {
	return get_cell(Vector2i((int)Math::floor(p_position.x / cell_size), (int)Math::floor(p_position.y / cell_size))).x;
}

void MobaMapData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &MobaMapData::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &MobaMapData::get_size);
	ClassDB::bind_method(D_METHOD("set_cell_size", "value"), &MobaMapData::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &MobaMapData::get_cell_size);
	ClassDB::bind_method(D_METHOD("set_height_step", "value"), &MobaMapData::set_height_step);
	ClassDB::bind_method(D_METHOD("get_height_step"), &MobaMapData::get_height_step);
	ClassDB::bind_method(D_METHOD("set_cells", "cells"), &MobaMapData::set_cells);
	ClassDB::bind_method(D_METHOD("get_cells"), &MobaMapData::get_cells);
	ClassDB::bind_method(D_METHOD("contains", "cell"), &MobaMapData::contains);
	ClassDB::bind_method(D_METHOD("get_cell", "cell"), &MobaMapData::get_cell);
	ClassDB::bind_method(D_METHOD("set_cell", "cell", "level", "material", "ramp"), &MobaMapData::set_cell, DEFVAL(RAMP_NONE));
	ClassDB::bind_method(D_METHOD("apply_cells", "patch"), &MobaMapData::apply_cells);
	ClassDB::bind_method(D_METHOD("can_place_ramp", "cell", "direction", "width"), &MobaMapData::can_place_ramp, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("place_ramp", "cell", "direction", "width"), &MobaMapData::place_ramp, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_surface_height", "local_xz"), &MobaMapData::get_surface_height);
	ClassDB::bind_method(D_METHOD("get_plateau_level", "local_xz"), &MobaMapData::get_plateau_level);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "1,64,0.1"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_step", PROPERTY_HINT_RANGE, "0.5,32,0.1"), "set_height_step", "get_height_step");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "cells", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_cells", "get_cells");
	ADD_SIGNAL(MethodInfo("region_changed", PropertyInfo(Variant::RECT2I, "region")));
	BIND_ENUM_CONSTANT(RAMP_NONE);
	BIND_ENUM_CONSTANT(RAMP_NORTH);
	BIND_ENUM_CONSTANT(RAMP_EAST);
	BIND_ENUM_CONSTANT(RAMP_SOUTH);
	BIND_ENUM_CONSTANT(RAMP_WEST);
}
