/**************************************************************************/
/*  test_voxel_world_save.h                                               */
/**************************************************************************/

#pragma once

#include "../voxel_terrain_generator.h"
#include "../voxel_world.h"
#include "../voxel_world_save.h"

#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
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
