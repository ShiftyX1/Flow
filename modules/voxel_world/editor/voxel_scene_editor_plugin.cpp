#include "voxel_scene_editor_plugin.h"

#include "../voxel_mesher.h"

#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/image.h"
#include "core/object/callable_mp.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/separator.h"
#include "scene/main/window.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

// =============================================================================
// VoxelSceneGizmoPlugin
// =============================================================================

VoxelSceneGizmoPlugin::VoxelSceneGizmoPlugin() {
	create_material("vs_aabb", Color(0.4, 0.85, 1.0, 0.9));
	create_material("vs_hover", Color(1.0, 0.95, 0.2, 1.0));
	create_material("vs_place", Color(0.3, 1.0, 0.3, 1.0));
	create_material("vs_box_first", Color(1.0, 0.4, 0.7, 1.0));
}

bool VoxelSceneGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<VoxelScene>(p_spatial) != nullptr;
}

static void _add_aabb_lines(Vector<Vector3> &lines, const Vector3 &min_pt, const Vector3 &max_pt) {
	Vector3 corners[8] = {
		Vector3(min_pt.x, min_pt.y, min_pt.z), Vector3(max_pt.x, min_pt.y, min_pt.z),
		Vector3(max_pt.x, min_pt.y, max_pt.z), Vector3(min_pt.x, min_pt.y, max_pt.z),
		Vector3(min_pt.x, max_pt.y, min_pt.z), Vector3(max_pt.x, max_pt.y, min_pt.z),
		Vector3(max_pt.x, max_pt.y, max_pt.z), Vector3(min_pt.x, max_pt.y, max_pt.z),
	};
	int edges[12][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};
	for (int i = 0; i < 12; i++) {
		lines.push_back(corners[edges[i][0]]);
		lines.push_back(corners[edges[i][1]]);
	}
}

void VoxelSceneGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();
	VoxelScene *scene = Object::cast_to<VoxelScene>(p_gizmo->get_node_3d());
	if (!scene) {
		return;
	}
	Vector3i size = scene->get_size();
	if (size.x <= 0 || size.y <= 0 || size.z <= 0) {
		return;
	}
	float bs = scene->get_block_size();
	Vector3 min_pt(0, 0, 0);
	Vector3 max_pt(size.x * bs, size.y * bs, size.z * bs);

	Vector<Vector3> aabb_lines;
	_add_aabb_lines(aabb_lines, min_pt, max_pt);
	p_gizmo->add_lines(aabb_lines, get_material("vs_aabb", p_gizmo));

	if (!editor_plugin || editor_plugin->get_current_scene() != scene) {
		return;
	}

	Vector3i hover_cell, hover_normal, place_cell;
	bool block_hit = false;
	if (editor_plugin->get_hover(hover_cell, hover_normal, place_cell, block_hit)) {
		if (block_hit) {
			Vector3 c0(hover_cell.x * bs, hover_cell.y * bs, hover_cell.z * bs);
			Vector3 c1 = c0 + Vector3(bs, bs, bs);
			Vector<Vector3> hover_lines;
			_add_aabb_lines(hover_lines, c0, c1);
			p_gizmo->add_lines(hover_lines, get_material("vs_hover", p_gizmo));
		}

		// Draw the placement cell only if it's inside bounds and currently empty.
		if (place_cell.x >= 0 && place_cell.y >= 0 && place_cell.z >= 0 &&
				place_cell.x < size.x && place_cell.y < size.y && place_cell.z < size.z &&
				scene->get_block_v(place_cell) == 0 && (!block_hit || place_cell != hover_cell)) {
			Vector3 p0(place_cell.x * bs, place_cell.y * bs, place_cell.z * bs);
			Vector3 p1 = p0 + Vector3(bs, bs, bs);
			Vector<Vector3> place_lines;
			_add_aabb_lines(place_lines, p0, p1);
			p_gizmo->add_lines(place_lines, get_material("vs_place", p_gizmo));
		}
	}

	Vector3i box_first;
	if (editor_plugin->get_box_first(box_first)) {
		Vector3 c0(box_first.x * bs, box_first.y * bs, box_first.z * bs);
		Vector3 c1 = c0 + Vector3(bs, bs, bs);
		Vector<Vector3> bf_lines;
		_add_aabb_lines(bf_lines, c0, c1);
		p_gizmo->add_lines(bf_lines, get_material("vs_box_first", p_gizmo));
	}
}

// =============================================================================
// VoxelSceneToolbar
// =============================================================================

VoxelSceneToolbar::VoxelSceneToolbar() {
	const char *labels[TOOL_MAX] = { "Paint", "Erase", "Pick", "Box Fill", "Box Erase" };
	for (int i = 0; i < TOOL_MAX; i++) {
		Button *b = memnew(Button);
		b->set_text(labels[i]);
		b->set_toggle_mode(true);
		b->set_focus_mode(Control::FOCUS_NONE);
		if (i == TOOL_PAINT) {
			b->set_pressed(true);
		}
		b->connect("pressed", callable_mp(this, &VoxelSceneToolbar::_on_tool_pressed).bind(i));
		add_child(b);
		tool_buttons[i] = b;
	}

	add_child(memnew(VSeparator));

	resize_button = memnew(Button);
	resize_button->set_text("Resize\xE2\x80\xA6"); // "Resize…"
	resize_button->set_focus_mode(Control::FOCUS_NONE);
	resize_button->connect("pressed", callable_mp(this, &VoxelSceneToolbar::_on_resize_pressed));
	add_child(resize_button);

	add_child(memnew(VSeparator));

	status_label = memnew(Label);
	status_label->set_text("");
	status_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	add_child(status_label);
}

void VoxelSceneToolbar::_notification(int p_what) {
	(void)p_what;
}

void VoxelSceneToolbar::_on_tool_pressed(int p_tool) {
	current_tool = (Tool)p_tool;
	for (int i = 0; i < TOOL_MAX; i++) {
		if (tool_buttons[i]) {
			tool_buttons[i]->set_pressed_no_signal(i == p_tool);
		}
	}
}

void VoxelSceneToolbar::_on_resize_pressed() {
	if (editor_plugin) {
		editor_plugin->open_resize_dialog();
	}
}

void VoxelSceneToolbar::set_status_text(const String &p_text) {
	if (status_label) {
		status_label->set_text(p_text);
	}
}

// =============================================================================
// VoxelScenePaletteDock
// =============================================================================

VoxelScenePaletteDock::VoxelScenePaletteDock() {
	set_name("VoxelScene Palette");
	set_v_size_flags(Control::SIZE_EXPAND_FILL);

	Label *title = memnew(Label);
	title->set_text("VoxelScene Palette");
	add_child(title);

	info_label = memnew(Label);
	info_label->set_text("(no scene selected)");
	add_child(info_label);

	block_list = memnew(ItemList);
	block_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	block_list->set_max_columns(0); // wrap as grid
	block_list->set_max_text_lines(2);
	block_list->set_icon_mode(ItemList::ICON_MODE_TOP);
	block_list->set_fixed_icon_size(Size2(48, 48));
	block_list->set_same_column_width(true);
	block_list->set_select_mode(ItemList::SELECT_SINGLE);
	block_list->connect("item_selected", callable_mp(this, &VoxelScenePaletteDock::_on_item_selected));
	add_child(block_list);
}

VoxelScenePaletteDock::~VoxelScenePaletteDock() {
	if (registry.is_valid() && registry->is_connected(SNAME("changed"), callable_mp(this, &VoxelScenePaletteDock::_on_registry_changed))) {
		registry->disconnect(SNAME("changed"), callable_mp(this, &VoxelScenePaletteDock::_on_registry_changed));
	}
}

void VoxelScenePaletteDock::set_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	if (registry == p_registry) {
		return;
	}
	if (registry.is_valid() && registry->is_connected(SNAME("changed"), callable_mp(this, &VoxelScenePaletteDock::_on_registry_changed))) {
		registry->disconnect(SNAME("changed"), callable_mp(this, &VoxelScenePaletteDock::_on_registry_changed));
	}
	registry = p_registry;
	if (registry.is_valid()) {
		registry->connect(SNAME("changed"), callable_mp(this, &VoxelScenePaletteDock::_on_registry_changed));
	}
	_rebuild();
}

void VoxelScenePaletteDock::_on_registry_changed() {
	_rebuild();
}

void VoxelScenePaletteDock::_on_item_selected(int p_idx) {
	int id = block_list->get_item_metadata(p_idx);
	selected_block_id = id;
}

void VoxelScenePaletteDock::set_selected_block_id(int p_id) {
	selected_block_id = p_id;
	if (!block_list) {
		return;
	}
	for (int i = 0; i < block_list->get_item_count(); i++) {
		if ((int)block_list->get_item_metadata(i) == p_id) {
			block_list->select(i);
			break;
		}
	}
}

// Build a 16x16 solid-color icon as fallback when a block has no top texture.
static Ref<Texture2D> _make_color_icon(const Color &p_color) {
	const int W = 16;
	Vector<uint8_t> pixels;
	pixels.resize(W * W * 4);
	uint8_t r = (uint8_t)CLAMP(p_color.r * 255.0f, 0.0f, 255.0f);
	uint8_t g = (uint8_t)CLAMP(p_color.g * 255.0f, 0.0f, 255.0f);
	uint8_t b = (uint8_t)CLAMP(p_color.b * 255.0f, 0.0f, 255.0f);
	uint8_t *w = pixels.ptrw();
	for (int i = 0; i < W * W; i++) {
		w[i * 4 + 0] = r;
		w[i * 4 + 1] = g;
		w[i * 4 + 2] = b;
		w[i * 4 + 3] = 255;
	}
	Ref<Image> img = Image::create_from_data(W, W, false, Image::FORMAT_RGBA8, pixels);
	return ImageTexture::create_from_image(img);
}

void VoxelScenePaletteDock::_rebuild() {
	if (!block_list) {
		return;
	}
	block_list->clear();
	if (registry.is_null()) {
		info_label->set_text("(no block registry assigned)");
		return;
	}
	if (!registry->is_finalized()) {
		registry->finalize();
	}
	PackedInt32Array ids = registry->get_block_list();
	int added = 0;
	for (int i = 0; i < ids.size(); i++) {
		int id = ids[i];
		if (id == 0) {
			continue; // skip air
		}
		String name = registry->get_block_name(id);
		Ref<Texture2D> tex = registry->get_block_texture_for_face(id, Vector3(0, 1, 0));
		if (tex.is_null()) {
			tex = _make_color_icon(registry->get_block_color(id));
		}
		int item_idx = block_list->add_item(vformat("%s\n#%d", name, id), tex);
		block_list->set_item_metadata(item_idx, id);
		added++;
	}
	info_label->set_text(vformat("%d block(s)", added));
	// Try to keep selection.
	set_selected_block_id(selected_block_id);
}

// =============================================================================
// VoxelSceneResizeDialog
// =============================================================================

VoxelSceneResizeDialog::VoxelSceneResizeDialog() {
	set_title("Resize VoxelScene");
	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	auto add_row = [&](const String &label, SpinBox **out) {
		HBoxContainer *hb = memnew(HBoxContainer);
		Label *l = memnew(Label);
		l->set_text(label);
		l->set_custom_minimum_size(Size2(40, 0));
		hb->add_child(l);
		SpinBox *sb = memnew(SpinBox);
		sb->set_min(1);
		sb->set_max(1024);
		sb->set_step(1);
		sb->set_value(64);
		hb->add_child(sb);
		vb->add_child(hb);
		*out = sb;
	};
	add_row("X:", &spin_x);
	add_row("Y:", &spin_y);
	add_row("Z:", &spin_z);

	get_ok_button()->set_text("Apply");
	connect("confirmed", callable_mp(this, &VoxelSceneResizeDialog::_on_confirmed));
}

void VoxelSceneResizeDialog::open_with_size(const Vector3i &p_size) {
	spin_x->set_value(p_size.x);
	spin_y->set_value(p_size.y);
	spin_z->set_value(p_size.z);
	popup_centered(Size2(260, 0));
}

void VoxelSceneResizeDialog::_on_confirmed() {
	if (editor_plugin) {
		editor_plugin->apply_resize(Vector3i((int)spin_x->get_value(), (int)spin_y->get_value(), (int)spin_z->get_value()));
	}
}

// =============================================================================
// VoxelSceneEditorPlugin
// =============================================================================

VoxelSceneEditorPlugin::VoxelSceneEditorPlugin() {
	toolbar = memnew(VoxelSceneToolbar);
	toolbar->set_editor_plugin(this);
	toolbar->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);

	palette = memnew(VoxelScenePaletteDock);
	palette->set_editor_plugin(this);
	palette->hide();
	add_control_to_dock(DOCK_SLOT_RIGHT_BR, palette);

	resize_dialog = memnew(VoxelSceneResizeDialog);
	resize_dialog->set_editor_plugin(this);
	EditorNode::get_singleton()->get_gui_base()->add_child(resize_dialog);

	gizmo_plugin.instantiate();
	gizmo_plugin->set_editor_plugin(this);
	add_node_3d_gizmo_plugin(gizmo_plugin);
}

VoxelSceneEditorPlugin::~VoxelSceneEditorPlugin() {
	if (gizmo_plugin.is_valid()) {
		remove_node_3d_gizmo_plugin(gizmo_plugin);
	}
	if (toolbar) {
		remove_control_from_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);
		memdelete(toolbar);
		toolbar = nullptr;
	}
	if (palette) {
		remove_control_from_docks(palette);
		memdelete(palette);
		palette = nullptr;
	}
	if (resize_dialog) {
		resize_dialog->queue_free();
		resize_dialog = nullptr;
	}
}

bool VoxelSceneEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<VoxelScene>(p_object) != nullptr;
}

void VoxelSceneEditorPlugin::edit(Object *p_object) {
	current_scene = Object::cast_to<VoxelScene>(p_object);
	has_hover = false;
	box_first_set = false;
	if (current_scene) {
		palette->set_registry(current_scene->get_block_registry());
	} else {
		palette->set_registry(Ref<VoxelBlockRegistry>());
	}
	update_gizmo_for_current();
}

void VoxelSceneEditorPlugin::make_visible(bool p_visible) {
	if (toolbar) {
		toolbar->set_visible(p_visible);
	}
	if (palette) {
		palette->set_visible(p_visible);
	}
	if (!p_visible) {
		current_scene = nullptr;
		has_hover = false;
		box_first_set = false;
	}
}

void VoxelSceneEditorPlugin::update_gizmo_for_current() {
	if (current_scene) {
		current_scene->update_gizmos();
	}
}

void VoxelSceneEditorPlugin::open_resize_dialog() {
	if (!current_scene || current_scene->get_voxel_data().is_null()) {
		return;
	}
	resize_dialog->open_with_size(current_scene->get_size());
}

void VoxelSceneEditorPlugin::apply_resize(const Vector3i &p_new_size) {
	if (!current_scene || current_scene->get_voxel_data().is_null()) {
		return;
	}
	Ref<VoxelSceneData> data = current_scene->get_voxel_data();
	Vector3i old_size = data->get_size();
	if (old_size == p_new_size) {
		return;
	}
	PackedByteArray old_blocks = data->get_blocks_packed();
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action("Resize VoxelScene");
	ur->add_do_method(data.ptr(), "set_size", p_new_size);
	ur->add_undo_method(data.ptr(), "set_size", old_size);
	ur->add_undo_method(data.ptr(), "set_blocks_packed", old_blocks);
	ur->commit_action();
}

// ---- DDA voxel raycast in local space ----

VoxelSceneEditorPlugin::VoxelRaycastHit VoxelSceneEditorPlugin::_voxel_raycast(const Vector3 &p_local_origin, const Vector3 &p_local_dir) const {
	VoxelRaycastHit result;
	if (!current_scene || current_scene->get_voxel_data().is_null()) {
		return result;
	}
	Ref<VoxelSceneData> data = current_scene->get_voxel_data();
	Vector3i size = data->get_size();
	float bs = current_scene->get_block_size();
	if (size.x <= 0 || size.y <= 0 || size.z <= 0 || bs <= 0.0f) {
		return result;
	}

	Vector3 dir = p_local_dir;
	if (dir.length_squared() < 1e-12f) {
		return result;
	}
	dir.normalize();

	// Convert to "voxel space" where 1 unit = 1 cell.
	Vector3 ro = p_local_origin / bs;
	Vector3 rd = dir; // direction direction is unitless; only ratios matter for stepping.

	// Ray-AABB intersection vs [0, size].
	Vector3 box_min(0, 0, 0);
	Vector3 box_max(size.x, size.y, size.z);
	float tmin = 0.0f;
	float tmax = 1e20f;
	for (int a = 0; a < 3; a++) {
		float o = ro[a];
		float d = rd[a];
		float lo = box_min[a];
		float hi = box_max[a];
		if (Math::abs(d) < 1e-8f) {
			if (o < lo || o > hi) {
				return result;
			}
		} else {
			float t1 = (lo - o) / d;
			float t2 = (hi - o) / d;
			if (t1 > t2) {
				SWAP(t1, t2);
			}
			tmin = MAX(tmin, t1);
			tmax = MIN(tmax, t2);
			if (tmax < tmin) {
				return result;
			}
		}
	}

	// Start at the entry point (slightly inside the box).
	float t_start = MAX(tmin, 0.0f);
	Vector3 p_entry = ro + rd * t_start;
	// Initial cell.
	int x = (int)Math::floor(p_entry.x);
	int y = (int)Math::floor(p_entry.y);
	int z = (int)Math::floor(p_entry.z);
	x = CLAMP(x, 0, size.x - 1);
	y = CLAMP(y, 0, size.y - 1);
	z = CLAMP(z, 0, size.z - 1);

	int step_x = (rd.x > 0) ? 1 : ((rd.x < 0) ? -1 : 0);
	int step_y = (rd.y > 0) ? 1 : ((rd.y < 0) ? -1 : 0);
	int step_z = (rd.z > 0) ? 1 : ((rd.z < 0) ? -1 : 0);

	auto next_boundary = [](float origin, float dir, int cell, int step) -> float {
		if (step == 0) {
			return 1e20f;
		}
		float boundary = (step > 0) ? (float)(cell + 1) : (float)cell;
		return (boundary - origin) / dir;
	};

	float t_max_x = next_boundary(p_entry.x, rd.x, x, step_x);
	float t_max_y = next_boundary(p_entry.y, rd.y, y, step_y);
	float t_max_z = next_boundary(p_entry.z, rd.z, z, step_z);

	float t_delta_x = (step_x == 0) ? 1e20f : Math::abs(1.0f / rd.x);
	float t_delta_y = (step_y == 0) ? 1e20f : Math::abs(1.0f / rd.y);
	float t_delta_z = (step_z == 0) ? 1e20f : Math::abs(1.0f / rd.z);

	int last_axis = -1; // 0=X, 1=Y, 2=Z

	// First, check if the entry cell itself is solid. If so, normal points back along ray axis with largest |component|.
	if (data->get_block(x, y, z) != 0) {
		result.hit = true;
		result.block_hit = true;
		result.cell = Vector3i(x, y, z);
		// Pick the entry-face normal (the axis where t_start was reached).
		float ax = Math::abs(rd.x);
		float ay = Math::abs(rd.y);
		float az = Math::abs(rd.z);
		Vector3i n(0, 0, 0);
		if (ax >= ay && ax >= az) {
			n.x = (rd.x > 0) ? -1 : 1;
		} else if (ay >= ax && ay >= az) {
			n.y = (rd.y > 0) ? -1 : 1;
		} else {
			n.z = (rd.z > 0) ? -1 : 1;
		}
		result.normal = n;
		result.place_cell = result.cell + result.normal;
		return result;
	}

	const int MAX_STEPS = (size.x + size.y + size.z) * 2 + 16;
	for (int s = 0; s < MAX_STEPS; s++) {
		if (t_max_x < t_max_y && t_max_x < t_max_z) {
			x += step_x;
			t_max_x += t_delta_x;
			last_axis = 0;
		} else if (t_max_y < t_max_z) {
			y += step_y;
			t_max_y += t_delta_y;
			last_axis = 1;
		} else {
			z += step_z;
			t_max_z += t_delta_z;
			last_axis = 2;
		}
		if (x < 0 || y < 0 || z < 0 || x >= size.x || y >= size.y || z >= size.z) {
			result.hit = true;
			result.block_hit = false;
			result.cell = Vector3i((int)Math::floor(p_entry.x), (int)Math::floor(p_entry.y), (int)Math::floor(p_entry.z));
			result.cell.x = CLAMP(result.cell.x, 0, size.x - 1);
			result.cell.y = CLAMP(result.cell.y, 0, size.y - 1);
			result.cell.z = CLAMP(result.cell.z, 0, size.z - 1);
			result.place_cell = result.cell;
			return result;
		}
		if (data->get_block(x, y, z) != 0) {
			result.hit = true;
			result.block_hit = true;
			result.cell = Vector3i(x, y, z);
			Vector3i n(0, 0, 0);
			if (last_axis == 0) {
				n.x = -step_x;
			} else if (last_axis == 1) {
				n.y = -step_y;
			} else {
				n.z = -step_z;
			}
			result.normal = n;
			result.place_cell = result.cell + result.normal;
			return result;
		}
	}
	result.hit = true;
	result.block_hit = false;
	result.cell = Vector3i((int)Math::floor(p_entry.x), (int)Math::floor(p_entry.y), (int)Math::floor(p_entry.z));
	result.cell.x = CLAMP(result.cell.x, 0, size.x - 1);
	result.cell.y = CLAMP(result.cell.y, 0, size.y - 1);
	result.cell.z = CLAMP(result.cell.z, 0, size.z - 1);
	result.place_cell = result.cell;
	return result;
}

// ---- Apply edits ----

void VoxelSceneEditorPlugin::_apply_single_block(const Vector3i &p_cell, int p_new_id, const String &p_action_name) {
	if (!current_scene || current_scene->get_voxel_data().is_null()) {
		return;
	}
	Ref<VoxelSceneData> data = current_scene->get_voxel_data();
	if (!data->in_bounds_v(p_cell)) {
		return;
	}
	int old_id = data->get_block_v(p_cell);
	if (old_id == p_new_id) {
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(p_action_name, UndoRedo::MERGE_DISABLE);
	ur->add_do_method(data.ptr(), "set_block_v", p_cell, p_new_id);
	ur->add_undo_method(data.ptr(), "set_block_v", p_cell, old_id);
	ur->commit_action();
}

void VoxelSceneEditorPlugin::_apply_box(const Vector3i &p_a, const Vector3i &p_b, int p_new_id, const String &p_action_name) {
	if (!current_scene || current_scene->get_voxel_data().is_null()) {
		return;
	}
	Ref<VoxelSceneData> data = current_scene->get_voxel_data();
	Vector3i size = data->get_size();
	Vector3i lo(MIN(p_a.x, p_b.x), MIN(p_a.y, p_b.y), MIN(p_a.z, p_b.z));
	Vector3i hi(MAX(p_a.x, p_b.x), MAX(p_a.y, p_b.y), MAX(p_a.z, p_b.z));
	lo.x = CLAMP(lo.x, 0, size.x - 1);
	lo.y = CLAMP(lo.y, 0, size.y - 1);
	lo.z = CLAMP(lo.z, 0, size.z - 1);
	hi.x = CLAMP(hi.x, 0, size.x - 1);
	hi.y = CLAMP(hi.y, 0, size.y - 1);
	hi.z = CLAMP(hi.z, 0, size.z - 1);

	// Snapshot before/after as PackedByteArray for compact undo.
	PackedByteArray before = data->get_blocks_packed();

	// Apply directly to compute "after" snapshot.
	for (int y = lo.y; y <= hi.y; y++) {
		for (int z = lo.z; z <= hi.z; z++) {
			for (int x = lo.x; x <= hi.x; x++) {
				data->set_block(x, y, z, p_new_id);
			}
		}
	}
	PackedByteArray after = data->get_blocks_packed();
	// Restore so undo->redo cycle is consistent (we register set_blocks_packed for both directions).
	data->set_blocks_packed(before);

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(p_action_name, UndoRedo::MERGE_DISABLE);
	ur->add_do_method(data.ptr(), "set_blocks_packed", after);
	ur->add_undo_method(data.ptr(), "set_blocks_packed", before);
	ur->commit_action();
}

// ---- 3D gui input ----

EditorPlugin::AfterGUIInput VoxelSceneEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (!current_scene || !p_camera) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	// Cancel pending box op with Escape before mouse-only handling.
	Ref<InputEventKey> ke = p_event;
	if (ke.is_valid() && ke->is_pressed() && ke->get_keycode() == Key::ESCAPE) {
		if (box_first_set) {
			box_first_set = false;
			update_gizmo_for_current();
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
	}

	Ref<InputEventMouse> me = p_event;
	if (me.is_null()) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	if (current_scene->get_voxel_data().is_null()) {
		has_hover = false;
		if (toolbar) {
			toolbar->set_status_text("Assign VoxelSceneData to edit");
		}
		update_gizmo_for_current();
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}
	if (current_scene->get_block_registry().is_null()) {
		has_hover = false;
		if (toolbar) {
			toolbar->set_status_text("Assign VoxelBlockRegistry to choose blocks");
		}
		update_gizmo_for_current();
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	// Build ray in scene-local space.
	Vector2 mp = me->get_position();
	Transform3D xf = current_scene->get_global_transform();
	Transform3D inv = xf.affine_inverse();
	Vector3 world_origin = p_camera->project_ray_origin(mp);
	Vector3 world_dir = p_camera->project_ray_normal(mp);
	Vector3 local_origin = inv.xform(world_origin);
	Vector3 local_dir = inv.basis.xform(world_dir);

	VoxelRaycastHit hit = _voxel_raycast(local_origin, local_dir);

	bool prev_hover = has_hover;
	bool prev_block_hit = hover_block_hit;
	Vector3i prev_cell = hover_cell;
	Vector3i prev_normal = hover_normal;
	Vector3i prev_place_cell = hover_place_cell;
	if (hit.hit) {
		has_hover = true;
		hover_block_hit = hit.block_hit;
		hover_cell = hit.cell;
		hover_normal = hit.normal;
		hover_place_cell = hit.place_cell;
	} else {
		has_hover = false;
		hover_block_hit = false;
	}

	// Status text update on motion.
	if (toolbar) {
		String s;
		if (has_hover) {
			if (hover_block_hit) {
				s = vformat("Cell %s (block %d), face %s", String(hover_cell), current_scene->get_block_v(hover_cell), String(hover_normal));
			} else {
				s = vformat("Place cell %s (empty volume)", String(hover_place_cell));
			}
			if (box_first_set) {
				s += vformat("  |  Box A %s", String(box_first_cell));
			}
		} else {
			s = "(no hit)";
		}
		toolbar->set_status_text(s);
	}

	if (prev_hover != has_hover || (has_hover && (prev_block_hit != hover_block_hit || prev_cell != hover_cell || prev_normal != hover_normal || prev_place_cell != hover_place_cell))) {
		update_gizmo_for_current();
	}

	// Click handling.
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed()) {
		if (!has_hover) {
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}
		MouseButton btn = mb->get_button_index();
		VoxelSceneToolbar::Tool tool = toolbar->get_current_tool();
		int sel_id = palette->get_selected_block_id();

		// ПКМ = Erase regardless of current tool (ergonomic shortcut).
		if (btn == MouseButton::RIGHT) {
			if (hover_block_hit) {
				_apply_single_block(hover_cell, 0, "Erase Voxel");
				return EditorPlugin::AFTER_GUI_INPUT_STOP;
			}
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}
		if (btn != MouseButton::LEFT) {
			return EditorPlugin::AFTER_GUI_INPUT_PASS;
		}

		bool handled = false;
		switch (tool) {
			case VoxelSceneToolbar::TOOL_PAINT: {
				_apply_single_block(hover_place_cell, sel_id, "Paint Voxel");
				handled = true;
			} break;
			case VoxelSceneToolbar::TOOL_ERASE: {
				if (hover_block_hit) {
					_apply_single_block(hover_cell, 0, "Erase Voxel");
					handled = true;
				}
			} break;
			case VoxelSceneToolbar::TOOL_PICK: {
				if (hover_block_hit) {
					int picked = current_scene->get_block_v(hover_cell);
					if (picked > 0) {
						palette->set_selected_block_id(picked);
						handled = true;
					}
				}
			} break;
			case VoxelSceneToolbar::TOOL_BOX_FILL: {
				if (!box_first_set) {
					box_first_set = true;
					box_first_cell = hover_place_cell;
					update_gizmo_for_current();
				} else {
					_apply_box(box_first_cell, hover_place_cell, sel_id, "Box Fill");
					box_first_set = false;
					update_gizmo_for_current();
				}
				handled = true;
			} break;
			case VoxelSceneToolbar::TOOL_BOX_ERASE: {
				if (!hover_block_hit) {
					break;
				}
				if (!box_first_set) {
					box_first_set = true;
					box_first_cell = hover_cell;
					update_gizmo_for_current();
				} else {
					_apply_box(box_first_cell, hover_cell, 0, "Box Erase");
					box_first_set = false;
					update_gizmo_for_current();
				}
				handled = true;
			} break;
			default:
				break;
		}
		return handled ? EditorPlugin::AFTER_GUI_INPUT_STOP : EditorPlugin::AFTER_GUI_INPUT_PASS;
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}
