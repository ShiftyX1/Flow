/**************************************************************************/
/*  cef_ipc_inspector.h                                                   */
/**************************************************************************/

#pragma once

#include "cef_texture.h"
#include "scene/gui/control.h"

class CefIpcInspector : public Control {
	GDCLASS(CefIpcInspector, Control);

	NodePath target_path;
	int max_entries = 200;

protected:
	static void _bind_methods();

public:
	void set_target_path(const NodePath &p_target_path);
	NodePath get_target_path() const;
	void set_max_entries(int p_max_entries);
	int get_max_entries() const;
	void clear();
};
