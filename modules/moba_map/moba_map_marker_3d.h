#pragma once

#include "scene/3d/node_3d.h"

class MobaMapMarker3D : public Node3D {
	GDCLASS(MobaMapMarker3D, Node3D);

public:
	enum Kind { SPAWN, POINT_OF_INTEREST };

private:
	Kind kind = POINT_OF_INTEREST;
	StringName marker_id = "point";

protected:
	static void _bind_methods();

public:
	void set_kind(Kind p_kind);
	Kind get_kind() const { return kind; }
	void set_marker_id(const StringName &p_id) { marker_id = p_id; }
	StringName get_marker_id() const { return marker_id; }
};

VARIANT_ENUM_CAST(MobaMapMarker3D::Kind);
