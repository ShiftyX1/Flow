/**************************************************************************/
/*  test_voxel_world_save.h                                               */
/**************************************************************************/

#pragma once

#include "../voxel_terrain_generator.h"
#include "../voxel_world_save.h"

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

} // namespace TestVoxelWorldSave
