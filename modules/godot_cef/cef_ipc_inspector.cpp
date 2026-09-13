/**************************************************************************/
/*  cef_ipc_inspector.cpp                                                 */
/**************************************************************************/

#include "cef_ipc_inspector.h"

#include "core/object/class_db.h"

void CefIpcInspector::set_target_path(const NodePath &p_target_path) {
	target_path = p_target_path;
}

NodePath CefIpcInspector::get_target_path() const {
	return target_path;
}

void CefIpcInspector::set_max_entries(int p_max_entries) {
	max_entries = MAX(1, p_max_entries);
}

int CefIpcInspector::get_max_entries() const {
	return max_entries;
}

void CefIpcInspector::clear() {
}

void CefIpcInspector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_path", "target_path"), &CefIpcInspector::set_target_path);
	ClassDB::bind_method(D_METHOD("get_target_path"), &CefIpcInspector::get_target_path);
	ClassDB::bind_method(D_METHOD("set_max_entries", "max_entries"), &CefIpcInspector::set_max_entries);
	ClassDB::bind_method(D_METHOD("get_max_entries"), &CefIpcInspector::get_max_entries);
	ClassDB::bind_method(D_METHOD("clear"), &CefIpcInspector::clear);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_path"), "set_target_path", "get_target_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_entries", PROPERTY_HINT_RANGE, "1,10000,1,or_greater"), "set_max_entries", "get_max_entries");
}
