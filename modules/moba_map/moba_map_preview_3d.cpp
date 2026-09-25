#include "moba_map_preview_3d.h"

#include "core/config/engine.h"
#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/environment.h"
#include "servers/navigation_3d/navigation_server_3d.h"

void MobaMapPreview3D::_notification(int p_what) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (p_what == NOTIFICATION_READY) {
		map = Object::cast_to<MobaMap3D>(get_node_or_null(map_path));
		CanvasLayer *overlay = memnew(CanvasLayer);
		add_child(overlay, false, INTERNAL_MODE_BACK);
		status = memnew(Label);
		status->set_position(Vector2(20, 20));
		status->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
		overlay->add_child(status);
		if (!map || map->get_map_data().is_null()) {
			status->set_text("Assign map_path to a MobaMap3D with map data.");
			return;
		}
		status->set_text("Preparing navigation...");
		hero = memnew(CharacterBody3D);
		hero->set_floor_snap_length(0.5);
		hero->set_floor_max_angle(Math::deg_to_rad(50.0));
		hero->set_floor_constant_speed_enabled(true);
		add_child(hero, false, INTERNAL_MODE_BACK);
		CollisionShape3D *collision = memnew(CollisionShape3D);
		Ref<CapsuleShape3D> shape;
		shape.instantiate();
		shape->set_radius(0.35);
		shape->set_height(1.7);
		collision->set_shape(shape);
		collision->set_position(Vector3(0, 0.85, 0));
		hero->add_child(collision);
		MeshInstance3D *visual = memnew(MeshInstance3D);
		Ref<CapsuleMesh> mesh;
		mesh.instantiate();
		mesh->set_radius(0.35);
		mesh->set_height(1.7);
		visual->set_mesh(mesh);
		visual->set_position(Vector3(0, 0.85, 0));
		Ref<StandardMaterial3D> material;
		material.instantiate();
		material->set_albedo(Color(0.3, 0.7, 1.0));
		visual->set_material_override(material);
		hero->add_child(visual);
		camera = memnew(Camera3D);
		camera->set_projection(Camera3D::PROJECTION_ORTHOGONAL);
		camera->set_size(35.0);
		add_child(camera, false, INTERNAL_MODE_BACK);
		camera->set_current(true);
		DirectionalLight3D *light = memnew(DirectionalLight3D);
		light->set_rotation_degrees(Vector3(-55, -25, 0));
		light->set_shadow(true);
		add_child(light, false, INTERNAL_MODE_BACK);
		WorldEnvironment *environment = memnew(WorldEnvironment);
		Ref<Environment> settings;
		settings.instantiate();
		settings->set_background(Environment::BG_COLOR);
		settings->set_bg_color(Color(0.16, 0.20, 0.25));
		settings->set_ambient_source(Environment::AMBIENT_SOURCE_COLOR);
		settings->set_ambient_light_color(Color(0.75, 0.8, 0.9));
		settings->set_ambient_light_energy(0.6);
		environment->set_environment(settings);
		add_child(environment, false, INTERNAL_MODE_BACK);
		set_process_internal(true);
		set_physics_process_internal(true);
		set_process_unhandled_input(true);
	} else if (p_what == NOTIFICATION_INTERNAL_PHYSICS_PROCESS && map && hero) {
		NavigationServer3D *server = NavigationServer3D::get_singleton();
		RID nav_map = map->get_world_3d()->get_navigation_map();
		if (!map->is_navigation_ready() || server->map_get_iteration_id(nav_map) == 0) {
			hero->set_velocity(Vector3());
			path.clear();
			had_navigation = false;
			navigation_settle_frames = 0;
			status->set_text(map->is_baking_navigation() ? "Preparing navigation..." : "Navigation unavailable. Return to the editor and rebuild navigation.");
			return;
		}
		// A baked resource precedes NavigationServer's region synchronization.
		// Give the newly enabled region two physics ticks before projecting spawn.
		if (navigation_settle_frames < 2) {
			++navigation_settle_frames;
			return;
		}
		if (!had_navigation) {
			had_navigation = true;
			status->set_text("Click ground: move  |  Wheel: zoom  |  Camera follows the hero");
		}
		if (!placed) {
			Ref<MobaMapData> data = map->get_map_data();
			Vector2 center = Vector2(data->get_size()) * data->get_cell_size() * 0.5;
			Vector3 spawn = map->to_global(Vector3(center.x, data->get_surface_height(center), center.y));
			TypedArray<MobaMapMarker3D> markers = map->get_markers();
			for (int i = 0; i < markers.size(); ++i) {
				MobaMapMarker3D *marker = Object::cast_to<MobaMapMarker3D>(markers[i]);
				if (marker && marker->get_kind() == MobaMapMarker3D::SPAWN) {
					spawn = marker->get_global_position();
					break;
				}
			}
			hero->set_global_position(server->map_get_closest_point(nav_map, spawn) + Vector3(0, 0.15, 0));
			placed = true;
		}
		Vector3 velocity = hero->get_velocity();
		velocity.x = velocity.z = 0;
		while (waypoint < path.size()) {
			Vector3 delta = path[waypoint] - hero->get_global_position();
			delta.y = 0;
			if (delta.length() < 0.3) {
				++waypoint;
				continue;
			}
			Vector3 direction = delta.normalized() * MIN((real_t)7.0, delta.length() / (real_t)get_physics_process_delta_time());
			velocity.x = direction.x;
			velocity.z = direction.z;
			break;
		}
		velocity.y = hero->is_on_floor() ? -0.5 : velocity.y - 24.0 * get_physics_process_delta_time();
		hero->set_velocity(velocity);
		hero->move_and_slide();
	} else if (p_what == NOTIFICATION_INTERNAL_PROCESS && camera && hero) {
		Vector3 target = hero->get_global_position();
		camera->set_global_position(target + Vector3(0, 28, 22));
		camera->look_at(target);
	}
}

void MobaMapPreview3D::unhandled_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mouse = p_event;
	if (!map || !camera || mouse.is_null() || !mouse->is_pressed()) {
		return;
	}
	if (mouse->get_button_index() == MouseButton::WHEEL_UP || mouse->get_button_index() == MouseButton::WHEEL_DOWN) {
		camera->set_size(CLAMP(camera->get_size() + (mouse->get_button_index() == MouseButton::WHEEL_UP ? -3.0 : 3.0), 10.0, 120.0));
		get_viewport()->set_input_as_handled();
		return;
	}
	if (mouse->get_button_index() != MouseButton::LEFT || !placed || !map->is_navigation_ready()) {
		return;
	}
	Vector3 hit;
	if (!map->raycast(camera->project_ray_origin(mouse->get_position()), camera->project_ray_normal(mouse->get_position()), hit)) {
		return;
	}
	NavigationServer3D *server = NavigationServer3D::get_singleton();
	RID nav_map = map->get_world_3d()->get_navigation_map();
	Vector3 target = server->map_get_closest_point(nav_map, hit);
	Vector<Vector3> candidate = server->map_get_path(nav_map, hero->get_global_position(), target, true);
	if (target.distance_to(hit) > 1.0 || candidate.is_empty() || candidate[candidate.size() - 1].distance_to(target) > 0.5) {
		status->set_text("No walkable route to that point.");
		path.clear();
	} else {
		path = candidate;
		waypoint = 0;
		status->set_text("Click ground: move  |  Wheel: zoom  |  Camera follows the hero");
	}
	get_viewport()->set_input_as_handled();
}

void MobaMapPreview3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_map_path", "path"), &MobaMapPreview3D::set_map_path);
	ClassDB::bind_method(D_METHOD("get_map_path"), &MobaMapPreview3D::get_map_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "map_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MobaMap3D"), "set_map_path", "get_map_path");
}
