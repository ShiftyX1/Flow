#include "voxel_scene_format_loader.h"

#include "core/io/file_access.h"

namespace {
constexpr uint32_t VOXSCENE_MAGIC = 0x43535856; // 'V''X''S''C' little-endian.
constexpr uint32_t VOXSCENE_VERSION = 1;
} // namespace

// =================== Loader ===================

void ResourceFormatLoaderVoxelScene::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("voxscene");
}

bool ResourceFormatLoaderVoxelScene::handles_type(const String &p_type) const {
	return p_type == "VoxelSceneData" || p_type == "Resource";
}

String ResourceFormatLoaderVoxelScene::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "voxscene") {
		return "VoxelSceneData";
	}
	return "";
}

Ref<Resource> ResourceFormatLoaderVoxelScene::load(const String &p_path, const String &p_original_path,
		Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		if (r_error) {
			*r_error = err;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Cannot open .voxscene file: %s", p_path));
	}

	uint32_t magic = f->get_32();
	if (magic != VOXSCENE_MAGIC) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Invalid .voxscene magic in: %s", p_path));
	}
	uint32_t version = f->get_32();
	if (version > VOXSCENE_VERSION) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat(".voxscene file version %d is newer than supported %d.", version, VOXSCENE_VERSION));
	}

	int32_t sx = (int32_t)f->get_32();
	int32_t sy = (int32_t)f->get_32();
	int32_t sz = (int32_t)f->get_32();
	if (sx <= 0 || sy <= 0 || sz <= 0) {
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		ERR_FAIL_V_MSG(Ref<Resource>(), vformat("Invalid .voxscene size %dx%dx%d.", sx, sy, sz));
	}

	uint32_t palette_count = f->get_32();
	Vector<int> palette_remap; // palette index -> block id (== palette index for now; reserved for name-based remap).
	palette_remap.resize(palette_count);
	for (uint32_t i = 0; i < palette_count; i++) {
		uint16_t name_len = f->get_16();
		Vector<uint8_t> name_bytes;
		name_bytes.resize(name_len);
		if (name_len > 0) {
			f->get_buffer(name_bytes.ptrw(), name_len);
		}
		// Block name string is preserved for future remapping (e.g. across registry reorderings).
		// For v1 the palette index is treated as the block ID directly.
		palette_remap.write[i] = (int)i;
	}

	Ref<VoxelSceneData> data;
	data.instantiate();
	data->set_size(Vector3i(sx, sy, sz));

	int total = sx * sy * sz;
	Vector<uint16_t> &dst = data->get_blocks_array_mutable();
	if (dst.size() != total) {
		dst.resize(total);
	}
	uint16_t *w = dst.ptrw();

	int written = 0;
	while (written < total) {
		if (f->eof_reached()) {
			if (r_error) {
				*r_error = ERR_FILE_CORRUPT;
			}
			ERR_FAIL_V_MSG(Ref<Resource>(), vformat(".voxscene RLE truncated: wrote %d / %d.", written, total));
		}
		uint16_t block_id = f->get_16();
		uint16_t run = f->get_16();
		if (run == 0) {
			if (r_error) {
				*r_error = ERR_FILE_CORRUPT;
			}
			ERR_FAIL_V_MSG(Ref<Resource>(), ".voxscene RLE: zero run length.");
		}
		// Apply palette remap if present.
		uint16_t resolved = (palette_count > 0 && block_id < palette_count)
				? (uint16_t)palette_remap[block_id]
				: block_id;
		int end = MIN(written + (int)run, total);
		for (int i = written; i < end; i++) {
			w[i] = resolved;
		}
		written = end;
	}

	if (r_error) {
		*r_error = OK;
	}
	data->set_path(p_original_path.is_empty() ? p_path : p_original_path, true);
	return data;
}

// =================== Saver ===================

bool ResourceFormatSaverVoxelScene::recognize(const Ref<Resource> &p_resource) const {
	return p_resource.is_valid() && Object::cast_to<VoxelSceneData>(p_resource.ptr()) != nullptr;
}

void ResourceFormatSaverVoxelScene::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource)) {
		p_extensions->push_back("voxscene");
	}
}

Error ResourceFormatSaverVoxelScene::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<VoxelSceneData> data = p_resource;
	ERR_FAIL_COND_V_MSG(data.is_null(), ERR_INVALID_PARAMETER, "Resource is not a VoxelSceneData.");

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	if (f.is_null()) {
		ERR_FAIL_V_MSG(err, vformat("Cannot open .voxscene for write: %s", p_path));
	}

	Vector3i size = data->get_size();
	const Vector<uint16_t> &blocks = data->get_blocks_array();
	int total = size.x * size.y * size.z;
	ERR_FAIL_COND_V_MSG(blocks.size() != total, ERR_INVALID_DATA,
			vformat("VoxelSceneData blocks count %d != volume %d.", blocks.size(), total));

	f->store_32(VOXSCENE_MAGIC);
	f->store_32(VOXSCENE_VERSION);
	f->store_32((uint32_t)size.x);
	f->store_32((uint32_t)size.y);
	f->store_32((uint32_t)size.z);
	// Palette count = 0 for v1 (store raw block IDs). Reserved for future use.
	f->store_32(0);

	const uint16_t *r = blocks.ptr();
	int i = 0;
	while (i < total) {
		uint16_t cur = r[i];
		int run_start = i;
		// Cap run length at uint16 max.
		while (i < total && r[i] == cur && (i - run_start) < 0xFFFF) {
			i++;
		}
		f->store_16(cur);
		f->store_16((uint16_t)(i - run_start));
	}

	return OK;
}
