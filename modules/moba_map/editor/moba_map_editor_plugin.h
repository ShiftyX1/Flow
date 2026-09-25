#pragma once

#include "../moba_map_3d.h"

#include "core/math/random_pcg.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/scene/3d/node_3d_editor_gizmos.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"

class MobaMapEditorPlugin;

class MobaMapGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(MobaMapGizmoPlugin, EditorNode3DGizmoPlugin);
	MobaMapEditorPlugin *editor = nullptr;

public:
	void set_editor(MobaMapEditorPlugin *p_editor) { editor = p_editor; }
	bool has_gizmo(Node3D *p_node) override;
	String get_gizmo_name() const override { return "MobaMap"; }
	void redraw(EditorNode3DGizmo *p_gizmo) override;
	MobaMapGizmoPlugin();
};

class MobaMapEditorPlugin : public EditorPlugin {
	GDCLASS(MobaMapEditorPlugin, EditorPlugin);

public:
	enum Tool { SELECT, RAISE, LOWER, LEVEL, MATERIAL, RAMP, PLACE, FOREST, ERASE, SPAWN, POINT };

private:
	ObjectID map_id;
	ObjectID stroke_map_id;
	ObjectID preview_map_id;
	ObjectID preview_root_id;
	HBoxContainer *toolbar = nullptr;
	VBoxContainer *dock = nullptr;
	OptionButton *tools = nullptr;
	OptionButton *shape = nullptr;
	OptionButton *materials = nullptr;
	OptionButton *objects = nullptr;
	OptionButton *ramp_direction = nullptr;
	SpinBox *radius = nullptr;
	SpinBox *density = nullptr;
	SpinBox *ramp_width = nullptr;
	CheckBox *grid = nullptr;
	CheckBox *levels = nullptr;
	CheckBox *nav = nullptr;
	Label *status = nullptr;
	ConfirmationDialog *create_dialog = nullptr;
	SpinBox *width = nullptr;
	SpinBox *depth = nullptr;
	SpinBox *cell_scale = nullptr;
	SpinBox *height_step = nullptr;
	Ref<MobaMapGizmoPlugin> gizmos;
	Ref<MobaMapPalette> observed_palette;
	bool hovered = false;
	Vector3 hover_position;
	Vector2i hover_cell;
	bool stroke = false;
	bool stroke_erase = false;
	bool stroke_flatten = false;
	int target_level = 0;
	Vector2 last_stamp;
	PackedInt32Array before;
	HashSet<Vector2i> touched;
	Vector<Node3D *> added;
	Vector<Node3D *> removed;
	HashMap<ObjectID, Node *> removed_owners;
	RandomPCG random;
	String notice;

	void _show_create();
	void _create_map();
	void _tool_changed(int p_index);
	void _settings_changed();
	void _radius_changed(double p_value);
	void _overlay_changed(bool p_enabled);
	void _refresh_palette();
	void _begin_stroke(bool p_erase, bool p_flatten);
	void _stamp(Vector2 p_position);
	void _finish_stroke(bool p_cancel = false);
	void _put_object(Node3D *p_object, Vector2 p_position, real_t p_rotation);
	void _bake();
	void _run_preview();
	void _preview_baked(bool p_success);
	void _write_preview();
	void _cancel_preview();
	void _update_status();

protected:
	static void _bind_methods() {}
	void _notification(int p_what);

public:
	MobaMap3D *get_map() const;
	Tool get_tool() const { return (Tool)tools->get_selected(); }
	bool get_hover(Vector2i &r_cell, Vector3 &r_position) const;
	bool show_grid() const { return grid->is_pressed(); }
	bool show_levels() const { return levels->is_pressed(); }
	bool show_navigation() const { return nav->is_pressed(); }
	Vector<Vector2i> get_brush_cells(Vector2 p_position) const;
	bool is_ramp_valid(Vector2i p_cell) const;
	int get_ramp_direction() const { return ramp_direction->get_selected() + 1; }
	int get_ramp_width() const { return (int)ramp_width->get_value(); }
	String get_plugin_name() const override { return "MobaMap"; }
	bool handles(Object *p_object) const override;
	void edit(Object *p_object) override;
	void make_visible(bool p_visible) override;
	void apply_changes() override;
	EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;
	MobaMapEditorPlugin();
	~MobaMapEditorPlugin();
};
