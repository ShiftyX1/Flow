/**************************************************************************/
/*  test_voxel_world_save.h                                               */
/**************************************************************************/

#pragma once

#include "../voxel_terrain_generator.h"
#include "../voxel_mesher.h"
#include "../voxel_structure_registry.h"
#include "../voxel_world.h"
#include "../voxel_world_save.h"

#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "tests/test_macros.h"

namespace TestVoxelWorldSave {

TEST_CASE("[VoxelWorld] Chunk save path and round-trip") {
	const String root = "user://voxel_world_save_test";
	const Vector2i chunk_key(-2, 5);
	const String chunk_path = VoxelWorldSave::get_chunk_file_path(root, chunk_key);

	CHECK(chunk_path.ends_with("chunks/-2_5.voxchunk"));
	CHECK(DirAccess::make_dir_recursive_absolute(chunk_path.get_base_dir()) == OK);

	const int total = VoxelTerrainGenerator::CHUNK_SIZE_X * VoxelTerrainGenerator::CHUNK_SIZE_Y * VoxelTerrainGenerator::CHUNK_SIZE_Z;
	Vector<uint16_t> blocks;
	blocks.resize(total);
	for (int i = 0; i < total; i++) {
		blocks.write[i] = (i % 97 == 0) ? 8 : 0;
	}

	CHECK(VoxelWorldSave::save_chunk_file(chunk_path, blocks) == OK);
	CHECK(FileAccess::exists(chunk_path));

	Vector<uint16_t> loaded;
	CHECK(VoxelWorldSave::load_chunk_file(chunk_path, &loaded) == OK);
	REQUIRE(loaded.size() == blocks.size());
	for (int i = 0; i < total; i++) {
		CHECK(loaded[i] == blocks[i]);
	}

	blocks.resize(total - 1);
	CHECK(VoxelWorldSave::save_chunk_file(chunk_path, blocks) == ERR_INVALID_DATA);
	DirAccess::remove_absolute(chunk_path);
}

TEST_CASE("[VoxelWorld] Block registry stores world-foundation properties") {
	Ref<VoxelBlockRegistry> registry;
	registry.instantiate();

	PackedStringArray tags;
	tags.push_back("ruin");
	tags.push_back("metal");

	Dictionary props;
	props["shape"] = "pane";
	props["tags"] = tags;
	props["resource_id"] = "scrap_metal";
	props["hazard_id"] = "electric";
	props["hazard_strength"] = 2.5f;
	props["is_fluid"] = false;
	props["replaceable"] = true;

	const int id = registry->register_block("rusted_panel", props);
	CHECK(registry->get_block_shape(id) == VoxelBlockRegistry::BLOCK_SHAPE_PANE);
	CHECK(registry->block_has_tag(id, "ruin"));
	CHECK(registry->get_block_resource_id(id) == StringName("scrap_metal"));
	CHECK(registry->get_block_hazard_id(id) == StringName("electric"));
	CHECK(Math::is_equal_approx(registry->get_block_hazard_strength(id), 2.5f));
	CHECK(!registry->get_block_is_fluid(id));
	CHECK(registry->get_block_replaceable(id));
}

TEST_CASE("[VoxelWorld] Block registry stores model visual properties") {
	Ref<VoxelBlockRegistry> registry;
	registry.instantiate();

	Ref<BoxMesh> mesh;
	mesh.instantiate();

	Node3D *root = memnew(Node3D);
	AnimationPlayer *player = memnew(AnimationPlayer);
	root->add_child(player);
	Ref<PackedScene> scene;
	scene.instantiate();
	CHECK(scene->pack(root) == OK);
	memdelete(root);

	Dictionary animation_map;
	animation_map["idle"] = "Idle";
	animation_map["active"] = "Active";

	Dictionary props;
	props["visual_mode"] = "model_scene";
	props["model_mesh"] = mesh;
	props["model_scene"] = scene;
	props["model_offset"] = Vector3(0.0f, 0.25f, 0.0f);
	props["model_rotation_y"] = 90.0f;
	props["model_scale"] = Vector3(1.5f, 1.0f, 1.5f);
	props["animation_state_map"] = animation_map;

	const int id = registry->register_block("animated_beacon", props);
	CHECK(registry->get_block_visual_mode(id) == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE);
	CHECK(registry->get_block_model_mesh(id) == mesh);
	CHECK(registry->get_block_model_scene(id) == scene);
	CHECK(registry->get_block_model_offset(id) == Vector3(0.0f, 0.25f, 0.0f));
	CHECK(Math::is_equal_approx(registry->get_block_model_rotation_y(id), 90.0f));
	CHECK(registry->get_block_model_scale(id) == Vector3(1.5f, 1.0f, 1.5f));
	Dictionary loaded_animation_map = registry->get_block_animation_state_map(id);
	CHECK(String(loaded_animation_map["active"]) == "Active");
	CHECK(registry->block_has_model_visual(id));
}

TEST_CASE("[VoxelWorld] Model visual blocks are skipped by the chunk mesher") {
	Ref<VoxelBlockRegistry> registry;
	registry.instantiate();
	registry->setup_defaults();

	Ref<BoxMesh> mesh;
	mesh.instantiate();

	Dictionary props;
	props["visual_mode"] = "model_mesh";
	props["model_mesh"] = mesh;
	const int model_id = registry->register_block("model_marker", props);
	registry->finalize();

	const int total = VoxelTerrainGenerator::CHUNK_SIZE_X * VoxelTerrainGenerator::CHUNK_SIZE_Y * VoxelTerrainGenerator::CHUNK_SIZE_Z;
	Vector<uint16_t> blocks;
	blocks.resize(total);
	for (int i = 0; i < total; i++) {
		blocks.write[i] = VOXEL_BLOCK_AIR;
	}

	blocks.write[VoxelTerrainGenerator::block_index(1, 1, 1)] = model_id;
	Vector<VoxelMesher::MeshSurface> model_surfaces = VoxelMesher::build_chunk_mesh(blocks, 1.0f, registry, VoxelMesher::NeighborBlocks());
	CHECK(model_surfaces.is_empty());

	blocks.write[VoxelTerrainGenerator::block_index(1, 1, 1)] = VOXEL_BLOCK_STONE;
	Vector<VoxelMesher::MeshSurface> voxel_surfaces = VoxelMesher::build_chunk_mesh(blocks, 1.0f, registry, VoxelMesher::NeighborBlocks());
	CHECK(!voxel_surfaces.is_empty());
}

TEST_CASE("[VoxelWorld] Default content slice exposes restored forest beacon data") {
	Ref<VoxelBlockRegistry> blocks;
	blocks.instantiate();
	blocks->setup_defaults();

	CHECK(blocks->get_block_shape(VOXEL_BLOCK_BIOLUMEN_PLANT) == VoxelBlockRegistry::BLOCK_SHAPE_CROSS_PLANT);
	CHECK(blocks->block_has_tag(VOXEL_BLOCK_BIOLUMEN_PLANT, "restored_forest"));
	CHECK(blocks->get_block_resource_id(VOXEL_BLOCK_BIO_RESIN) == StringName("bio_resin"));
	CHECK(blocks->block_has_tag(VOXEL_BLOCK_BEACON_CORE, "signal"));

	Ref<VoxelStructureRegistry> structures;
	structures.instantiate();
	structures->setup_defaults();
	REQUIRE(structures->get_structure_count() == 1);

	Dictionary beacon = structures->get_structure(0);
	CHECK(String(beacon["name"]) == "restored_forest_emergency_beacon");
	CHECK(String(beacon["world_object_type"]) == "emergency_beacon");
	CHECK((bool)((Dictionary)beacon["world_object_state"])["active"]);

	Ref<VoxelSceneData> data = beacon["voxel_data"];
	REQUIRE(data.is_valid());
	CHECK(data->get_block(3, 1, 3) == VOXEL_BLOCK_BEACON_CORE);
	CHECK(data->get_block_count_non_air() > 0);
}

TEST_CASE("[VoxelWorld] Chunk object save round-trip") {
	const String root = "user://voxel_world_objects_test";
	const Vector2i chunk_key(3, -1);
	const String object_path = VoxelWorldSave::get_chunk_objects_file_path(root, chunk_key);

	Dictionary state;
	state["locked"] = true;
	state["signal"] = "distress";

	Dictionary object;
	object["id"] = (int64_t)42;
	object["type"] = "emergency_beacon";
	object["block_pos"] = Vector3i(49, 66, -12);
	object["rotation_y"] = 90.0f;
	object["state"] = state;
	object["blocking"] = false;

	Array objects;
	objects.push_back(object);
	CHECK(VoxelWorldSave::save_chunk_objects_file(object_path, objects) == OK);

	Array loaded;
	CHECK(VoxelWorldSave::load_chunk_objects_file(object_path, &loaded) == OK);
	REQUIRE(loaded.size() == 1);
	Dictionary loaded_object = loaded[0];
	CHECK((int64_t)loaded_object["id"] == 42);
	CHECK(String(loaded_object["type"]) == "emergency_beacon");
	CHECK((Vector3i)loaded_object["block_pos"] == Vector3i(49, 66, -12));
	CHECK((bool)((Dictionary)loaded_object["state"])["locked"]);

	DirAccess::remove_absolute(object_path);
}

TEST_CASE("[VoxelWorld] Blocking world objects participate in body movement") {
	VoxelWorld world;
	const int64_t object_id = world.add_world_object(StringName("test_door"), Vector3i(1, 0, 0), 0.0f, Dictionary(), true);

	AABB body(Vector3(0, 0, 0), Vector3(0.9f, 0.9f, 0.9f));
	Dictionary result = world.move_body(body, Vector3(1, 0, 0), 1.0f);
	Vector3 position = result["position"];
	Vector3 velocity = result["velocity"];

	CHECK(Math::is_equal_approx(position.x, 0.1f));
	CHECK(Math::is_equal_approx(velocity.x, 0.0f));

	CHECK(world.set_world_object_blocking(object_id, false));
	result = world.move_body(body, Vector3(1, 0, 0), 1.0f);
	position = result["position"];
	velocity = result["velocity"];

	CHECK(Math::is_equal_approx(position.x, 1.0f));
	CHECK(Math::is_equal_approx(velocity.x, 1.0f));
}

TEST_CASE("[VoxelWorld] Corrupt chunk RLE is rejected") {
	const String root = "user://voxel_world_save_corrupt_test";
	const String chunk_path = VoxelWorldSave::get_chunk_file_path(root, Vector2i(0, 0));
	CHECK(DirAccess::make_dir_recursive_absolute(chunk_path.get_base_dir()) == OK);

	Error err;
	Ref<FileAccess> f = FileAccess::open(chunk_path, FileAccess::WRITE, &err);
	REQUIRE(err == OK);
	REQUIRE(f.is_valid());
	f->store_32(VoxelWorldSave::VOXCHUNK_MAGIC);
	f->store_32(VoxelWorldSave::VOXCHUNK_VERSION);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_X);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_Y);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_Z);
	f->store_16(1);
	f->store_16(0xFFFF);
	f.unref();

	Vector<uint16_t> loaded;
	CHECK(VoxelWorldSave::load_chunk_file(chunk_path, &loaded) == ERR_FILE_CORRUPT);
	DirAccess::remove_absolute(chunk_path);
}

TEST_CASE("[VoxelWorld] Save API exposes budgeted dirty chunk flushing") {
	VoxelWorld world;

	CHECK(world.get_world_save_dirty_chunk_count() == 0);
	CHECK(world.flush_world_save_dirty_chunks(1) == OK);
	CHECK(world.save_world_state(Dictionary(), Dictionary(), 0) == ERR_UNCONFIGURED);
}

TEST_CASE("[VoxelWorld] Chunk region loading status exposes safe spawn progress") {
	VoxelWorld world;
	world.set_chunk_load_radius(3);

	Dictionary status = world.get_chunk_region_load_status(Vector3(0, 0, 0), 1);
	CHECK((int)status["total"] == 9);
	CHECK((int)status["loaded"] == 0);
	CHECK((int)status["remaining"] == 9);
	CHECK(Math::is_equal_approx((float)status["progress"], 0.0f));
	CHECK(!(bool)status["ready"]);
	CHECK(!world.is_chunk_region_loaded(Vector3(0, 0, 0), 1));

	Dictionary default_radius_status = world.get_chunk_region_load_status(Vector3(0, 0, 0), -1);
	CHECK((int)default_radius_status["total"] == 49);
}

TEST_CASE("[VoxelWorld] Streaming budgets cap initial chunk task pressure") {
	VoxelWorld world;
	world.set_chunk_load_radius(8);
	world.set_max_pending_chunk_tasks(2);
	world.set_chunk_requests_per_frame(8);
	world.set_chunks_per_frame(1);
	world.set_remesh_requests_per_frame(1);
	world.set_remeshes_per_frame(1);
	world.set_chunk_integration_time_budget_usec(1000);

	world.request_chunks_around(Vector3(0, 0, 0), 8);

	Dictionary metrics = world.get_debug_metrics();
	CHECK((int)metrics["max_pending_chunk_tasks"] == 2);
	CHECK((int)metrics["chunk_requests_per_frame"] == 8);
	CHECK((int)metrics["chunks_per_frame"] == 1);
	CHECK((int)metrics["remesh_requests_per_frame"] == 1);
	CHECK((int)metrics["remeshes_per_frame"] == 1);
	CHECK((int)metrics["integration_time_budget_usec"] == 1000);
	CHECK((int)metrics["pending_chunk_tasks"] <= 2);
}

TEST_CASE("[VoxelWorld] Debug metrics expose streaming counters") {
	VoxelWorld world;
	Dictionary metrics = world.get_debug_metrics();
	CHECK(metrics.has("loaded_chunks"));
	CHECK(metrics.has("pending_chunk_tasks"));
	CHECK(metrics.has("pending_remesh_tasks"));
	CHECK(metrics.has("finished_chunk_results"));
	CHECK(metrics.has("finished_remesh_results"));
	CHECK(metrics.has("dirty_light_chunks"));
	CHECK(metrics.has("last_integrated_loads"));
	CHECK(metrics.has("last_integrated_remeshes"));
	CHECK(metrics.has("last_remesh_requests"));
	CHECK(metrics.has("last_integration_time_usec"));
}

TEST_CASE("[VoxelWorld] Loaded save time becomes current and start time") {
	const String root = "user://voxel_world_time_test";
	const String metadata_path = VoxelWorldSave::get_metadata_path(root);
	CHECK(VoxelWorldSave::ensure_save_dirs(root) == OK);

	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("world", "schema_version", 1);
	config->set_value("world", "slot_id", root.get_file());
	config->set_value("world", "display_name", "Time Test");
	config->set_value("world", "seed", 1234);
	config->set_value("world", "world_time", 18.25);
	config->set_value("world", "time_of_day", 18.25);
	config->set_value("world", "created_unix", 1.0);
	config->set_value("world", "updated_unix", 2.0);
	config->set_value("player", "state", Dictionary());
	config->set_value("character", "state", Dictionary());
	CHECK(config->save(metadata_path) == OK);

	VoxelWorld world;
	world.set_start_time_of_day(6.0f);
	CHECK(world.load_world_save(root) == OK);
	CHECK(Math::is_equal_approx(world.get_time_of_day(), 18.25f));
	CHECK(Math::is_equal_approx(world.get_start_time_of_day(), 18.25f));

	DirAccess::remove_absolute(metadata_path);
}

} // namespace TestVoxelWorldSave
