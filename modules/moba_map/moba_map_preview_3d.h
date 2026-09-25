#pragma once

#include "moba_map_3d.h"

#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/gui/label.h"

// Optional map-authoring harness. It is not part of a game's MOBA rules.
class MobaMapPreview3D : public Node3D {
	GDCLASS(MobaMapPreview3D, Node3D);
	NodePath map_path = NodePath("Map");
	MobaMap3D *map = nullptr;
	CharacterBody3D *hero = nullptr;
	Camera3D *camera = nullptr;
	Label *status = nullptr;
	Vector<Vector3> path;
	int waypoint = 0;
	bool placed = false;
	bool had_navigation = false;
	int navigation_settle_frames = 0;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void unhandled_input(const Ref<InputEvent> &p_event) override;

public:
	void set_map_path(const NodePath &p_path) { map_path = p_path; }
	NodePath get_map_path() const { return map_path; }
};
