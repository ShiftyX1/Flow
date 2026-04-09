#pragma once

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/scroll_container.h"

#include "../voxel_block_registry.h"

class EditorResourcePicker;
class ColorRect;
class Label;
class Button;

// Popup dialog with the full block texture editor.
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
	};

	Vector<BlockRow> block_rows;

	void _rebuild_block_list();
	void _texture_changed(const Ref<Resource> &p_resource, int p_block_id, int p_face);
	void _shader_changed(const Ref<Resource> &p_resource, int p_block_id);

public:
	void set_registry(const Ref<VoxelBlockRegistry> &p_registry);
	VoxelBlockRegistryEditorDialog();
};

// Small widget in the inspector: shows summary + "Edit Textures" button.
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
