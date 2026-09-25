#pragma once

#include "core/io/resource.h"
#include "core/math/rect2i.h"
#include "core/math/vector3i.h"

// Each cell stores a plateau level, a palette material index and a ramp direction.
// A ramp occupies the low cell and rises toward its high neighbour.
class MobaMapData : public Resource {
	GDCLASS(MobaMapData, Resource);

public:
	enum Ramp { RAMP_NONE, RAMP_NORTH, RAMP_EAST, RAMP_SOUTH, RAMP_WEST };

private:
	Vector2i size = Vector2i(64, 64);
	real_t cell_size = 4.0;
	real_t height_step = 2.0;
	PackedInt32Array cells;
	uint64_t revision = 0;
	void _changed(const Rect2i &p_region);
	void _sanitize_ramps(Rect2i &r_region);

protected:
	static void _bind_methods();

public:
	void set_size(Vector2i p_size);
	Vector2i get_size() const { return size; }
	void set_cell_size(real_t p_value);
	real_t get_cell_size() const { return cell_size; }
	void set_height_step(real_t p_value);
	real_t get_height_step() const { return height_step; }
	void set_cells(const PackedInt32Array &p_cells);
	PackedInt32Array get_cells() const { return cells; }
	uint64_t get_revision() const { return revision; }
	bool contains(Vector2i p_cell) const;
	Vector3i get_cell(Vector2i p_cell) const;
	void set_cell(Vector2i p_cell, int p_level, int p_material, int p_ramp = RAMP_NONE);
	void apply_cells(const Dictionary &p_patch);
	static Vector2i ramp_direction(int p_ramp);
	bool can_place_ramp(Vector2i p_cell, int p_direction, int p_width = 1) const;
	bool place_ramp(Vector2i p_cell, int p_direction, int p_width = 1);
	void get_corners(Vector2i p_cell, Vector3 *r_corners) const;
	real_t get_surface_height(Vector2 p_position) const;
	int get_plateau_level(Vector2 p_position) const;
	MobaMapData();
};

VARIANT_ENUM_CAST(MobaMapData::Ramp);
