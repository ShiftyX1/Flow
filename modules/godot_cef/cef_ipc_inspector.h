/**************************************************************************/
/*  cef_ipc_inspector.h                                                   */
/**************************************************************************/

#pragma once

#include "scene/gui/control.h"

class Button;
class Label;
class LineEdit;
class Tree;

// Debug view listing the IPC traffic of a CefTexture (its debug_ipc_message signal).
class CefIpcInspector : public Control {
	GDCLASS(CefIpcInspector, Control);

	NodePath target_path;
	int max_entries = 200;
	ObjectID connected_target;

	Vector<Dictionary> entries;
	LineEdit *filter_edit = nullptr;
	Label *count_label = nullptr;
	Tree *tree = nullptr;

	void _build_ui();
	void _connect_target();
	void _disconnect_target();
	void _on_debug_ipc_message(const Dictionary &p_event);
	void _rebuild_tree();
	void _add_tree_item(const Dictionary &p_event);
	bool _matches_filter(const Dictionary &p_event) const;
	void _update_count();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_target_path(const NodePath &p_target_path);
	NodePath get_target_path() const;
	void set_max_entries(int p_max_entries);
	int get_max_entries() const;
	TypedArray<Dictionary> get_entries() const;
	void clear();

	CefIpcInspector();
};
