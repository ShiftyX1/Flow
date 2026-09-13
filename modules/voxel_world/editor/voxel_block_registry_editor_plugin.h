#pragma once

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_container.h"

#include "../voxel_block_registry.h"

class EditorResourcePicker;
class ColorRect;
class Label;
class Button;
class CheckBox;
class SpinBox;
class ColorPickerButton;

// Popup dialog with the full block visual editor.
class VoxelBlockRegistryEditorDialog : public AcceptDialog {
	GDCLASS(VoxelBlockRegistryEditorDialog, AcceptDialog);

	Ref<VoxelBlockRegistry> registry;

	VBoxContainer *block_list_vbox = nullptr;
	ScrollContainer *scroll = nullptr;

	struct BlockRow {
		int block_id = 0;
		EditorResourcePicker *picker_top = nullptr;
		EditorResourcePicker *picker_side = nullptr;
		EditorResourcePicker *picker_bottom = nullptr;
		EditorResourcePicker *picker_shader = nullptr;
		// Model visual controls.
		OptionButton *option_visual_mode = nullptr;
		EditorResourcePicker *picker_model_mesh = nullptr;
		EditorResourcePicker *picker_model_scene = nullptr;
		SpinBox *spin_model_offset[3] = { nullptr, nullptr, nullptr };
		SpinBox *spin_model_rotation_y = nullptr;
		SpinBox *spin_model_scale[3] = { nullptr, nullptr, nullptr };
		LineEdit *edit_animation_idle = nullptr;
		LineEdit *edit_animation_active = nullptr;
		LineEdit *edit_animation_open = nullptr;
		// Physics & lighting controls.
		CheckBox *check_solid = nullptr;
		CheckBox *check_transparent = nullptr;
		ColorPickerButton *picker_color = nullptr;
		SpinBox *spin_opacity = nullptr;
		SpinBox *spin_emission = nullptr;
		ColorPickerButton *picker_light_color = nullptr;
		// Mining / survival controls.
		CheckBox *check_breakable = nullptr;
		CheckBox *check_requires_tool = nullptr;
		SpinBox *spin_hand_break_time = nullptr;
	};

	Vector<BlockRow> block_rows;

	void _rebuild_block_list();
	BlockRow *_find_block_row(int p_block_id);
	void _update_visual_controls_enabled(int p_block_id);
	void _texture_changed(const Ref<Resource> &p_resource, int p_block_id, int p_face);
	void _shader_changed(const Ref<Resource> &p_resource, int p_block_id);
	void _visual_mode_selected(int p_index, int p_block_id);
	void _model_mesh_changed(const Ref<Resource> &p_resource, int p_block_id);
	void _model_scene_changed(const Ref<Resource> &p_resource, int p_block_id);
	void _model_offset_axis_changed(double p_value, int p_block_id, int p_axis);
	void _model_rotation_y_changed(double p_value, int p_block_id);
	void _model_scale_axis_changed(double p_value, int p_block_id, int p_axis);
	void _animation_state_submitted(const String &p_text, int p_block_id, const StringName &p_key);
	void _animation_state_focus_exited(LineEdit *p_line_edit, int p_block_id, const StringName &p_key);
	void _solid_toggled(bool p_pressed, int p_block_id);
	void _transparent_toggled(bool p_pressed, int p_block_id);
	void _color_changed(const Color &p_color, int p_block_id);
	void _light_opacity_changed(double p_value, int p_block_id);
	void _emission_changed(double p_value, int p_block_id);
	void _light_color_changed(const Color &p_color, int p_block_id);
	// Mining callbacks.
	void _breakable_toggled(bool p_pressed, int p_block_id);
	void _requires_tool_toggled(bool p_pressed, int p_block_id);
	void _hand_break_time_changed(double p_value, int p_block_id);
	void _tool_entry_add_pressed(int p_block_id);
	void _tool_entry_remove_pressed(int p_block_id, int p_entry_idx);
	void _tool_entry_type_submitted(const String &p_text, int p_block_id, int p_entry_idx);
	void _tool_entry_tier_changed(double p_value, int p_block_id, int p_entry_idx);
	void _tool_entry_time_changed(double p_value, int p_block_id, int p_entry_idx);

public:
	void set_registry(const Ref<VoxelBlockRegistry> &p_registry);
	VoxelBlockRegistryEditorDialog();
};

// Small widget in the inspector: shows summary + visual editor button.
class VoxelBlockRegistryInspectorButton : public VBoxContainer {
	GDCLASS(VoxelBlockRegistryInspectorButton, VBoxContainer);

	Ref<VoxelBlockRegistry> registry;
	VoxelBlockRegistryEditorDialog *dialog = nullptr;
	Button *edit_button = nullptr;

	void _on_edit_pressed();

public:
	void set_registry(const Ref<VoxelBlockRegistry> &p_registry);
	VoxelBlockRegistryInspectorButton();
};

// Inspector plugin.
class EditorInspectorPluginVoxelBlockRegistry : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginVoxelBlockRegistry, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
			const PropertyHint p_hint, const String &p_hint_text,
			const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

// Editor plugin — registration glue.
class VoxelBlockRegistryEditorPlugin : public EditorPlugin {
	GDCLASS(VoxelBlockRegistryEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "VoxelBlockRegistry"; }
	VoxelBlockRegistryEditorPlugin();
};
