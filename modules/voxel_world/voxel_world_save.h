#pragma once

#include "core/math/vector2i.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class VoxelWorldSave {
public:
	static constexpr uint32_t VOXCHUNK_MAGIC = 0x4B484356; // 'V''C''H''K' little-endian.
	static constexpr uint32_t VOXCHUNK_VERSION = 1;

	static String get_metadata_path(const String &p_save_dir);
	static String get_chunks_dir(const String &p_save_dir);
	static String get_chunk_file_path(const String &p_save_dir, const Vector2i &p_chunk_key);
	static Error ensure_save_dirs(const String &p_save_dir);

	static Error save_chunk_file(const String &p_path, const Vector<uint16_t> &p_blocks);
	static Error load_chunk_file(const String &p_path, Vector<uint16_t> *r_blocks);
};
