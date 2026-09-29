/**************************************************************************/
/*  cef_ipc_inspector.cpp                                                 */
/**************************************************************************/

#include "cef_ipc_inspector.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "core/variant/typed_array.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/tree.h"

static const int BODY_PREVIEW_LENGTH = 256;

void CefIpcInspector::_build_ui() {
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	add_child(vbox, false, INTERNAL_MODE_FRONT);

	HBoxContainer *toolbar = memnew(HBoxContainer);
	vbox->add_child(toolbar);

	filter_edit = memnew(LineEdit);
	filter_edit->set_placeholder("Filter");
	filter_edit->set_clear_button_enabled(true);
	filter_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	filter_edit->connect(SceneStringName(text_changed), callable_mp(this, &CefIpcInspector::_rebuild_tree).unbind(1));
	toolbar->add_child(filter_edit);

	count_label = memnew(Label);
	toolbar->add_child(count_label);

	Button *clear_button = memnew(Button);
	clear_button->set_text("Clear");
	clear_button->connect(SceneStringName(pressed), callable_mp(this, &CefIpcInspector::clear));
	toolbar->add_child(clear_button);

	tree = memnew(Tree);
	tree->set_v_size_flags(SIZE_EXPAND_FILL);
	tree->set_columns(5);
	tree->set_column_titles_visible(true);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_ROW);
	const char *titles[] = { "Time", "Direction", "Lane", "Size", "Body" };
	for (int i = 0; i < 5; i++) {
		tree->set_column_title(i, titles[i]);
		tree->set_column_expand(i, i == 4);
		tree->set_column_clip_content(i, i == 4);
	}
	tree->create_item();
	vbox->add_child(tree);
	_update_count();
}

void CefIpcInspector::_connect_target() {
	_disconnect_target();
	if (!is_inside_tree() || target_path.is_empty()) {
		return;
	}
	Node *target = get_node_or_null(target_path);
	if (!target) {
		return;
	}
	if (!target->has_signal(SNAME("debug_ipc_message"))) {
		WARN_PRINT(vformat("CefIpcInspector: \"%s\" has no debug_ipc_message signal.", String(target_path)));
		return;
	}
	target->connect(SNAME("debug_ipc_message"), callable_mp(this, &CefIpcInspector::_on_debug_ipc_message));
	connected_target = target->get_instance_id();
}

void CefIpcInspector::_disconnect_target() {
	Object *target = ObjectDB::get_instance(connected_target);
	if (target && target->is_connected(SNAME("debug_ipc_message"), callable_mp(this, &CefIpcInspector::_on_debug_ipc_message))) {
		target->disconnect(SNAME("debug_ipc_message"), callable_mp(this, &CefIpcInspector::_on_debug_ipc_message));
	}
	connected_target = ObjectID();
}

bool CefIpcInspector::_matches_filter(const Dictionary &p_event) const {
	const String filter = filter_edit ? filter_edit->get_text().strip_edges() : String();
	if (filter.is_empty()) {
		return true;
	}
	return String(p_event.get("body", "")).containsn(filter) || String(p_event.get("lane", "")).containsn(filter) ||
			String(p_event.get("direction", "")).containsn(filter);
}

void CefIpcInspector::_add_tree_item(const Dictionary &p_event) {
	if (!tree || !_matches_filter(p_event)) {
		return;
	}
	TreeItem *item = tree->create_item(tree->get_root());
	const int64_t timestamp = p_event.get("timestamp_unix_ms", 0);
	const String time = Time::get_singleton()->get_time_string_from_unix_time(timestamp / 1000) + vformat(".%03d", int(timestamp % 1000));
	const String direction = p_event.get("direction", "");
	String body = p_event.get("body", "");
	item->set_tooltip_text(4, body.left(4096));
	if (body.length() > BODY_PREVIEW_LENGTH) {
		body = body.left(BODY_PREVIEW_LENGTH) + "…";
	}
	item->set_text(0, time);
	item->set_text(1, direction == "to_godot" ? String::utf8("renderer → Godot") : String::utf8("Godot → renderer"));
	item->set_text(2, p_event.get("lane", ""));
	item->set_text(3, String::humanize_size(int64_t(p_event.get("body_size_bytes", 0))));
	item->set_text(4, body.replace_char('\n', ' '));
	item->set_metadata(0, p_event);
}

void CefIpcInspector::_on_debug_ipc_message(const Dictionary &p_event) {
	entries.push_back(p_event.duplicate());
	bool trimmed = false;
	while (entries.size() > max_entries) {
		entries.remove_at(0);
		trimmed = true;
	}
	if (trimmed) {
		_rebuild_tree();
	} else {
		_add_tree_item(p_event);
		_update_count();
	}
}

void CefIpcInspector::_rebuild_tree() {
	if (!tree) {
		return;
	}
	tree->clear();
	tree->create_item();
	for (const Dictionary &entry : entries) {
		_add_tree_item(entry);
	}
	_update_count();
}

void CefIpcInspector::_update_count() {
	if (count_label) {
		count_label->set_text(vformat("%d / %d", entries.size(), max_entries));
	}
}

void CefIpcInspector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_connect_target();
		} break;
		case NOTIFICATION_ENTER_TREE: {
			if (is_ready()) {
				_connect_target();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_disconnect_target();
		} break;
	}
}

void CefIpcInspector::set_target_path(const NodePath &p_target_path) {
	target_path = p_target_path;
	if (is_inside_tree() && is_ready()) {
		_connect_target();
	}
}

NodePath CefIpcInspector::get_target_path() const {
	return target_path;
}

void CefIpcInspector::set_max_entries(int p_max_entries) {
	max_entries = MAX(1, p_max_entries);
	if (entries.size() > max_entries) {
		entries = entries.slice(entries.size() - max_entries);
		_rebuild_tree();
	} else {
		_update_count();
	}
}

int CefIpcInspector::get_max_entries() const {
	return max_entries;
}

TypedArray<Dictionary> CefIpcInspector::get_entries() const {
	TypedArray<Dictionary> result;
	for (const Dictionary &entry : entries) {
		result.push_back(entry);
	}
	return result;
}

void CefIpcInspector::clear() {
	entries.clear();
	_rebuild_tree();
}

void CefIpcInspector::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_target_path", "target_path"), &CefIpcInspector::set_target_path);
	ClassDB::bind_method(D_METHOD("get_target_path"), &CefIpcInspector::get_target_path);
	ClassDB::bind_method(D_METHOD("set_max_entries", "max_entries"), &CefIpcInspector::set_max_entries);
	ClassDB::bind_method(D_METHOD("get_max_entries"), &CefIpcInspector::get_max_entries);
	ClassDB::bind_method(D_METHOD("get_entries"), &CefIpcInspector::get_entries);
	ClassDB::bind_method(D_METHOD("clear"), &CefIpcInspector::clear);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "CefTexture"), "set_target_path", "get_target_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_entries", PROPERTY_HINT_RANGE, "1,10000,1,or_greater"), "set_max_entries", "get_max_entries");
}

CefIpcInspector::CefIpcInspector() {
	_build_ui();
}
