#pragma once

#include "core/io/resource.h"
#include "core/math/color.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"

// The only hardcoded block convention: ID 0 is always AIR.
static constexpr int VOXEL_BLOCK_AIR = 0;

// Default built-in block IDs (assigned by setup_defaults()).
// These are convenience constants for engine code; the actual block properties
// are defined in the registry at runtime and can be customised.
static constexpr int VOXEL_BLOCK_GRASS = 1;
static constexpr int VOXEL_BLOCK_DIRT = 2;
static constexpr int VOXEL_BLOCK_STONE = 3;
static constexpr int VOXEL_BLOCK_SAND = 4;
static constexpr int VOXEL_BLOCK_WATER = 5;
static constexpr int VOXEL_BLOCK_SNOW = 6;
static constexpr int VOXEL_BLOCK_WOOD = 7;
static constexpr int VOXEL_BLOCK_LEAVES = 8;
static constexpr int VOXEL_BLOCK_BEDROCK = 9;
static constexpr int VOXEL_BLOCK_TORCH = 10;

// Dynamic block registry: single source of truth for all block definitions.
// Blocks are registered at startup, then finalize() builds flat cache arrays
// for thread-safe, lock-free reads during meshing/lighting.
//
// Typical usage:
//   registry->register_block("air", { "solid": false, "transparent": true });
//   registry->register_block("grass", { "solid": true, "color": Color(0.36, 0.63, 0.20) });
//   registry->finalize();
class VoxelBlockRegistry : public Resource {
	GDCLASS(VoxelBlockRegistry, Resource);

public:
	struct BlockEntry {
		String name;
		// --- Visual ---
		Ref<Texture2D> texture_top;
		Ref<Texture2D> texture_side;
		Ref<Texture2D> texture_bottom;
		Ref<ShaderMaterial> shader_material;
		float mesh_height = 1.0f;
		float collision_height = 1.0f;
		// --- Physics & identity ---
		Color color = Color(1, 1, 1);
		bool solid = true;
		bool transparent = false;
		bool uses_alpha = false;
		// --- Lighting ---
		uint8_t light_opacity = 15;
		uint8_t emission = 0;
		Color light_color = Color(1, 1, 1);
	};

private:
	Vector<BlockEntry> blocks;
	HashMap<String, int> name_to_id;
	bool finalized = false;

	// Flat cache arrays — built by finalize(), immutable afterwards (thread-safe).
	Vector<Color> cache_colors;
	Vector<bool> cache_solid;
	Vector<bool> cache_transparent;
	Vector<bool> cache_uses_alpha;
	Vector<uint8_t> cache_emission;
	Vector<uint8_t> cache_light_opacity;
	Vector<Color> cache_light_color;

	void _build_cache();

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	// --- Registration API ---
	int register_block(const String &p_name, const Dictionary &p_properties = Dictionary());
	int register_block_at(const String &p_name, int p_id, const Dictionary &p_properties = Dictionary());
	void finalize();
	bool is_finalized() const { return finalized; }

	// --- Query API ---
	int get_block_id(const String &p_name) const;
	int get_block_count() const;
	String get_block_name(int p_id) const;
	bool has_block(int p_id) const;
	PackedInt32Array get_block_list() const;

	// --- Per-property setters (pre-finalize only, emit_changed) ---
	void set_block_texture_top(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_top(int p_id) const;

	void set_block_texture_side(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_side(int p_id) const;

	void set_block_texture_bottom(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_bottom(int p_id) const;

	Ref<Texture2D> get_block_texture_for_face(int p_id, const Vector3 &p_normal) const;
	bool block_has_texture(int p_id) const;

	void set_block_shader_material(int p_id, const Ref<ShaderMaterial> &p_material);
	Ref<ShaderMaterial> get_block_shader_material(int p_id) const;
	bool block_has_shader(int p_id) const;

	void set_block_mesh_height(int p_id, float p_height);
	float get_block_mesh_height(int p_id) const;

	void set_block_collision_height(int p_id, float p_height);
	float get_block_collision_height(int p_id) const;

	void set_block_color(int p_id, const Color &p_color);
	Color get_block_color(int p_id) const;

	void set_block_solid(int p_id, bool p_solid);
	bool get_block_solid(int p_id) const;

	void set_block_transparent(int p_id, bool p_transparent);
	bool get_block_transparent(int p_id) const;

	void set_block_uses_alpha(int p_id, bool p_uses_alpha);
	bool get_block_uses_alpha(int p_id) const;

	void set_block_light_opacity(int p_id, int p_opacity);
	int get_block_light_opacity(int p_id) const;

	void set_block_emission(int p_id, int p_emission);
	int get_block_emission(int p_id) const;

	void set_block_light_color(int p_id, const Color &p_color);
	Color get_block_light_color(int p_id) const;

	void set_block_name(int p_id, const String &p_name);

	// Legacy compat: create_block / remove_block
	void create_block(int p_id);
	void remove_block(int p_id);

	// Populate with 11 built-in block types (air, grass, dirt, stone, sand, water, snow, wood, leaves, bedrock, torch).
	void setup_defaults();

	// --- Fast inline cache accessors (safe after finalize(), thread-safe) ---
	_FORCE_INLINE_ bool is_solid(int p_id) const {
		return p_id >= 0 && p_id < cache_solid.size() && cache_solid[p_id];
	}
	_FORCE_INLINE_ bool is_transparent(int p_id) const {
		return p_id < 0 || p_id >= cache_transparent.size() || cache_transparent[p_id];
	}
	_FORCE_INLINE_ bool is_uses_alpha(int p_id) const {
		return p_id >= 0 && p_id < cache_uses_alpha.size() && cache_uses_alpha[p_id];
	}
	_FORCE_INLINE_ uint8_t get_emission(int p_id) const {
		if (p_id >= 0 && p_id < cache_emission.size()) {
			return cache_emission[p_id];
		}
		return 0;
	}
	_FORCE_INLINE_ uint8_t get_light_opacity(int p_id) const {
		if (p_id >= 0 && p_id < cache_light_opacity.size()) {
			return cache_light_opacity[p_id];
		}
		return 15;
	}
	_FORCE_INLINE_ Color get_color(int p_id) const {
		if (p_id >= 0 && p_id < cache_colors.size()) {
			return cache_colors[p_id];
		}
		return Color(1, 1, 1);
	}
	_FORCE_INLINE_ Color get_cached_light_color(int p_id) const {
		if (p_id >= 0 && p_id < cache_light_color.size()) {
			return cache_light_color[p_id];
		}
		return Color(1, 1, 1);
	}

	// Direct access to cache arrays for passing to worker threads.
	const Vector<bool> &get_cache_solid() const { return cache_solid; }
	const Vector<bool> &get_cache_transparent() const { return cache_transparent; }
	const Vector<uint8_t> &get_cache_emission() const { return cache_emission; }
	const Vector<uint8_t> &get_cache_light_opacity() const { return cache_light_opacity; }

	VoxelBlockRegistry();
};
