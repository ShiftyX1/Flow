#pragma once

#include "voxel_scene_data.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

// Custom binary container for VoxelSceneData (.voxscene).
//
// Layout (little-endian):
//   [0..3]  Magic "VXSC"
//   [4..7]  uint32 version (current: 1)
//   [8..11] int32  size_x
//   [12..15] int32 size_y
//   [16..19] int32 size_z
//   [20..23] uint32 palette_count (0 = no palette, store raw block IDs)
//   palette: for i in [0..palette_count):
//       uint16 string_byte_len
//       <string_byte_len bytes UTF-8 block name>
//   data: RLE-compressed pairs until size_x*size_y*size_z blocks decoded:
//       uint16 block_id (or palette index if palette_count > 0)
//       uint16 run_length (>= 1)
//
// Indexing matches VoxelSceneData::index_of: index = (y * size_z + z) * size_x + x.
class ResourceFormatLoaderVoxelScene : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr,
			bool p_use_sub_threads = false, float *r_progress = nullptr,
			CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverVoxelScene : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
};
