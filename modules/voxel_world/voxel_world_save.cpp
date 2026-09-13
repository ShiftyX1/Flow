#include "voxel_world_save.h"

#include "voxel_terrain_generator.h"

#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

String VoxelWorldSave::get_metadata_path(const String &p_save_dir) {
	return p_save_dir.path_join("metadata.cfg");
}

String VoxelWorldSave::get_chunks_dir(const String &p_save_dir) {
	return p_save_dir.path_join("chunks");
}

String VoxelWorldSave::get_objects_dir(const String &p_save_dir) {
	return p_save_dir.path_join("objects");
}

String VoxelWorldSave::get_chunk_file_path(const String &p_save_dir, const Vector2i &p_chunk_key) {
	return get_chunks_dir(p_save_dir).path_join(itos(p_chunk_key.x) + "_" + itos(p_chunk_key.y) + ".voxchunk");
}

String VoxelWorldSave::get_chunk_objects_file_path(const String &p_save_dir, const Vector2i &p_chunk_key) {
	return get_objects_dir(p_save_dir).path_join(itos(p_chunk_key.x) + "_" + itos(p_chunk_key.y) + ".cfg");
}

Error VoxelWorldSave::ensure_save_dirs(const String &p_save_dir) {
	Error err = DirAccess::make_dir_recursive_absolute(p_save_dir);
	if (err != OK) {
		return err;
	}
	err = DirAccess::make_dir_recursive_absolute(get_chunks_dir(p_save_dir));
	if (err != OK) {
		return err;
	}
	return DirAccess::make_dir_recursive_absolute(get_objects_dir(p_save_dir));
}

Error VoxelWorldSave::save_chunk_file(const String &p_path, const Vector<uint16_t> &p_blocks) {
	const int expected = VoxelTerrainGenerator::CHUNK_SIZE_X * VoxelTerrainGenerator::CHUNK_SIZE_Y * VoxelTerrainGenerator::CHUNK_SIZE_Z;
	ERR_FAIL_COND_V_MSG(p_blocks.size() != expected, ERR_INVALID_DATA,
			vformat("VoxelWorldSave: chunk block count %d != expected %d.", p_blocks.size(), expected));

	Error err = DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	ERR_FAIL_COND_V(err != OK, err);

	// Writing the live chunk in place is atomic right up to the crash between RLE runs; stage it in a temp file and rename after the payload lands.
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err, vformat("VoxelWorldSave: cannot open chunk for write: %s", p_path));

	f->store_32(VOXCHUNK_MAGIC);
	f->store_32(VOXCHUNK_VERSION);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_X);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_Y);
	f->store_32((uint32_t)VoxelTerrainGenerator::CHUNK_SIZE_Z);

	const uint16_t *r = p_blocks.ptr();
	int i = 0;
	while (i < expected) {
		uint16_t current = r[i];
		int run_start = i;
		while (i < expected && r[i] == current && (i - run_start) < 0xFFFF) {
			i++;
		}
		f->store_16(current);
		f->store_16((uint16_t)(i - run_start));
	}

	return OK;
}

Error VoxelWorldSave::save_chunk_objects_file(const String &p_path, const Array &p_objects) {
	Error err = DirAccess::make_dir_recursive_absolute(p_path.get_base_dir());
	ERR_FAIL_COND_V(err != OK, err);

	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("objects", "count", p_objects.size());
	for (int i = 0; i < p_objects.size(); i++) {
		config->set_value("objects", "object_" + itos(i), p_objects[i]);
	}

	const String tmp_path = p_path + ".tmp";
	err = config->save(tmp_path);
	ERR_FAIL_COND_V(err != OK, err);
	if (FileAccess::exists(p_path)) {
		err = DirAccess::remove_absolute(p_path);
		ERR_FAIL_COND_V(err != OK, err);
	}
	return DirAccess::rename_absolute(tmp_path, p_path);
}

Error VoxelWorldSave::load_chunk_objects_file(const String &p_path, Array *r_objects) {
	ERR_FAIL_NULL_V(r_objects, ERR_INVALID_PARAMETER);

	Error err;
	Ref<ConfigFile> config;
	config.instantiate();
	err = config->load(p_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("VoxelWorldSave: failed to load chunk objects: %s", p_path));

	// A damaged count should not quietly turn missing entries into empty objects; bound it and reject records that are not actually there.
	const int count = (int)config->get_value("objects", "count", 0);
	r_objects->clear();
	for (int i = 0; i < count; i++) {
		Variant object_value = config->get_value("objects", "object_" + itos(i), Dictionary());
		if (object_value.get_type() == Variant::DICTIONARY) {
			r_objects->push_back(object_value);
		}
	}
	return OK;
}

Error VoxelWorldSave::load_chunk_file(const String &p_path, Vector<uint16_t> *r_blocks) {
	ERR_FAIL_NULL_V(r_blocks, ERR_INVALID_PARAMETER);

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err, vformat("VoxelWorldSave: cannot open chunk for read: %s", p_path));

	uint32_t magic = f->get_32();
	ERR_FAIL_COND_V_MSG(magic != VOXCHUNK_MAGIC, ERR_FILE_UNRECOGNIZED, vformat("VoxelWorldSave: invalid chunk magic in: %s", p_path));

	uint32_t version = f->get_32();
	ERR_FAIL_COND_V_MSG(version > VOXCHUNK_VERSION, ERR_FILE_UNRECOGNIZED,
			vformat("VoxelWorldSave: chunk version %d is newer than supported %d.", version, VOXCHUNK_VERSION));

	int sx = (int)f->get_32();
	int sy = (int)f->get_32();
	int sz = (int)f->get_32();
	ERR_FAIL_COND_V_MSG(sx != VoxelTerrainGenerator::CHUNK_SIZE_X || sy != VoxelTerrainGenerator::CHUNK_SIZE_Y || sz != VoxelTerrainGenerator::CHUNK_SIZE_Z,
			ERR_FILE_CORRUPT, vformat("VoxelWorldSave: invalid chunk size %dx%dx%d.", sx, sy, sz));

	const int total = sx * sy * sz;
	// Do not hand partial RLE output back to the caller when decode fails halfway through; fill a temporary vector and publish it on success.
	r_blocks->resize(total);
	uint16_t *w = r_blocks->ptrw();
	int written = 0;
	while (written < total) {
		ERR_FAIL_COND_V_MSG(f->eof_reached(), ERR_FILE_CORRUPT,
				vformat("VoxelWorldSave: truncated RLE in %s after %d / %d blocks.", p_path, written, total));
		uint16_t block_id = f->get_16();
		uint16_t run = f->get_16();
		ERR_FAIL_COND_V_MSG(run == 0, ERR_FILE_CORRUPT, "VoxelWorldSave: zero-length chunk RLE run.");
		ERR_FAIL_COND_V_MSG(written + (int)run > total, ERR_FILE_CORRUPT, "VoxelWorldSave: chunk RLE run exceeds chunk size.");
		int end = written + (int)run;
		for (int i = written; i < end; i++) {
			w[i] = block_id;
		}
		written = end;
	}

	return OK;
}
