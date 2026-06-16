#include "voxel_block_registry_editor_plugin.h"

#include "../voxel_block_registry.h"

#include "core/object/callable_mp.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/scene_string_names.h"

namespace {

String _get_animation_state_value(const Dictionary &p_map, const StringName &p_key) {
	if (p_map.has(p_key)) {
		return String(p_map[p_key]);
	}
	const String key_text = String(p_key);
	if (p_map.has(key_text)) {
		return String(p_map[key_text]);
	}
	return String();
}

void _erase_animation_state_key(Dictionary &r_map, const StringName &p_key) {
	r_map.erase(p_key);
	r_map.erase(String(p_key));
}

void _set_vector_axis(Vector3 &r_vector, int p_axis, double p_value) {
	switch (p_axis) {
		case 0:
			r_vector.x = (float)p_value;
			break;
		case 1:
			r_vector.y = (float)p_value;
			break;
		case 2:
			r_vector.z = (float)p_value;
			break;
	}
}

SpinBox *_make_model_spin_box(double p_min, double p_max, double p_step, double p_value) {
	SpinBox *spin = memnew(SpinBox);
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_value(p_value);
	spin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	return spin;
}

} // namespace

// ============================================================
// VoxelBlockRegistryEditorDialog
// ============================================================

VoxelBlockRegistryEditorDialog::VoxelBlockRegistryEditorDialog() {
	set_title("Voxel Block Registry - Visual Editor");
	set_min_size(Size2(960 * EDSCALE, 560 * EDSCALE));

	// Hide the built-in message label so only our content shows.
	get_label()->hide();

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main_vbox);

	scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	main_vbox->add_child(scroll);

	block_list_vbox = memnew(VBoxContainer);
	block_list_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(block_list_vbox);
}

void VoxelBlockRegistryEditorDialog::set_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	registry = p_registry;
	_rebuild_block_list();
}

VoxelBlockRegistryEditorDialog::BlockRow *VoxelBlockRegistryEditorDialog::_find_block_row(int p_block_id) {
	for (int i = 0; i < block_rows.size(); i++) {
		if (block_rows[i].block_id == p_block_id) {
			return &block_rows.write[i];
		}
	}
	return nullptr;
}

void VoxelBlockRegistryEditorDialog::_update_visual_controls_enabled(int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	BlockRow *row = _find_block_row(p_block_id);
	if (row == nullptr) {
		return;
	}

	const VoxelBlockRegistry::VisualMode mode = registry->get_block_visual_mode(p_block_id);
	const bool model_mode = mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH || mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE;
	const bool mesh_mode = mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH;
	const bool scene_mode = mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE;

	if (row->picker_model_mesh != nullptr) {
		row->picker_model_mesh->set_editable(mesh_mode);
	}
	if (row->picker_model_scene != nullptr) {
		row->picker_model_scene->set_editable(scene_mode);
	}
	for (int i = 0; i < 3; i++) {
		if (row->spin_model_offset[i] != nullptr) {
			row->spin_model_offset[i]->set_editable(model_mode);
		}
		if (row->spin_model_scale[i] != nullptr) {
			row->spin_model_scale[i]->set_editable(model_mode);
		}
	}
	if (row->spin_model_rotation_y != nullptr) {
		row->spin_model_rotation_y->set_editable(model_mode);
	}
	if (row->edit_animation_idle != nullptr) {
		row->edit_animation_idle->set_editable(scene_mode);
	}
	if (row->edit_animation_active != nullptr) {
		row->edit_animation_active->set_editable(scene_mode);
	}
	if (row->edit_animation_open != nullptr) {
		row->edit_animation_open->set_editable(scene_mode);
	}
}

void VoxelBlockRegistryEditorDialog::_texture_changed(const Ref<Resource> &p_resource, int p_block_id, int p_face) {
	if (registry.is_null()) {
		return;
	}

	Ref<Texture2D> tex = p_resource;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	switch (p_face) {
		case 0: {
			undo_redo->create_action("Set Block Texture Top");
			undo_redo->add_do_property(registry.ptr(), vformat("block/%d/texture_top", p_block_id), tex);
			undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/texture_top", p_block_id), registry->get_block_texture_top(p_block_id));
			undo_redo->commit_action();
		} break;
		case 1: {
			undo_redo->create_action("Set Block Texture Side");
			undo_redo->add_do_property(registry.ptr(), vformat("block/%d/texture_side", p_block_id), tex);
			undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/texture_side", p_block_id), registry->get_block_texture_side(p_block_id));
			undo_redo->commit_action();
		} break;
		case 2: {
			undo_redo->create_action("Set Block Texture Bottom");
			undo_redo->add_do_property(registry.ptr(), vformat("block/%d/texture_bottom", p_block_id), tex);
			undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/texture_bottom", p_block_id), registry->get_block_texture_bottom(p_block_id));
			undo_redo->commit_action();
		} break;
	}
}

void VoxelBlockRegistryEditorDialog::_shader_changed(const Ref<Resource> &p_resource, int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	Ref<ShaderMaterial> mat = p_resource;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action("Set Block Shader Material");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/shader_material", p_block_id), mat);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/shader_material", p_block_id), registry->get_block_shader_material(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_visual_mode_selected(int p_index, int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	VoxelBlockRegistry::VisualMode mode = VoxelBlockRegistry::VISUAL_MODE_VOXEL;
	if (p_index == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH) {
		mode = VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH;
	} else if (p_index == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE) {
		mode = VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE;
	}

	const VoxelBlockRegistry::VisualMode old_mode = registry->get_block_visual_mode(p_block_id);
	if (old_mode == mode) {
		_update_visual_controls_enabled(p_block_id);
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Visual Mode");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/visual_mode", p_block_id), VoxelBlockRegistry::visual_mode_to_string(mode));
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/visual_mode", p_block_id), VoxelBlockRegistry::visual_mode_to_string(old_mode));
	undo_redo->commit_action();
	_update_visual_controls_enabled(p_block_id);
}

void VoxelBlockRegistryEditorDialog::_model_mesh_changed(const Ref<Resource> &p_resource, int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	Ref<Mesh> mesh = p_resource;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Model Mesh");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/model_mesh", p_block_id), mesh);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/model_mesh", p_block_id), registry->get_block_model_mesh(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_model_scene_changed(const Ref<Resource> &p_resource, int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	Ref<PackedScene> scene = p_resource;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Model Scene");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/model_scene", p_block_id), scene);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/model_scene", p_block_id), registry->get_block_model_scene(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_model_offset_axis_changed(double p_value, int p_block_id, int p_axis) {
	if (registry.is_null()) {
		return;
	}

	const Vector3 old_offset = registry->get_block_model_offset(p_block_id);
	Vector3 new_offset = old_offset;
	_set_vector_axis(new_offset, p_axis, p_value);
	if (old_offset == new_offset) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Model Offset", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/model_offset", p_block_id), new_offset);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/model_offset", p_block_id), old_offset);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_model_rotation_y_changed(double p_value, int p_block_id) {
	if (registry.is_null()) {
		return;
	}

	const float old_rotation = registry->get_block_model_rotation_y(p_block_id);
	const float new_rotation = (float)p_value;
	if (old_rotation == new_rotation) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Model Rotation", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/model_rotation_y", p_block_id), new_rotation);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/model_rotation_y", p_block_id), old_rotation);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_model_scale_axis_changed(double p_value, int p_block_id, int p_axis) {
	if (registry.is_null()) {
		return;
	}

	const Vector3 old_scale = registry->get_block_model_scale(p_block_id);
	Vector3 new_scale = old_scale;
	_set_vector_axis(new_scale, p_axis, p_value);
	if (old_scale == new_scale) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Model Scale", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/model_scale", p_block_id), new_scale);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/model_scale", p_block_id), old_scale);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_animation_state_submitted(const String &p_text, int p_block_id, const StringName &p_key) {
	if (registry.is_null()) {
		return;
	}

	Dictionary old_map = registry->get_block_animation_state_map(p_block_id);
	const String old_value = _get_animation_state_value(old_map, p_key);
	const String new_value = p_text.strip_edges();
	if (old_value == new_value) {
		return;
	}

	Dictionary new_map = old_map.duplicate();
	_erase_animation_state_key(new_map, p_key);
	if (!new_value.is_empty()) {
		new_map[p_key] = new_value;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Animation State");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/animation_state_map", p_block_id), new_map);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/animation_state_map", p_block_id), old_map);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_animation_state_focus_exited(LineEdit *p_line_edit, int p_block_id, const StringName &p_key) {
	if (p_line_edit == nullptr) {
		return;
	}
	_animation_state_submitted(p_line_edit->get_text(), p_block_id, p_key);
}

void VoxelBlockRegistryEditorDialog::_solid_toggled(bool p_pressed, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Solid");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/solid", p_block_id), p_pressed);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/solid", p_block_id), registry->get_block_solid(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_transparent_toggled(bool p_pressed, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Transparent");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/transparent", p_block_id), p_pressed);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/transparent", p_block_id), registry->get_block_transparent(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_color_changed(const Color &p_color, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Color", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/color", p_block_id), p_color);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/color", p_block_id), registry->get_block_color(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_light_opacity_changed(double p_value, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Light Opacity", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/light_opacity", p_block_id), (int)p_value);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/light_opacity", p_block_id), registry->get_block_light_opacity(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_emission_changed(double p_value, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Emission", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/emission", p_block_id), (int)p_value);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/emission", p_block_id), registry->get_block_emission(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_light_color_changed(const Color &p_color, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Light Color", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/light_color", p_block_id), p_color);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/light_color", p_block_id), registry->get_block_light_color(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_breakable_toggled(bool p_pressed, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Breakable");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/breakable", p_block_id), p_pressed);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/breakable", p_block_id), registry->get_block_breakable(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_requires_tool_toggled(bool p_pressed, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Requires Tool");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/requires_tool", p_block_id), p_pressed);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/requires_tool", p_block_id), registry->get_block_requires_tool(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_hand_break_time_changed(double p_value, int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Block Hand Break Time", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/hand_break_time", p_block_id), (float)p_value);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/hand_break_time", p_block_id), registry->get_block_hand_break_time(p_block_id));
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_tool_entry_add_pressed(int p_block_id) {
	if (registry.is_null()) {
		return;
	}
	int new_idx = registry->get_block_tool_entry_count(p_block_id);
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Add Tool Entry");
	undo_redo->add_do_method(registry.ptr(), "add_block_tool_entry", p_block_id, StringName(""), 0, 1.0f);
	undo_redo->add_undo_method(registry.ptr(), "remove_block_tool_entry", p_block_id, new_idx);
	undo_redo->commit_action();
	callable_mp(this, &VoxelBlockRegistryEditorDialog::_rebuild_block_list).call_deferred();
}

void VoxelBlockRegistryEditorDialog::_tool_entry_remove_pressed(int p_block_id, int p_entry_idx) {
	if (registry.is_null()) {
		return;
	}
	Array entries = registry->get_block_tool_entries(p_block_id);
	if (p_entry_idx < 0 || p_entry_idx >= (int)entries.size()) {
		return;
	}
	Dictionary entry_data = entries[p_entry_idx];
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Remove Tool Entry");
	undo_redo->add_do_method(registry.ptr(), "remove_block_tool_entry", p_block_id, p_entry_idx);
	undo_redo->add_undo_method(registry.ptr(), "add_block_tool_entry", p_block_id,
			StringName(String(entry_data["tool_type"])), (int)entry_data["min_tier"], (float)entry_data["break_time"]);
	undo_redo->commit_action();
	callable_mp(this, &VoxelBlockRegistryEditorDialog::_rebuild_block_list).call_deferred();
}

void VoxelBlockRegistryEditorDialog::_tool_entry_type_submitted(const String &p_text, int p_block_id, int p_entry_idx) {
	if (registry.is_null()) {
		return;
	}
	Array entries = registry->get_block_tool_entries(p_block_id);
	if (p_entry_idx < 0 || p_entry_idx >= (int)entries.size()) {
		return;
	}
	Dictionary entry_dict = entries[p_entry_idx];
	String old_type = entry_dict["tool_type"];
	if (old_type == p_text) {
		return;
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Tool Entry Type");
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/tool_entry/%d/tool_type", p_block_id, p_entry_idx), p_text);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/tool_entry/%d/tool_type", p_block_id, p_entry_idx), old_type);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_tool_entry_tier_changed(double p_value, int p_block_id, int p_entry_idx) {
	if (registry.is_null()) {
		return;
	}
	Array entries = registry->get_block_tool_entries(p_block_id);
	if (p_entry_idx < 0 || p_entry_idx >= (int)entries.size()) {
		return;
	}
	Dictionary entry_dict = entries[p_entry_idx];
	int old_tier = entry_dict["min_tier"];
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Tool Entry Min Tier", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/tool_entry/%d/min_tier", p_block_id, p_entry_idx), (int)p_value);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/tool_entry/%d/min_tier", p_block_id, p_entry_idx), old_tier);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_tool_entry_time_changed(double p_value, int p_block_id, int p_entry_idx) {
	if (registry.is_null()) {
		return;
	}
	Array entries = registry->get_block_tool_entries(p_block_id);
	if (p_entry_idx < 0 || p_entry_idx >= (int)entries.size()) {
		return;
	}
	Dictionary entry_dict = entries[p_entry_idx];
	float old_time = entry_dict["break_time"];
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action("Set Tool Entry Break Time", UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(registry.ptr(), vformat("block/%d/tool_entry/%d/break_time", p_block_id, p_entry_idx), (float)p_value);
	undo_redo->add_undo_property(registry.ptr(), vformat("block/%d/tool_entry/%d/break_time", p_block_id, p_entry_idx), old_time);
	undo_redo->commit_action();
}

void VoxelBlockRegistryEditorDialog::_rebuild_block_list() {
	// Clear.
	while (block_list_vbox->get_child_count() > 0) {
		Node *child = block_list_vbox->get_child(0);
		block_list_vbox->remove_child(child);
		memdelete(child);
	}
	block_rows.clear();

	if (registry.is_null()) {
		return;
	}

	// Ensure all engine-defined block types have entries in the registry.
	// Always call setup_defaults() — it merges names and physics/lighting
	// defaults into existing entries without overwriting textures from .tscn.
	registry->setup_defaults();
	int block_count = registry->get_block_count();

	// Always show all registered block types (skip AIR=0).
	for (int block_id = 1; block_id < block_count; block_id++) {

		// --- Block header: color swatch + name ---
		HBoxContainer *header = memnew(HBoxContainer);
		header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(header);

		ColorRect *swatch = memnew(ColorRect);
		String block_name = registry->get_block_name(block_id);
		if (block_name.is_empty()) {
			block_name = "Block #" + itos(block_id);
		}
		Color block_color = registry->has_block(block_id) ? registry->get_block_color(block_id) : Color(1, 1, 1);
		swatch->set_color(block_color);
		swatch->set_custom_minimum_size(Size2(28 * EDSCALE, 28 * EDSCALE));
		header->add_child(swatch);

		Label *name_label = memnew(Label);
		name_label->set_text(vformat("[%d] %s", block_id, block_name));
		name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		header->add_child(name_label);

		// --- Three texture pickers (Top / Side / Bottom) ---
		HBoxContainer *picker_row = memnew(HBoxContainer);
		picker_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(picker_row);

		BlockRow row;
		row.block_id = block_id;

		const char *face_names[3] = { "Top", "Side", "Bottom" };
		EditorResourcePicker **pickers[3] = { &row.picker_top, &row.picker_side, &row.picker_bottom };

		for (int f = 0; f < 3; f++) {
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			picker_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text(face_names[f]);
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			EditorResourcePicker *picker = memnew(EditorResourcePicker);
			picker->set_base_type("Texture2D");
			picker->set_editable(true);
			picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);

			Ref<Texture2D> current_tex;
			if (registry->has_block(block_id)) {
				if (f == 0) {
					current_tex = registry->get_block_texture_top(block_id);
				} else if (f == 1) {
					current_tex = registry->get_block_texture_side(block_id);
				} else {
					current_tex = registry->get_block_texture_bottom(block_id);
				}
			}
			if (current_tex.is_valid()) {
				picker->set_edited_resource(current_tex);
			}

			picker->connect("resource_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_texture_changed).bind(block_id, f));
			slot->add_child(picker);
			*pickers[f] = picker;
		}

		// --- Shader material picker ---
		HBoxContainer *shader_row = memnew(HBoxContainer);
		shader_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(shader_row);

		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			shader_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text("Shader Material");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			EditorResourcePicker *picker = memnew(EditorResourcePicker);
			picker->set_base_type("ShaderMaterial");
			picker->set_editable(true);
			picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);

			if (registry->has_block(block_id)) {
				Ref<ShaderMaterial> current_shader = registry->get_block_shader_material(block_id);
				if (current_shader.is_valid()) {
					picker->set_edited_resource(current_shader);
				}
			}

			picker->connect("resource_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_shader_changed).bind(block_id));
			slot->add_child(picker);
			row.picker_shader = picker;
		}

		// --- Model visual section ---
		HBoxContainer *visual_header_row = memnew(HBoxContainer);
		visual_header_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(visual_header_row);

		{
			Label *lbl = memnew(Label);
			lbl->set_text("Visual:");
			lbl->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			visual_header_row->add_child(lbl);
		}

		const VoxelBlockRegistry::VisualMode visual_mode = registry->has_block(block_id) ? registry->get_block_visual_mode(block_id) : VoxelBlockRegistry::VISUAL_MODE_VOXEL;
		const bool model_mode = visual_mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH || visual_mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE;
		const bool mesh_mode = visual_mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH;
		const bool scene_mode = visual_mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE;

		HBoxContainer *visual_row = memnew(HBoxContainer);
		visual_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(visual_row);

		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			visual_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text("Mode");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			OptionButton *option = memnew(OptionButton);
			option->add_item("Voxel");
			option->add_item("Model Mesh");
			option->add_item("Model Scene");
			option->select((int)visual_mode);
			option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			option->connect("item_selected", callable_mp(this, &VoxelBlockRegistryEditorDialog::_visual_mode_selected).bind(block_id));
			slot->add_child(option);
			row.option_visual_mode = option;
		}

		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			visual_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text("Model Mesh");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			EditorResourcePicker *picker = memnew(EditorResourcePicker);
			picker->set_base_type("Mesh");
			picker->set_editable(mesh_mode);
			picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			if (registry->has_block(block_id)) {
				Ref<Mesh> current_mesh = registry->get_block_model_mesh(block_id);
				if (current_mesh.is_valid()) {
					picker->set_edited_resource(current_mesh);
				}
			}
			picker->connect("resource_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_model_mesh_changed).bind(block_id));
			slot->add_child(picker);
			row.picker_model_mesh = picker;
		}

		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			visual_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text("Model Scene");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			EditorResourcePicker *picker = memnew(EditorResourcePicker);
			picker->set_base_type("PackedScene");
			picker->set_editable(scene_mode);
			picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			if (registry->has_block(block_id)) {
				Ref<PackedScene> current_scene = registry->get_block_model_scene(block_id);
				if (current_scene.is_valid()) {
					picker->set_edited_resource(current_scene);
				}
			}
			picker->connect("resource_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_model_scene_changed).bind(block_id));
			slot->add_child(picker);
			row.picker_model_scene = picker;
		}

		HBoxContainer *transform_row = memnew(HBoxContainer);
		transform_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(transform_row);

		const Vector3 model_offset = registry->has_block(block_id) ? registry->get_block_model_offset(block_id) : Vector3();
		const float model_rotation_y = registry->has_block(block_id) ? registry->get_block_model_rotation_y(block_id) : 0.0f;
		const Vector3 model_scale = registry->has_block(block_id) ? registry->get_block_model_scale(block_id) : Vector3(1, 1, 1);
		const char *axis_names[3] = { "X", "Y", "Z" };

		for (int axis = 0; axis < 3; axis++) {
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			transform_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text(vformat("Offset %s", axis_names[axis]));
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			const double value = axis == 0 ? model_offset.x : (axis == 1 ? model_offset.y : model_offset.z);
			SpinBox *spin = _make_model_spin_box(-1024.0, 1024.0, 0.05, value);
			spin->set_editable(model_mode);
			spin->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_model_offset_axis_changed).bind(block_id, axis));
			slot->add_child(spin);
			row.spin_model_offset[axis] = spin;
		}

		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			transform_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text("Rot Y");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			SpinBox *spin = _make_model_spin_box(-3600.0, 3600.0, 0.1, model_rotation_y);
			spin->set_editable(model_mode);
			spin->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_model_rotation_y_changed).bind(block_id));
			slot->add_child(spin);
			row.spin_model_rotation_y = spin;
		}

		for (int axis = 0; axis < 3; axis++) {
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			transform_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text(vformat("Scale %s", axis_names[axis]));
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			const double value = axis == 0 ? model_scale.x : (axis == 1 ? model_scale.y : model_scale.z);
			SpinBox *spin = _make_model_spin_box(-100.0, 100.0, 0.05, value);
			spin->set_editable(model_mode);
			spin->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_model_scale_axis_changed).bind(block_id, axis));
			slot->add_child(spin);
			row.spin_model_scale[axis] = spin;
		}

		HBoxContainer *animation_row = memnew(HBoxContainer);
		animation_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(animation_row);

		Dictionary animation_map = registry->has_block(block_id) ? registry->get_block_animation_state_map(block_id) : Dictionary();
		const StringName animation_keys[3] = { StringName("idle"), StringName("active"), StringName("open") };
		const char *animation_labels[3] = { "Idle", "Active", "Open" };
		for (int i = 0; i < 3; i++) {
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			animation_row->add_child(slot);

			Label *lbl = memnew(Label);
			lbl->set_text(vformat("%s Animation", animation_labels[i]));
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);

			LineEdit *edit = memnew(LineEdit);
			edit->set_text(_get_animation_state_value(animation_map, animation_keys[i]));
			edit->set_placeholder(animation_labels[i]);
			edit->set_editable(scene_mode);
			edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			edit->connect("text_submitted", callable_mp(this, &VoxelBlockRegistryEditorDialog::_animation_state_submitted).bind(block_id, animation_keys[i]));
			edit->connect("focus_exited", callable_mp(this, &VoxelBlockRegistryEditorDialog::_animation_state_focus_exited).bind(edit, block_id, animation_keys[i]));
			slot->add_child(edit);

			if (i == 0) {
				row.edit_animation_idle = edit;
			} else if (i == 1) {
				row.edit_animation_active = edit;
			} else {
				row.edit_animation_open = edit;
			}
		}

		// --- Physics & Lighting section ---
		HBoxContainer *phys_row = memnew(HBoxContainer);
		phys_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(phys_row);

		// Solid
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Solid");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			CheckBox *cb = memnew(CheckBox);
			cb->set_pressed(registry->has_block(block_id) ? registry->get_block_solid(block_id) : true);
			cb->connect("toggled", callable_mp(this, &VoxelBlockRegistryEditorDialog::_solid_toggled).bind(block_id));
			slot->add_child(cb);
			row.check_solid = cb;
		}

		// Transparent
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Transparent");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			CheckBox *cb = memnew(CheckBox);
			cb->set_pressed(registry->has_block(block_id) ? registry->get_block_transparent(block_id) : false);
			cb->connect("toggled", callable_mp(this, &VoxelBlockRegistryEditorDialog::_transparent_toggled).bind(block_id));
			slot->add_child(cb);
			row.check_transparent = cb;
		}

		// Vertex Color
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Vertex Color");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			ColorPickerButton *cpb = memnew(ColorPickerButton);
			cpb->set_pick_color(registry->has_block(block_id) ? registry->get_block_color(block_id) : Color(1, 1, 1));
			cpb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			cpb->connect("color_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_color_changed).bind(block_id));
			slot->add_child(cpb);
			row.picker_color = cpb;
		}

		// Light Opacity
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Light Op.");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			SpinBox *sp = memnew(SpinBox);
			sp->set_min(0);
			sp->set_max(15);
			sp->set_step(1);
			sp->set_value(registry->has_block(block_id) ? registry->get_block_light_opacity(block_id) : 15);
			sp->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_light_opacity_changed).bind(block_id));
			slot->add_child(sp);
			row.spin_opacity = sp;
		}

		// Emission
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Emission");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			SpinBox *sp = memnew(SpinBox);
			sp->set_min(0);
			sp->set_max(15);
			sp->set_step(1);
			sp->set_value(registry->has_block(block_id) ? registry->get_block_emission(block_id) : 0);
			sp->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_emission_changed).bind(block_id));
			slot->add_child(sp);
			row.spin_emission = sp;
		}

		// Light Color
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			phys_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Light Color");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			ColorPickerButton *cpb = memnew(ColorPickerButton);
			cpb->set_pick_color(registry->has_block(block_id) ? registry->get_block_light_color(block_id) : Color(1, 1, 1));
			cpb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			cpb->connect("color_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_light_color_changed).bind(block_id));
			slot->add_child(cpb);
			row.picker_light_color = cpb;
		}

		// --- Mining / survival section ---
		HBoxContainer *mining_row = memnew(HBoxContainer);
		mining_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(mining_row);

		// Breakable
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			mining_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Breakable");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			CheckBox *cb = memnew(CheckBox);
			cb->set_pressed(registry->has_block(block_id) ? registry->get_block_breakable(block_id) : true);
			cb->connect("toggled", callable_mp(this, &VoxelBlockRegistryEditorDialog::_breakable_toggled).bind(block_id));
			slot->add_child(cb);
			row.check_breakable = cb;
		}

		// Requires Tool
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			mining_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Req. Tool");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			CheckBox *cb = memnew(CheckBox);
			cb->set_pressed(registry->has_block(block_id) ? registry->get_block_requires_tool(block_id) : false);
			cb->connect("toggled", callable_mp(this, &VoxelBlockRegistryEditorDialog::_requires_tool_toggled).bind(block_id));
			slot->add_child(cb);
			row.check_requires_tool = cb;
		}

		// Hand Break Time
		{
			VBoxContainer *slot = memnew(VBoxContainer);
			slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			mining_row->add_child(slot);
			Label *lbl = memnew(Label);
			lbl->set_text("Hand Break (s)");
			lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			slot->add_child(lbl);
			SpinBox *sp = memnew(SpinBox);
			sp->set_min(0.0);
			sp->set_max(300.0);
			sp->set_step(0.1);
			sp->set_value(registry->has_block(block_id) ? registry->get_block_hand_break_time(block_id) : 5.0);
			sp->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_hand_break_time_changed).bind(block_id));
			slot->add_child(sp);
			row.spin_hand_break_time = sp;
		}

		// --- Tool Entries section ---
		HBoxContainer *entries_header_row = memnew(HBoxContainer);
		entries_header_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(entries_header_row);

		{
			Label *lbl = memnew(Label);
			lbl->set_text("Tool Entries:");
			lbl->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			entries_header_row->add_child(lbl);

			Button *add_btn = memnew(Button);
			add_btn->set_text("+");
			add_btn->set_tooltip_text("Add tool entry");
			add_btn->connect(SceneStringName(pressed), callable_mp(this, &VoxelBlockRegistryEditorDialog::_tool_entry_add_pressed).bind(block_id));
			entries_header_row->add_child(add_btn);
		}

		int entry_count = registry->get_block_tool_entry_count(block_id);
		Array cur_entries = registry->get_block_tool_entries(block_id);
		for (int j = 0; j < entry_count; j++) {
			Dictionary entry_dict = cur_entries[j];

			HBoxContainer *entry_row = memnew(HBoxContainer);
			entry_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			block_list_vbox->add_child(entry_row);

			// Tool type — LineEdit (press Enter to confirm)
			{
				VBoxContainer *slot = memnew(VBoxContainer);
				slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				entry_row->add_child(slot);
				Label *lbl = memnew(Label);
				lbl->set_text("Tool Type");
				lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
				slot->add_child(lbl);
				LineEdit *le = memnew(LineEdit);
				le->set_text(entry_dict["tool_type"]);
				le->set_placeholder("e.g. pickaxe");
				le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				le->connect("text_submitted", callable_mp(this, &VoxelBlockRegistryEditorDialog::_tool_entry_type_submitted).bind(block_id, j));
				slot->add_child(le);
			}

			// Min tier
			{
				VBoxContainer *slot = memnew(VBoxContainer);
				entry_row->add_child(slot);
				Label *lbl = memnew(Label);
				lbl->set_text("Min Tier");
				lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
				slot->add_child(lbl);
				SpinBox *sp = memnew(SpinBox);
				sp->set_min(0);
				sp->set_max(100);
				sp->set_step(1);
				sp->set_value((int)entry_dict["min_tier"]);
				sp->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_tool_entry_tier_changed).bind(block_id, j));
				slot->add_child(sp);
			}

			// Break time
			{
				VBoxContainer *slot = memnew(VBoxContainer);
				slot->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				entry_row->add_child(slot);
				Label *lbl = memnew(Label);
				lbl->set_text("Break Time (s)");
				lbl->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
				slot->add_child(lbl);
				SpinBox *sp = memnew(SpinBox);
				sp->set_min(0.0);
				sp->set_max(300.0);
				sp->set_step(0.1);
				sp->set_value((float)entry_dict["break_time"]);
				sp->connect("value_changed", callable_mp(this, &VoxelBlockRegistryEditorDialog::_tool_entry_time_changed).bind(block_id, j));
				slot->add_child(sp);
			}

			// Remove button
			{
				VBoxContainer *slot = memnew(VBoxContainer);
				entry_row->add_child(slot);
				Label *spacer = memnew(Label);
				spacer->set_text(" ");
				slot->add_child(spacer);
				Button *rm_btn = memnew(Button);
				rm_btn->set_text("X");
				rm_btn->set_tooltip_text("Remove this entry");
				rm_btn->connect(SceneStringName(pressed), callable_mp(this, &VoxelBlockRegistryEditorDialog::_tool_entry_remove_pressed).bind(block_id, j));
				slot->add_child(rm_btn);
			}
		}

		block_rows.push_back(row);
		block_list_vbox->add_child(memnew(HSeparator));
	}
}

// ============================================================
// VoxelBlockRegistryInspectorButton
// ============================================================

VoxelBlockRegistryInspectorButton::VoxelBlockRegistryInspectorButton() {
	dialog = memnew(VoxelBlockRegistryEditorDialog);
	add_child(dialog);

	edit_button = memnew(Button);
	edit_button->set_text("Edit Block Visuals...");
	edit_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	edit_button->set_custom_minimum_size(Size2(0, 36 * EDSCALE));
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &VoxelBlockRegistryInspectorButton::_on_edit_pressed));
	add_child(edit_button);
}

void VoxelBlockRegistryInspectorButton::set_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	registry = p_registry;
	dialog->set_registry(p_registry);

	// Show block count on button.
	if (p_registry.is_valid()) {
		int count = p_registry->get_block_count();
		edit_button->set_text(vformat("Edit Block Visuals (%d blocks)...", count));
	}
}

void VoxelBlockRegistryInspectorButton::_on_edit_pressed() {
	if (registry.is_valid()) {
		// Refresh in case registry was changed externally.
		dialog->set_registry(registry);
	}
	dialog->popup_centered_ratio(0.6);
}

// ============================================================
// EditorInspectorPluginVoxelBlockRegistry
// ============================================================

bool EditorInspectorPluginVoxelBlockRegistry::can_handle(Object *p_object) {
	return Object::cast_to<VoxelBlockRegistry>(p_object) != nullptr;
}

void EditorInspectorPluginVoxelBlockRegistry::parse_begin(Object *p_object) {
	VoxelBlockRegistry *reg = Object::cast_to<VoxelBlockRegistry>(p_object);
	if (!reg) {
		return;
	}

	VoxelBlockRegistryInspectorButton *btn = memnew(VoxelBlockRegistryInspectorButton);
	btn->set_registry(Ref<VoxelBlockRegistry>(reg));
	add_custom_control(btn);
}

bool EditorInspectorPluginVoxelBlockRegistry::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path,
		const PropertyHint p_hint, const String &p_hint_text,
		const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	// Hide raw block/* properties — our dialog handles them.
	if (p_path.begins_with("block/")) {
		return true;
	}
	return false;
}

// ============================================================
// VoxelBlockRegistryEditorPlugin
// ============================================================

VoxelBlockRegistryEditorPlugin::VoxelBlockRegistryEditorPlugin() {
	Ref<EditorInspectorPluginVoxelBlockRegistry> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
