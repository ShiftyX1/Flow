#include "voxel_block_registry_editor_plugin.h"

#include "../voxel_block_data.h"
#include "../voxel_block_registry.h"

#include "core/object/callable_mp.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/scene_string_names.h"

// ============================================================
// VoxelBlockRegistryEditorDialog
// ============================================================

VoxelBlockRegistryEditorDialog::VoxelBlockRegistryEditorDialog() {
	set_title("Voxel Block Registry — Texture Editor");
	set_min_size(Size2(700 * EDSCALE, 500 * EDSCALE));

	scroll = memnew(ScrollContainer);
	scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	add_child(scroll);

	block_list_vbox = memnew(VBoxContainer);
	block_list_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	scroll->add_child(block_list_vbox);
}

void VoxelBlockRegistryEditorDialog::set_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	registry = p_registry;
	_rebuild_block_list();
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

	PackedInt32Array ids = registry->get_block_list();
	Vector<int> sorted_ids;
	for (int i = 0; i < ids.size(); i++) {
		sorted_ids.push_back(ids[i]);
	}
	sorted_ids.sort();

	for (int i = 0; i < sorted_ids.size(); i++) {
		int block_id = sorted_ids[i];

		// Skip AIR.
		if (block_id == 0) {
			continue;
		}

		// --- Block header: color swatch + name ---
		HBoxContainer *header = memnew(HBoxContainer);
		header->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		block_list_vbox->add_child(header);

		ColorRect *swatch = memnew(ColorRect);
		Color block_color;
		String block_name;
		if (block_id >= 0 && block_id < VOXEL_BLOCK_TYPE_MAX) {
			block_color = VoxelBlockData::block_colors[block_id];
			block_name = VoxelBlockData::get_block_name((VoxelBlockType)block_id);
		} else {
			block_color = Color(1, 1, 1);
			block_name = "Custom #" + itos(block_id);
		}
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
	edit_button->set_text("Edit Block Textures...");
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
		edit_button->set_text(vformat("Edit Block Textures (%d blocks)...", count));
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
