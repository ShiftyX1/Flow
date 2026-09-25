#include "moba_map_marker_3d.h"

#include "core/object/class_db.h"

void MobaMapMarker3D::set_kind(Kind p_kind) {
	ERR_FAIL_COND(p_kind < SPAWN || p_kind > POINT_OF_INTEREST);
	kind = p_kind;
	update_gizmos();
}

void MobaMapMarker3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_kind", "kind"), &MobaMapMarker3D::set_kind);
	ClassDB::bind_method(D_METHOD("get_kind"), &MobaMapMarker3D::get_kind);
	ClassDB::bind_method(D_METHOD("set_marker_id", "id"), &MobaMapMarker3D::set_marker_id);
	ClassDB::bind_method(D_METHOD("get_marker_id"), &MobaMapMarker3D::get_marker_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "kind", PROPERTY_HINT_ENUM, "Spawn,Point of interest"), "set_kind", "get_kind");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "marker_id"), "set_marker_id", "get_marker_id");
	BIND_ENUM_CONSTANT(SPAWN);
	BIND_ENUM_CONSTANT(POINT_OF_INTEREST);
}
