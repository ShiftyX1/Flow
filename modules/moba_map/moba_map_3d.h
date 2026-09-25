#pragma once

#include "moba_map_data.h"
#include "moba_map_marker_3d.h"
#include "moba_map_palette.h"

#include "core/math/triangle_mesh.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"

class MobaMap3D : public Node3D {
	GDCLASS(MobaMap3D, Node3D);

	struct Chunk {
		MeshInstance3D *mesh = nullptr;
		CollisionShape3D *collision = nullptr;
		Ref<TriangleMesh> triangles;
	};
	static constexpr int CHUNK_SIZE = 16;
	Ref<MobaMapData> data;
	Ref<MobaMapPalette> palette;
	HashMap<Vector2i, Chunk> chunks;
	HashSet<Vector2i> dirty_chunks;
	Node3D *generated = nullptr;
	NavigationRegion3D *navigation = nullptr;
	bool rebuild_queued = false;
	bool navigation_dirty = true;
	bool baking = false;
	bool rebake_requested = false;
	uint64_t revision = 0;
	uint32_t object_hash = 0;
	double object_poll = 0.0;
	Vector2i built_size;

	void _region_changed(Rect2i p_region);
	void _palette_changed();
	void _rebuild_chunk(Vector2i p_chunk);
	void _clear_generated();
	void _navigation_baked(Ref<NavigationMesh> p_mesh, uint64_t p_revision, uint32_t p_objects);
	void _resnap_objects();
	uint32_t _objects_hash() const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_map_data(const Ref<MobaMapData> &p_data);
	Ref<MobaMapData> get_map_data() const { return data; }
	void set_palette(const Ref<MobaMapPalette> &p_palette);
	Ref<MobaMapPalette> get_palette() const { return palette; }
	void rebuild_dirty();
	void invalidate_navigation();
	void bake_navigation();
	bool is_navigation_ready() const { return !navigation_dirty && !baking; }
	bool is_baking_navigation() const { return baking; }
	Ref<NavigationMesh> get_navigation_mesh() const;
	real_t get_surface_height(Vector2 p_local_xz) const;
	int get_plateau_level(Vector2 p_local_xz) const;
	TypedArray<MobaMapMarker3D> get_markers() const;
	bool raycast(const Vector3 &p_origin, const Vector3 &p_direction, Vector3 &r_hit) const;
	Dictionary raycast_surface(Vector3 p_world_origin, Vector3 p_world_direction) const;
	void attach_object(Node3D *p_object, Node *p_owner);
	void detach_object(Node3D *p_object);
	static void adopt_object_owner(Node *p_object, Node *p_owner);
	PackedStringArray get_configuration_warnings() const override;
	MobaMap3D();
};
