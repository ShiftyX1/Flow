#pragma once

#include "../voxel_block_registry.h"
#include "../voxel_scene.h"
#include "../voxel_scene_data.h"

#include "editor/plugins/editor_plugin.h"
#include "editor/scene/3d/node_3d_editor_gizmos.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/spin_box.h"

class VoxelSceneEditorPlugin;

// =============================================================================
// Gizmo plugin — draws voxel scene AABB + hover/selection cell highlight.
// =============================================================================
class VoxelSceneGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(VoxelSceneGizmoPlugin, EditorNode3DGizmoPlugin);

	VoxelSceneEditorPlugin *editor_plugin = nullptr;

protected:
	virtual bool has_gizmo(Node3D *p_spatial) override;

public:
	void set_editor_plugin(VoxelSceneEditorPlugin *p_plugin) { editor_plugin = p_plugin; }

	virtual String get_gizmo_name() const override { return "VoxelScene"; }
	virtual int get_priority() const override { return -1; }

	virtual void redraw(EditorNode3DGizmo *p_gizmo) override;

	VoxelSceneGizmoPlugin();
};

// =============================================================================
// Toolbar — placed in the spatial editor menu. Tool buttons + size editor.
// =============================================================================
class VoxelSceneToolbar : public HBoxContainer {
	GDCLASS(VoxelSceneToolbar, HBoxContainer);

public:
	enum Tool {
		TOOL_PAINT,
		TOOL_ERASE,
		TOOL_PICK,
		TOOL_BOX_FILL,
		TOOL_BOX_ERASE,
		TOOL_MAX,
	};

private:
	VoxelSceneEditorPlugin *editor_plugin = nullptr;
	Button *tool_buttons[TOOL_MAX] = { nullptr };
	Button *resize_button = nullptr;
	Label *status_label = nullptr;
	Tool current_tool = TOOL_PAINT;

	void _on_tool_pressed(int p_tool);
	void _on_resize_pressed();

protected:
	void _notification(int p_what);

public:
	void set_editor_plugin(VoxelSceneEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Tool get_current_tool() const { return current_tool; }
	void set_status_text(const String &p_text);

	VoxelSceneToolbar();
};

// =============================================================================
// Palette dock — selectable list of all blocks in the scene's registry.
// =============================================================================
class VoxelScenePaletteDock : public VBoxContainer {
	GDCLASS(VoxelScenePaletteDock, VBoxContainer);

	VoxelSceneEditorPlugin *editor_plugin = nullptr;
	Ref<VoxelBlockRegistry> registry;
	ItemList *block_list = nullptr;
	Label *info_label = nullptr;
	int selected_block_id = 1; // Default to first non-air block.

	void _on_item_selected(int p_idx);
	void _on_registry_changed();
	void _rebuild();

public:
	void set_editor_plugin(VoxelSceneEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	void set_registry(const Ref<VoxelBlockRegistry> &p_registry);
	void set_selected_block_id(int p_id);
	int get_selected_block_id() const { return selected_block_id; }

	VoxelScenePaletteDock();
	~VoxelScenePaletteDock();
};

// =============================================================================
// Resize dialog.
// =============================================================================
class VoxelSceneResizeDialog : public ConfirmationDialog {
	GDCLASS(VoxelSceneResizeDialog, ConfirmationDialog);

	SpinBox *spin_x = nullptr;
	SpinBox *spin_y = nullptr;
	SpinBox *spin_z = nullptr;
	VoxelSceneEditorPlugin *editor_plugin = nullptr;

	void _on_confirmed();

public:
	void set_editor_plugin(VoxelSceneEditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	void open_with_size(const Vector3i &p_size);

	VoxelSceneResizeDialog();
};

// =============================================================================
// Main editor plugin.
// =============================================================================
class VoxelSceneEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelSceneEditorPlugin, EditorPlugin);

	VoxelScene *current_scene = nullptr;
	VoxelSceneToolbar *toolbar = nullptr;
	VoxelScenePaletteDock *palette = nullptr;
	VoxelSceneResizeDialog *resize_dialog = nullptr;
	Ref<VoxelSceneGizmoPlugin> gizmo_plugin;

	// Hover/preview state.
	bool has_hover = false;
	bool hover_block_hit = false; // true = existing block hit; false = empty volume placement hit.
	Vector3i hover_cell;       // voxel cell pointed at (existing block hit, in voxel coords)
	Vector3i hover_normal;     // face normal of that hit (e.g. (0,1,0))
	Vector3i hover_place_cell; // cell that would be filled by Paint (= hover_cell + hover_normal)

	// Box op state.
	bool box_first_set = false;
	Vector3i box_first_cell;

	struct VoxelRaycastHit {
		bool hit = false;
		bool block_hit = false;
		Vector3i cell;
		Vector3i normal;
		Vector3i place_cell;
	};

	// Internal: DDA voxel raycast in scene local space.
	// Returns an existing block hit, or an empty volume hit for first-block placement.
	VoxelRaycastHit _voxel_raycast(const Vector3 &p_local_origin, const Vector3 &p_local_dir) const;

	void _apply_single_block(const Vector3i &p_cell, int p_new_id, const String &p_action_name);
	void _apply_box(const Vector3i &p_a, const Vector3i &p_b, int p_new_id, const String &p_action_name);

public:
	virtual String get_plugin_name() const override { return "VoxelScene"; }
	virtual bool handles(Object *p_object) const override;
	virtual void edit(Object *p_object) override;
	virtual void make_visible(bool p_visible) override;
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

	VoxelScene *get_current_scene() const { return current_scene; }
	VoxelSceneToolbar *get_toolbar() const { return toolbar; }
	VoxelScenePaletteDock *get_palette() const { return palette; }

	// Used by toolbar's resize button.
	void open_resize_dialog();
	// Used by resize dialog when confirmed.
	void apply_resize(const Vector3i &p_new_size);

	// Refresh AABB gizmo when state changes.
	void update_gizmo_for_current();

	// Hover info for gizmo drawing.
	bool get_hover(Vector3i &r_cell, Vector3i &r_normal, Vector3i &r_place_cell, bool &r_block_hit) const {
		if (!has_hover) {
			return false;
		}
		r_cell = hover_cell;
		r_normal = hover_normal;
		r_place_cell = hover_place_cell;
		r_block_hit = hover_block_hit;
		return true;
	}
	bool get_box_first(Vector3i &r_cell) const {
		if (!box_first_set) {
			return false;
		}
		r_cell = box_first_cell;
		return true;
	}

	VoxelSceneEditorPlugin();
	~VoxelSceneEditorPlugin();
};
