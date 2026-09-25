#include "moba_map_editor_plugin.h"

#include "../moba_map_preview_3d.h"

#include "core/input/input.h"
#include "core/io/dir_access.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "editor/editor_interface.h"
#include "editor/editor_data.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/separator.h"

#include "core/variant/variant_utility.h"

MobaMapGizmoPlugin::MobaMapGizmoPlugin() {
	create_material("grid", Color(0.65, 0.8, 0.75, 0.5));
	create_material("brush", Color(1.0, 0.85, 0.2));
	create_material("valid", Color(0.25, 1.0, 0.4));
	create_material("invalid", Color(1.0, 0.25, 0.2));
	create_material("spawn", Color(0.2, 0.85, 1.0));
	create_material("point", Color(1.0, 0.65, 0.2));
	for (int i = 0; i < 12; ++i) {
		create_material("level_" + itos(i), Color::from_hsv(i / 12.0, 0.65, 1.0));
	}
}

bool MobaMapGizmoPlugin::has_gizmo(Node3D *p_node) {
	return Object::cast_to<MobaMap3D>(p_node) || Object::cast_to<MobaMapMarker3D>(p_node);
}

void MobaMapGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();
	MobaMapMarker3D *marker = Object::cast_to<MobaMapMarker3D>(p_gizmo->get_node_3d());
	if (marker) {
		Vector<Vector3> lines;
		lines.push_back(Vector3(0, 0, 0));
		lines.push_back(Vector3(0, 3, 0));
		lines.push_back(Vector3(0, 3, 0));
		lines.push_back(Vector3(1.4, 2.5, 0));
		lines.push_back(Vector3(1.4, 2.5, 0));
		lines.push_back(Vector3(0, 2, 0));
		for (int i = 0; i < 24; ++i) {
			real_t a = i * Math::TAU / 24.0;
			real_t b = (i + 1) * Math::TAU / 24.0;
			lines.push_back(Vector3(Math::cos(a), 0.1, Math::sin(a)));
			lines.push_back(Vector3(Math::cos(b), 0.1, Math::sin(b)));
		}
		p_gizmo->add_lines(lines, get_material(marker->get_kind() == MobaMapMarker3D::SPAWN ? "spawn" : "point", p_gizmo));
		p_gizmo->add_collision_segments(lines);
		return;
	}
	MobaMap3D *map = Object::cast_to<MobaMap3D>(p_gizmo->get_node_3d());
	if (!map || map->get_map_data().is_null() || !editor || editor->get_map() != map) {
		return;
	}
	Ref<MobaMapData> data = map->get_map_data();
	if (editor->show_grid() || editor->show_levels()) {
		Vector<Vector3> lines[12];
		// Large maps retain a useful overview without a million-cell gizmo.
		int stride = MAX(1, MAX(data->get_size().x, data->get_size().y) / 128);
		for (int z = 0; z < data->get_size().y; z += stride) {
			for (int x = 0; x < data->get_size().x; x += stride) {
				Vector2i cell(x, z);
				Vector3 v[4];
				data->get_corners(cell, v);
				int color = editor->show_levels() ? ((data->get_cell(cell).x % 12) + 12) % 12 : 0;
				for (int i = 0; i < 4; ++i) {
					lines[color].push_back(v[i] + Vector3(0, 0.04, 0));
					lines[color].push_back(v[(i + 1) % 4] + Vector3(0, 0.04, 0));
				}
			}
		}
		for (int i = 0; i < 12; ++i) {
			if (!lines[i].is_empty()) {
				p_gizmo->add_lines(lines[i], get_material(editor->show_levels() ? "level_" + itos(i) : "grid", p_gizmo));
			}
		}
	}
	if (editor->show_navigation()) {
		Ref<NavigationMesh> mesh = map->get_navigation_mesh();
		if (mesh.is_valid()) {
			Vector<Vector3> vertices = mesh->get_vertices();
			Vector<Vector3> lines;
			for (int p = 0; p < mesh->get_polygon_count(); ++p) {
				Vector<int> polygon = mesh->get_polygon(p);
				for (int i = 0; i < polygon.size(); ++i) {
					lines.push_back(vertices[polygon[i]] + Vector3(0, 0.08, 0));
					lines.push_back(vertices[polygon[(i + 1) % polygon.size()]] + Vector3(0, 0.08, 0));
				}
			}
			p_gizmo->add_lines(lines, get_material(map->is_navigation_ready() ? "valid" : "invalid", p_gizmo));
		}
	}
	Vector2i cell;
	Vector3 position;
	if (!editor->get_hover(cell, position) || editor->get_tool() == MobaMapEditorPlugin::SELECT) {
		return;
	}
	Vector<Vector2i> cells;
	bool ramp = editor->get_tool() == MobaMapEditorPlugin::RAMP;
	if (ramp) {
		Vector2i direction = MobaMapData::ramp_direction(editor->get_ramp_direction());
		Vector2i side(-direction.y, direction.x);
		for (int i = 0; i < editor->get_ramp_width(); ++i) {
			cells.push_back(cell + side * (i - (editor->get_ramp_width() - 1) / 2));
		}
	} else {
		cells = editor->get_brush_cells(Vector2(position.x, position.z));
	}
	Vector<Vector3> outline;
	for (const Vector2i &c : cells) {
		Vector3 v[4];
		data->get_corners(c, v);
		for (int i = 0; i < 4; ++i) {
			outline.push_back(v[i] + Vector3(0, 0.12, 0));
			outline.push_back(v[(i + 1) % 4] + Vector3(0, 0.12, 0));
		}
		if (ramp) {
			Vector2i direction = MobaMapData::ramp_direction(editor->get_ramp_direction());
			Vector3 start((c.x + 0.5) * data->get_cell_size(), data->get_cell(c).x * data->get_height_step() + 0.25, (c.y + 0.5) * data->get_cell_size());
			outline.push_back(start);
			outline.push_back(start + Vector3(direction.x, 0, direction.y) * data->get_cell_size() * 0.5 + Vector3(0, data->get_height_step(), 0));
		}
	}
	p_gizmo->add_lines(outline, get_material(ramp ? (editor->is_ramp_valid(cell) ? "valid" : "invalid") : "brush", p_gizmo));
}

static SpinBox *moba_spin(VBoxContainer *p_parent, const String &p_label, double p_min, double p_max, double p_step, double p_default) {
	Label *label = memnew(Label);
	label->set_text(p_label);
	p_parent->add_child(label);
	SpinBox *spin = memnew(SpinBox);
	spin->set_min(p_min);
	spin->set_max(p_max);
	spin->set_step(p_step);
	spin->set_value(p_default);
	p_parent->add_child(spin);
	return spin;
}

static void moba_label(VBoxContainer *p_parent, const String &p_text) {
	Label *label = memnew(Label);
	label->set_text(p_text);
	p_parent->add_child(label);
}

MobaMapEditorPlugin::MobaMapEditorPlugin() {
	random.randomize();
	toolbar = memnew(HBoxContainer);
	Button *initialize = memnew(Button);
	initialize->set_text("Map settings");
	initialize->connect("pressed", callable_mp(this, &MobaMapEditorPlugin::_show_create));
	toolbar->add_child(initialize);
	tools = memnew(OptionButton);
	const char *names[] = { "Select", "Raise plateau", "Lower plateau", "Flatten", "Paint material", "Ramp", "Place object", "Forest brush", "Erase objects", "Spawn", "Point of interest" };
	for (const char *name : names) {
		tools->add_item(name);
	}
	tools->connect("item_selected", callable_mp(this, &MobaMapEditorPlugin::_tool_changed));
	toolbar->add_child(tools);
	Button *bake = memnew(Button);
	bake->set_text("Bake navigation");
	bake->connect("pressed", callable_mp(this, &MobaMapEditorPlugin::_bake));
	toolbar->add_child(bake);
	Button *preview = memnew(Button);
	preview->set_text("Play map");
	preview->connect("pressed", callable_mp(this, &MobaMapEditorPlugin::_run_preview));
	toolbar->add_child(preview);
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);
	toolbar->hide();

	dock = memnew(VBoxContainer);
	dock->set_name("MOBA Map");
	dock->set_custom_minimum_size(Vector2(230, 0));
	moba_label(dock, "Brush shape");
	shape = memnew(OptionButton);
	shape->add_item("Circle");
	shape->add_item("Square");
	shape->connect("item_selected", callable_mp(this, &MobaMapEditorPlugin::_settings_changed).unbind(1));
	dock->add_child(shape);
	radius = moba_spin(dock, "Radius (cells)", 0.5, 16, 0.5, 1.5);
	radius->connect("value_changed", callable_mp(this, &MobaMapEditorPlugin::_radius_changed));
	density = moba_spin(dock, "Forest density", 0.05, 1.0, 0.05, 0.35);
	density->connect("value_changed", callable_mp(this, &MobaMapEditorPlugin::_settings_changed).unbind(1));
	moba_label(dock, "Surface material");
	materials = memnew(OptionButton);
	materials->connect("item_selected", callable_mp(this, &MobaMapEditorPlugin::_settings_changed).unbind(1));
	dock->add_child(materials);
	moba_label(dock, "Object scene");
	objects = memnew(OptionButton);
	objects->connect("item_selected", callable_mp(this, &MobaMapEditorPlugin::_settings_changed).unbind(1));
	dock->add_child(objects);
	moba_label(dock, "Ramp rises toward");
	ramp_direction = memnew(OptionButton);
	for (const char *name : { "North (-Z)", "East (+X)", "South (+Z)", "West (-X)" }) {
		ramp_direction->add_item(name);
	}
	ramp_direction->connect("item_selected", callable_mp(this, &MobaMapEditorPlugin::_settings_changed).unbind(1));
	dock->add_child(ramp_direction);
	ramp_width = moba_spin(dock, "Ramp width (cells)", 1, 16, 1, 2);
	ramp_width->connect("value_changed", callable_mp(this, &MobaMapEditorPlugin::_radius_changed));
	dock->add_child(memnew(HSeparator));
	grid = memnew(CheckBox);
	grid->set_text("Grid");
	grid->set_pressed(true);
	dock->add_child(grid);
	levels = memnew(CheckBox);
	levels->set_text("Plateau levels (colors)");
	dock->add_child(levels);
	nav = memnew(CheckBox);
	nav->set_text("Navigation (red = outdated)");
	dock->add_child(nav);
	for (CheckBox *toggle : { grid, levels, nav }) {
		toggle->connect("toggled", callable_mp(this, &MobaMapEditorPlugin::_overlay_changed));
	}
	status = memnew(Label);
	status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	dock->add_child(status);
	moba_label(dock, "LMB: paint / place\nCtrl: inverse / erase\nShift: flatten terrain\nEsc: cancel stroke / Select\nAlt / RMB: Godot camera\nSelect mode: edit objects normally");
	add_control_to_dock(DOCK_SLOT_RIGHT_BR, dock);
	dock->hide();

	create_dialog = memnew(ConfirmationDialog);
	create_dialog->set_title("MOBA map settings");
	VBoxContainer *fields = memnew(VBoxContainer);
	create_dialog->add_child(fields);
	width = moba_spin(fields, "Width (cells)", 1, 1024, 1, 64);
	depth = moba_spin(fields, "Depth (cells)", 1, 1024, 1, 64);
	cell_scale = moba_spin(fields, "Cell size (world units)", 1, 64, 0.5, 4);
	height_step = moba_spin(fields, "Plateau height step", 0.5, 32, 0.5, 2);
	moba_label(fields, "Resizing keeps overlapping terrain and existing objects.\nFor walkable ramps, height step / cell size must be below 1.19.");
	create_dialog->connect("confirmed", callable_mp(this, &MobaMapEditorPlugin::_create_map));
	EditorNode::get_singleton()->get_gui_base()->add_child(create_dialog);
	add_tool_menu_item("Create MOBA map...", callable_mp(this, &MobaMapEditorPlugin::_show_create));
	gizmos.instantiate();
	gizmos->set_editor(this);
	add_node_3d_gizmo_plugin(gizmos);
	set_process_internal(true);
}

MobaMapEditorPlugin::~MobaMapEditorPlugin() {
	_finish_stroke();
	_cancel_preview();
	if (observed_palette.is_valid()) {
		observed_palette->disconnect("changed", callable_mp(this, &MobaMapEditorPlugin::_refresh_palette));
	}
	gizmos->set_editor(nullptr);
	remove_node_3d_gizmo_plugin(gizmos);
	remove_tool_menu_item("Create MOBA map...");
	remove_control_from_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);
	remove_control_from_docks(dock);
	memdelete(toolbar);
	memdelete(dock);
	memdelete(create_dialog);
}

MobaMap3D *MobaMapEditorPlugin::get_map() const {
	return Object::cast_to<MobaMap3D>(ObjectDB::get_instance(map_id));
}

bool MobaMapEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<MobaMap3D>(p_object) != nullptr;
}

void MobaMapEditorPlugin::edit(Object *p_object) {
	_finish_stroke();
	MobaMap3D *previous = get_map();
	map_id = ObjectID();
	if (previous) {
		previous->update_gizmos();
	}
	MobaMap3D *map = Object::cast_to<MobaMap3D>(p_object);
	map_id = map ? map->get_instance_id() : ObjectID();
	hovered = false;
	_refresh_palette();
	_update_status();
	if (map) {
		map->update_gizmos();
	}
}

void MobaMapEditorPlugin::make_visible(bool p_visible) {
	toolbar->set_visible(p_visible);
	dock->set_visible(p_visible);
	if (!p_visible) {
		edit(nullptr);
	}
}

void MobaMapEditorPlugin::apply_changes() {
	_finish_stroke();
}

void MobaMapEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_INTERNAL_PROCESS) {
		if (stroke && !Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
			_finish_stroke();
		}
		MobaMap3D *map = get_map();
		if (map && map->get_palette() != observed_palette) {
			_refresh_palette();
		}
		if (preview_map_id.is_valid() && !ObjectDB::get_instance(preview_map_id)) {
			_cancel_preview();
		}
		Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
		if (preview_root_id.is_valid() && (!root || root->get_instance_id() != preview_root_id)) {
			_cancel_preview();
		}
		_update_status();
	}
}

void MobaMapEditorPlugin::_show_create() {
	_finish_stroke();
	MobaMap3D *map = get_map();
	Ref<MobaMapData> data = map ? map->get_map_data() : Ref<MobaMapData>();
	width->set_value(data.is_valid() ? data->get_size().x : 64);
	depth->set_value(data.is_valid() ? data->get_size().y : 64);
	cell_scale->set_value(data.is_valid() ? data->get_cell_size() : 4);
	height_step->set_value(data.is_valid() ? data->get_height_step() : 2);
	create_dialog->popup_centered();
}

void MobaMapEditorPlugin::_create_map() {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root) {
		EditorNode::get_singleton()->show_warning("Create a 3D scene first, then choose Project > Tools > Create MOBA map.");
		return;
	}
	MobaMap3D *map = get_map();
	Ref<MobaMapData> old_data = map ? map->get_map_data() : Ref<MobaMapData>();
	Ref<MobaMapData> data;
	if (old_data.is_valid()) {
		data = old_data->duplicate(true);
	} else {
		data.instantiate();
	}
	data->set_size(Vector2i(width->get_value(), depth->get_value()));
	data->set_cell_size(cell_scale->get_value());
	data->set_height_step(height_step->get_value());
	Ref<MobaMapPalette> palette = map ? map->get_palette() : Ref<MobaMapPalette>();
	if (palette.is_null()) {
		palette.instantiate();
		palette->create_defaults();
	}
	EditorUndoRedoManager *undo = get_undo_redo();
	undo->create_action(map ? "Configure MOBA map" : "Create MOBA map", UndoRedo::MERGE_DISABLE, root);
	if (!map) {
		map = memnew(MobaMap3D);
		map->set_name("Map");
		map->set_map_data(data);
		map->set_palette(palette);
		undo->add_do_method(root, "add_child", map, true);
		undo->add_do_method(map, "set_owner", root);
		undo->add_undo_method(root, "remove_child", map);
		undo->add_do_reference(map);
	} else {
		undo->add_do_method(map, "set_map_data", data);
		undo->add_do_method(map, "set_palette", palette);
		undo->add_undo_method(map, "set_map_data", old_data);
		undo->add_undo_method(map, "set_palette", map->get_palette());
	}
	undo->commit_action();
	EditorInterface::get_singleton()->get_selection()->clear();
	EditorInterface::get_singleton()->get_selection()->add_node(map);
	EditorInterface::get_singleton()->edit_node(map);
	EditorInterface::get_singleton()->set_main_screen_editor("3D");
}

void MobaMapEditorPlugin::_refresh_palette() {
	MobaMap3D *map = get_map();
	Ref<MobaMapPalette> next = map ? map->get_palette() : Ref<MobaMapPalette>();
	if (observed_palette != next) {
		if (observed_palette.is_valid()) {
			observed_palette->disconnect("changed", callable_mp(this, &MobaMapEditorPlugin::_refresh_palette));
		}
		observed_palette = next;
		if (observed_palette.is_valid()) {
			observed_palette->connect("changed", callable_mp(this, &MobaMapEditorPlugin::_refresh_palette));
		}
	}
	int selected_material = materials->get_selected();
	int selected_object = objects->get_selected();
	materials->clear();
	objects->clear();
	if (next.is_null()) {
		return;
	}
	for (int i = 0; i < next->get_materials().size(); ++i) {
		Ref<Material> material = next->get_material(i);
		materials->add_item(material.is_valid() && !material->get_name().is_empty() ? material->get_name() : "Material " + itos(i), i);
	}
	for (int i = 0; i < next->get_objects().size(); ++i) {
		Ref<PackedScene> scene = next->get_object(i);
		String name = scene.is_valid() ? scene->get_name() : "Empty";
		if (name.is_empty()) {
			name = scene->get_path().get_file().get_basename();
		}
		objects->add_item(name.is_empty() ? "Object " + itos(i) : name, i);
	}
	if (materials->get_item_count()) { materials->select(CLAMP(selected_material, 0, materials->get_item_count() - 1)); }
	if (objects->get_item_count()) { objects->select(CLAMP(selected_object, 0, objects->get_item_count() - 1)); }
}

void MobaMapEditorPlugin::_tool_changed(int p_index) {
	_finish_stroke();
	_settings_changed();
}

void MobaMapEditorPlugin::_settings_changed() {
	_finish_stroke();
	if (get_map()) {
		get_map()->update_gizmos();
	}
}

void MobaMapEditorPlugin::_radius_changed(double p_value) {
	_settings_changed();
}

void MobaMapEditorPlugin::_overlay_changed(bool p_enabled) {
	if (get_map()) {
		get_map()->update_gizmos();
	}
}

bool MobaMapEditorPlugin::get_hover(Vector2i &r_cell, Vector3 &r_position) const {
	r_cell = hover_cell;
	r_position = hover_position;
	return hovered;
}

bool MobaMapEditorPlugin::is_ramp_valid(Vector2i p_cell) const {
	return get_map() && get_map()->get_map_data().is_valid() && get_map()->get_map_data()->can_place_ramp(p_cell, get_ramp_direction(), get_ramp_width());
}

Vector<Vector2i> MobaMapEditorPlugin::get_brush_cells(Vector2 p_position) const {
	Vector<Vector2i> result;
	MobaMap3D *map = get_map();
	if (!map || map->get_map_data().is_null()) {
		return result;
	}
	Ref<MobaMapData> data = map->get_map_data();
	Vector2 center = p_position / data->get_cell_size();
	Vector2i cell((int)Math::floor(center.x), (int)Math::floor(center.y));
	Tool tool = get_tool();
	if (tool == PLACE || tool == SPAWN || tool == POINT) {
		if (data->contains(cell)) { result.push_back(cell); }
		return result;
	}
	real_t r = radius->get_value();
	int extent = (int)Math::ceil(r);
	for (int z = cell.y - extent; z <= cell.y + extent; ++z) {
		for (int x = cell.x - extent; x <= cell.x + extent; ++x) {
			Vector2i c(x, z);
			Vector2 offset = Vector2(x + 0.5, z + 0.5) - center;
			bool inside = shape->get_selected() == 0 ? offset.length() <= r : MAX(Math::abs(offset.x), Math::abs(offset.y)) <= r;
			if (data->contains(c) && (inside || c == cell)) {
				result.push_back(c);
			}
		}
	}
	return result;
}

void MobaMapEditorPlugin::_begin_stroke(bool p_erase, bool p_flatten) {
	MobaMap3D *map = get_map();
	if (!map || map->get_map_data().is_null()) {
		return;
	}
	stroke = true;
	stroke_map_id = map_id;
	stroke_erase = p_erase;
	stroke_flatten = p_flatten;
	before = map->get_map_data()->get_cells();
	touched.clear();
	added.clear();
	removed.clear();
	removed_owners.clear();
	notice = String();
	target_level = map->get_map_data()->get_cell(hover_cell).x;
	if (!stroke_flatten && get_tool() != LEVEL) {
		if (get_tool() == RAISE) { target_level += p_erase ? -1 : 1; }
		if (get_tool() == LOWER) { target_level += p_erase ? 1 : -1; }
	}
	target_level = CLAMP(target_level, -128, 128);
	last_stamp = Vector2(hover_position.x, hover_position.z);
	_stamp(last_stamp);
}

void MobaMapEditorPlugin::_put_object(Node3D *p_object, Vector2 p_position, real_t p_rotation) {
	MobaMap3D *map = get_map();
	if (!map) {
		memdelete(p_object);
		return;
	}
	p_object->set_meta("moba_placed_object", true);
	p_object->set_meta("moba_grounded", true);
	p_object->set_meta("moba_height_offset", 0.0);
	p_object->set_position(Vector3(p_position.x, map->get_surface_height(p_position), p_position.y));
	Vector3 rotation = p_object->get_rotation();
	rotation.y = p_rotation;
	p_object->set_rotation(rotation);
	map->attach_object(p_object, EditorInterface::get_singleton()->get_edited_scene_root());
	added.push_back(p_object);
}

void MobaMapEditorPlugin::_stamp(Vector2 p_position) {
	MobaMap3D *map = get_map();
	if (!stroke || !map || map_id != stroke_map_id || map->get_map_data().is_null()) {
		return;
	}
	Ref<MobaMapData> data = map->get_map_data();
	Tool tool = get_tool();
	Vector2i center((int)Math::floor(p_position.x / data->get_cell_size()), (int)Math::floor(p_position.y / data->get_cell_size()));
	if (!data->contains(center)) {
		return;
	}
	if (tool == RAMP) {
		if (!touched.is_empty()) { return; }
		touched.insert(center);
		if (stroke_erase) {
			Vector2i dir = MobaMapData::ramp_direction(get_ramp_direction());
			Vector2i side(-dir.y, dir.x);
			Dictionary patch;
			for (int i = 0; i < get_ramp_width(); ++i) {
				Vector2i c = center + side * (i - (get_ramp_width() - 1) / 2);
				if (!data->contains(c)) { continue; }
				Vector3i value = data->get_cell(c);
				value.z = MobaMapData::RAMP_NONE;
				patch[c] = value;
			}
			data->apply_cells(patch);
		} else if (!data->place_ramp(center, get_ramp_direction(), get_ramp_width())) {
			notice = "Ramp needs a low plateau behind it and a plateau exactly one level higher in front.";
		}
		return;
	}
	Vector<Vector2i> cells = get_brush_cells(p_position);
	HashSet<Vector2i> footprint;
	for (const Vector2i &cell : cells) { footprint.insert(cell); }
	if (tool == ERASE || (stroke_erase && (tool == PLACE || tool == FOREST || tool == SPAWN || tool == POINT))) {
		for (int i = map->get_child_count() - 1; i >= 0; --i) {
			Node3D *object = Object::cast_to<Node3D>(map->get_child(i));
			if (!object || !bool(object->get_meta("moba_placed_object", false))) { continue; }
			Vector3 position = object->get_position();
			Vector2i cell((int)Math::floor(position.x / data->get_cell_size()), (int)Math::floor(position.z / data->get_cell_size()));
			if (!footprint.has(cell)) { continue; }
			removed_owners.insert(object->get_instance_id(), object->get_owner());
			map->detach_object(object);
			removed.push_back(object);
		}
		return;
	}
	if (tool == PLACE || tool == SPAWN || tool == POINT) {
		if (!touched.is_empty()) { return; }
		touched.insert(center);
		if (tool == PLACE) {
			Ref<PackedScene> scene = map->get_palette().is_valid() ? map->get_palette()->get_object(objects->get_selected()) : Ref<PackedScene>();
			if (scene.is_null()) {
				notice = "Add a PackedScene to the map palette and select it.";
				return;
			}
			Node *instance = scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
			Node3D *spatial = Object::cast_to<Node3D>(instance);
			if (!spatial) {
				if (instance) { memdelete(instance); }
				notice = "Object scenes must have a Node3D root.";
				return;
			}
			_put_object(spatial, p_position, random.randf() * Math::TAU);
		} else {
			MobaMapMarker3D *marker = memnew(MobaMapMarker3D);
			marker->set_kind(tool == SPAWN ? MobaMapMarker3D::SPAWN : MobaMapMarker3D::POINT_OF_INTEREST);
			marker->set_name(tool == SPAWN ? "Spawn" : "PointOfInterest");
			marker->set_marker_id((tool == SPAWN ? "spawn_" : "point_") + itos(map->get_markers().size() + 1));
			_put_object(marker, p_position, 0.0);
		}
		return;
	}
	Dictionary patch;
	for (const Vector2i &cell : cells) {
		if (touched.has(cell)) { continue; }
		touched.insert(cell);
		Vector3i value = data->get_cell(cell);
		if (tool == FOREST) {
			if (random.randf() > density->get_value()) { continue; }
			Vector2 point = (Vector2(cell) + Vector2(0.2 + random.randf() * 0.6, 0.2 + random.randf() * 0.6)) * data->get_cell_size();
			bool occupied = false;
			for (int i = 0; i < map->get_child_count(); ++i) {
				Node3D *object = Object::cast_to<Node3D>(map->get_child(i));
				if (object && bool(object->get_meta("moba_placed_object", false))) {
					Vector3 position = object->get_position();
					if (point.distance_to(Vector2(position.x, position.z)) < 1.5) { occupied = true; break; }
				}
			}
			if (occupied || map->get_palette().is_null()) { continue; }
			Ref<PackedScene> scene = map->get_palette()->get_object(objects->get_selected());
			if (scene.is_null()) { notice = "Select an object scene in the palette."; continue; }
			Node *instance = scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
			Node3D *spatial = Object::cast_to<Node3D>(instance);
			if (!spatial) {
				if (instance) { memdelete(instance); }
				notice = "Object scenes must have a Node3D root.";
				continue;
			}
			_put_object(spatial, point, random.randf() * Math::TAU);
		} else if (tool == MATERIAL) {
			if (materials->get_selected() < 0) { continue; }
			value.y = stroke_erase ? 0 : materials->get_selected();
			patch[cell] = value;
		} else if (tool == RAISE || tool == LOWER || tool == LEVEL) {
			if (stroke_flatten || tool == LEVEL) { value.x = target_level; }
			else if ((tool == RAISE) != stroke_erase) { value.x = MAX(value.x, target_level); }
			else { value.x = MIN(value.x, target_level); }
			value.z = MobaMapData::RAMP_NONE;
			patch[cell] = value;
		}
	}
	if (!patch.is_empty()) { data->apply_cells(patch); }
}

void MobaMapEditorPlugin::_finish_stroke(bool p_cancel) {
	if (!stroke) { return; }
	stroke = false;
	MobaMap3D *map = Object::cast_to<MobaMap3D>(ObjectDB::get_instance(stroke_map_id));
	stroke_map_id = ObjectID();
	if (!map || map->get_map_data().is_null()) {
		for (Node3D *object : removed) { memdelete(object); }
		added.clear();
		removed.clear();
		return;
	}
	Ref<MobaMapData> data = map->get_map_data();
	PackedInt32Array after = data->get_cells();
	if (p_cancel) {
		data->set_cells(before);
		for (Node3D *object : added) {
			map->detach_object(object);
			memdelete(object);
		}
		for (Node3D *object : removed) { map->attach_object(object, removed_owners[object->get_instance_id()]); }
	} else if (before != after || !added.is_empty() || !removed.is_empty()) {
		EditorUndoRedoManager *undo = get_undo_redo();
		undo->create_action("Paint MOBA map", UndoRedo::MERGE_DISABLE, map);
		if (before != after) {
			undo->add_do_method(data.ptr(), "set_cells", after);
			undo->add_undo_method(data.ptr(), "set_cells", before);
		}
		Node *owner = EditorInterface::get_singleton()->get_edited_scene_root();
		for (Node3D *object : added) {
			undo->add_do_method(map, "attach_object", object, owner);
			undo->add_undo_method(map, "detach_object", object);
			undo->add_do_reference(object);
		}
		for (Node3D *object : removed) {
			undo->add_do_method(map, "detach_object", object);
			undo->add_undo_method(map, "attach_object", object, removed_owners[object->get_instance_id()]);
			undo->add_undo_reference(object);
		}
		// The drag is already visible; replaying it here would place every object twice.
		undo->commit_action(false);
		EditorInterface::get_singleton()->mark_scene_as_unsaved();
	}
	before.clear();
	added.clear();
	removed.clear();
	removed_owners.clear();
	touched.clear();
	map->rebuild_dirty();
}

EditorPlugin::AfterGUIInput MobaMapEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	MobaMap3D *map = get_map();
	if (!map || map->get_map_data().is_null()) { return AFTER_GUI_INPUT_PASS; }
	Ref<InputEventKey> key = p_event;
	if (key.is_valid() && key->is_pressed()) {
		if (key->get_keycode() == Key::ESCAPE) {
			if (stroke) { _finish_stroke(true); }
			else { tools->select(SELECT); }
			map->update_gizmos();
			return AFTER_GUI_INPUT_STOP;
		}
		if (key->is_command_or_control_pressed()) { _finish_stroke(); }
		return AFTER_GUI_INPUT_PASS;
	}
	Ref<InputEventMouse> mouse = p_event;
	if (mouse.is_null()) { return AFTER_GUI_INPUT_PASS; }
	Ref<InputEventMouseButton> button = p_event;
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT && !button->is_pressed() && stroke) {
		_finish_stroke();
		return AFTER_GUI_INPUT_STOP;
	}
	if (mouse->is_alt_pressed() || Input::get_singleton()->is_mouse_button_pressed(MouseButton::RIGHT) || Input::get_singleton()->is_mouse_button_pressed(MouseButton::MIDDLE)) {
		_finish_stroke();
		hovered = false;
		map->update_gizmos();
		return AFTER_GUI_INPUT_PASS;
	}
	Vector3 hit;
	hovered = map->raycast(p_camera->project_ray_origin(mouse->get_position()), p_camera->project_ray_normal(mouse->get_position()), hit);
	if (hovered) {
		hover_position = map->to_local(hit);
		real_t cell = map->get_map_data()->get_cell_size();
		hover_cell = Vector2i((int)Math::floor(hover_position.x / cell), (int)Math::floor(hover_position.z / cell));
		hovered = map->get_map_data()->contains(hover_cell);
	}
	map->update_gizmos();
	if (get_tool() == SELECT) { return AFTER_GUI_INPUT_PASS; }
	if (!hovered) { return stroke ? AFTER_GUI_INPUT_STOP : AFTER_GUI_INPUT_PASS; }
	if (button.is_valid() && button->get_button_index() == MouseButton::LEFT && button->is_pressed()) {
		_begin_stroke(mouse->is_ctrl_pressed(), mouse->is_shift_pressed());
		return AFTER_GUI_INPUT_STOP;
	}
	Ref<InputEventMouseMotion> motion = p_event;
	if (motion.is_valid() && stroke) {
		Vector2 target(hover_position.x, hover_position.z);
		real_t distance = last_stamp.distance_to(target);
		int samples = MIN(2048, (int)Math::ceil(distance / (map->get_map_data()->get_cell_size() * 0.25)));
		for (int i = 1; i <= samples; ++i) {
			_stamp(last_stamp.lerp(target, (real_t)i / samples));
		}
		last_stamp = target;
		return AFTER_GUI_INPUT_STOP;
	}
	return AFTER_GUI_INPUT_PASS;
}

void MobaMapEditorPlugin::_update_status() {
	MobaMap3D *map = get_map();
	if (!map) { return; }
	if (map->get_map_data().is_null()) {
		status->set_text("Use Map settings to initialize terrain.");
		return;
	}
	String text = map->is_baking_navigation() ? "Navigation: baking" : (map->is_navigation_ready() ? "Navigation: ready" : "Navigation: needs bake");
	if (hovered) {
		Vector3i value = map->get_map_data()->get_cell(hover_cell);
		text += "\nCell " + itos(hover_cell.x) + ", " + itos(hover_cell.y) + " | level " + itos(value.x);
	}
	if (!notice.is_empty()) { text += "\n" + notice; }
	status->set_text(text);
}

void MobaMapEditorPlugin::_bake() {
	_finish_stroke();
	if (get_map()) { get_map()->bake_navigation(); }
}

void MobaMapEditorPlugin::_cancel_preview() {
	MobaMap3D *map = Object::cast_to<MobaMap3D>(ObjectDB::get_instance(preview_map_id));
	if (map && map->is_connected("navigation_baked", callable_mp(this, &MobaMapEditorPlugin::_preview_baked))) {
		map->disconnect("navigation_baked", callable_mp(this, &MobaMapEditorPlugin::_preview_baked));
	}
	preview_map_id = ObjectID();
	preview_root_id = ObjectID();
}

void MobaMapEditorPlugin::_run_preview() {
	_finish_stroke();
	_cancel_preview();
	MobaMap3D *map = get_map();
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!map || !root || map->get_map_data().is_null()) { return; }
	preview_map_id = map_id;
	preview_root_id = root->get_instance_id();
	map->connect("navigation_baked", callable_mp(this, &MobaMapEditorPlugin::_preview_baked));
	map->bake_navigation();
}

void MobaMapEditorPlugin::_preview_baked(bool p_success) {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root || root->get_instance_id() != preview_root_id) {
		_cancel_preview();
		return;
	}
	if (p_success) {
		_write_preview();
	} else {
		EditorNode::get_singleton()->show_warning("Navigation produced no walkable surface. Check the map geometry and ramp slope.");
	}
	_cancel_preview();
}

void MobaMapEditorPlugin::_write_preview() {
	MobaMap3D *map = Object::cast_to<MobaMap3D>(ObjectDB::get_instance(preview_map_id));
	if (!map || !map->is_navigation_ready()) { return; }
	MobaMapPreview3D *root = memnew(MobaMapPreview3D);
	root->set_name("MobaPreview");
	MobaMap3D *copy = memnew(MobaMap3D);
	copy->set_name("Map");
	copy->set_transform(map->get_global_transform());
	copy->set_map_data(map->get_map_data()->duplicate(true));
	copy->set_palette(map->get_palette());
	root->add_child(copy);
	copy->set_owner(root);
	for (int i = 0; i < map->get_child_count(); ++i) {
		Node *child = map->get_child(i)->duplicate(Node::DUPLICATE_GROUPS | Node::DUPLICATE_SCRIPTS | Node::DUPLICATE_USE_INSTANTIATION);
		if (!child) { continue; }
		copy->add_child(child, true);
		MobaMap3D::adopt_object_owner(child, root);
	}
	Ref<PackedScene> packed;
	packed.instantiate();
	Error error = packed->pack(root);
	memdelete(root);
	String directory = "res://moba_previews";
	if (error == OK) { error = DirAccess::make_dir_recursive_absolute(directory); }
	String path = directory.path_join("map_" + itos(OS::get_singleton()->get_ticks_usec()) + ".tscn");
	if (error == OK) { error = ResourceSaver::save(packed, path); }
	if (error != OK) {
		EditorNode::get_singleton()->show_warning("Could not save the preview scene: " + VariantUtilityFunctions::error_string(error));
		return;
	}
	EditorInterface::get_singleton()->play_custom_scene(path);
}
