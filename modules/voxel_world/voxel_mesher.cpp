#include "voxel_mesher.h"

void VoxelMesher::add_face(
		SurfaceData &r_surface,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color,
		bool p_has_uv,
		bool p_flip_v) {
	// CCW winding: tri1 = v0,v2,v1 ; tri2 = v0,v3,v2
	r_surface.vertices.push_back(p_v0);
	r_surface.vertices.push_back(p_v2);
	r_surface.vertices.push_back(p_v1);
	r_surface.vertices.push_back(p_v0);
	r_surface.vertices.push_back(p_v3);
	r_surface.vertices.push_back(p_v2);

	for (int i = 0; i < 6; i++) {
		r_surface.normals.push_back(p_normal);
		r_surface.colors.push_back(p_color);
	}

	if (p_has_uv) {
		r_surface.has_uv = true;
		if (p_flip_v) {
			// Flipped V for side faces: bottom vertices get V=1, top get V=0.
			r_surface.uvs.push_back(Vector2(0, 1));
			r_surface.uvs.push_back(Vector2(1, 0));
			r_surface.uvs.push_back(Vector2(0, 0));
			r_surface.uvs.push_back(Vector2(0, 1));
			r_surface.uvs.push_back(Vector2(1, 1));
			r_surface.uvs.push_back(Vector2(1, 0));
		} else {
			r_surface.uvs.push_back(Vector2(0, 0));
			r_surface.uvs.push_back(Vector2(1, 1));
			r_surface.uvs.push_back(Vector2(0, 1));
			r_surface.uvs.push_back(Vector2(0, 0));
			r_surface.uvs.push_back(Vector2(1, 0));
			r_surface.uvs.push_back(Vector2(1, 1));
		}
	} else {
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
	}
}

Vector<VoxelMesher::MeshSurface> VoxelMesher::build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry, uint32_t p_alpha_block_flags, const NeighborBlocks &p_neighbors) {
	const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	const uint8_t *blocks = p_blocks.ptr();

	// Surface index 0 = untextured (vertex colors).
	// Each unique Texture2D pointer gets its own surface.
	// Blocks with a ShaderMaterial get their own surface keyed by block_type.
	SurfaceData untextured_surface;
	HashMap<Ref<Texture2D>, SurfaceData> textured_surfaces;
	HashMap<Ref<Texture2D>, int> textured_block_types; // Track which block type owns each texture.

	// Shader surfaces: one per block type that has a ShaderMaterial.
	struct ShaderSurfaceInfo {
		SurfaceData surface;
		Ref<Texture2D> texture; // Primary texture for the block (top face texture, for shader uniform).
		Ref<ShaderMaterial> shader_material;
	};
	HashMap<int, ShaderSurfaceInfo> shader_surfaces; // key = block_type

	bool use_registry = p_registry.is_valid();

	for (int y = 0; y < CY; y++) {
		for (int z = 0; z < CZ; z++) {
			for (int x = 0; x < CX; x++) {
				VoxelBlockType type = (VoxelBlockType)blocks[VoxelTerrainGenerator::block_index(x, y, z)];

				if (type == VOXEL_BLOCK_AIR) {
					continue;
				}

				// Non-solid blocks (e.g. water) are only meshed if they have a shader.
				bool solid = VoxelBlockData::is_solid(type);
				if (!solid) {
					if (!use_registry || !p_registry->block_has_shader((int)type)) {
						continue;
					}
				}

				Color col = VoxelBlockData::block_colors[type];
				float bs = p_block_size;
				Vector3 origin(x * bs, y * bs, z * bs);

				// For non-solid blocks (water with shader): show face only where neighbor is air.
				// For solid blocks: show face where neighbor is transparent or alpha-flagged.
				auto should_show_face = [&](int nx, int ny, int nz) -> bool {
					VoxelBlockType neighbor = get_block(blocks, nx, ny, nz, p_neighbors);
					if (!solid) {
						// Non-solid shader blocks: only show face against air.
						return neighbor == VOXEL_BLOCK_AIR;
					}
					if (VoxelBlockData::is_transparent(neighbor)) {
						return true;
					}
					if (p_alpha_block_flags & (1 << (int)neighbor)) {
						return true;
					}
					return false;
				};

				// Check if this block type has a custom shader.
				Ref<ShaderMaterial> block_shader;
				if (use_registry) {
					block_shader = p_registry->get_block_shader_material((int)type);
				}

				// Lookup per-face textures from registry.
				Ref<Texture2D> tex_top, tex_bottom, tex_side;
				if (use_registry) {
					tex_top = p_registry->get_block_texture_for_face((int)type, Vector3(0, 1, 0));
					tex_bottom = p_registry->get_block_texture_for_face((int)type, Vector3(0, -1, 0));
					tex_side = p_registry->get_block_texture_for_face((int)type, Vector3(1, 0, 0));
				}

				if (block_shader.is_valid()) {
					// Route all faces to a shader surface keyed by block_type.
					if (!shader_surfaces.has((int)type)) {
						ShaderSurfaceInfo info;
						info.shader_material = block_shader;
						// Use top texture as the primary texture for the shader.
						info.texture = tex_top.is_valid() ? tex_top : (tex_side.is_valid() ? tex_side : tex_bottom);
						shader_surfaces[(int)type] = info;
					}
					SurfaceData *surf = &shader_surfaces[(int)type].surface;
					bool has_tex = tex_top.is_valid() || tex_side.is_valid() || tex_bottom.is_valid();
					Color face_col = has_tex ? Color(1, 1, 1) : col;

					// +X face (side)
					if (should_show_face(x + 1, y, z)) {
						add_face(*surf,
								origin + Vector3(bs, 0, 0),
								origin + Vector3(bs, bs, 0),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(bs, 0, bs),
								Vector3(1, 0, 0), face_col, has_tex, true);
					}
					// -X face (side)
					if (should_show_face(x - 1, y, z)) {
						add_face(*surf,
								origin + Vector3(0, 0, bs),
								origin + Vector3(0, bs, bs),
								origin + Vector3(0, bs, 0),
								origin + Vector3(0, 0, 0),
								Vector3(-1, 0, 0), face_col, has_tex, true);
					}
					// +Y face (top)
					if (should_show_face(x, y + 1, z)) {
						add_face(*surf,
								origin + Vector3(0, bs, 0),
								origin + Vector3(0, bs, bs),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(bs, bs, 0),
								Vector3(0, 1, 0), face_col, has_tex);
					}
					// -Y face (bottom)
					if (should_show_face(x, y - 1, z)) {
						add_face(*surf,
								origin + Vector3(0, 0, bs),
								origin + Vector3(0, 0, 0),
								origin + Vector3(bs, 0, 0),
								origin + Vector3(bs, 0, bs),
								Vector3(0, -1, 0), face_col, has_tex);
					}
					// +Z face (side)
					if (should_show_face(x, y, z + 1)) {
						add_face(*surf,
								origin + Vector3(bs, 0, bs),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(0, bs, bs),
								origin + Vector3(0, 0, bs),
								Vector3(0, 0, 1), face_col, has_tex, true);
					}
					// -Z face (side)
					if (should_show_face(x, y, z - 1)) {
						add_face(*surf,
								origin + Vector3(0, 0, 0),
								origin + Vector3(0, bs, 0),
								origin + Vector3(bs, bs, 0),
								origin + Vector3(bs, 0, 0),
								Vector3(0, 0, -1), face_col, has_tex, true);
					}
				} else {
					// No shader — use existing texture-based batching.

					// Helper: pick surface + color for a face based on its texture.
					auto get_face_surface = [&](const Ref<Texture2D> &p_tex) -> SurfaceData * {
						if (p_tex.is_valid()) {
							if (!textured_block_types.has(p_tex)) {
								textured_block_types[p_tex] = (int)type;
							}
							return &textured_surfaces[p_tex];
						}
						return &untextured_surface;
					};

					auto get_face_color = [&](const Ref<Texture2D> &p_tex) -> Color {
						if (p_tex.is_valid()) {
							return Color(1, 1, 1); // White — texture provides color.
						}
						return col;
					};

					// +X face (side)
					if (should_show_face(x + 1, y, z)) {
						add_face(*get_face_surface(tex_side), 
								origin + Vector3(bs, 0, 0),
								origin + Vector3(bs, bs, 0),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(bs, 0, bs),
								Vector3(1, 0, 0), get_face_color(tex_side), tex_side.is_valid(), true);
					}
					// -X face (side)
					if (should_show_face(x - 1, y, z)) {
						add_face(*get_face_surface(tex_side),
								origin + Vector3(0, 0, bs),
								origin + Vector3(0, bs, bs),
								origin + Vector3(0, bs, 0),
								origin + Vector3(0, 0, 0),
								Vector3(-1, 0, 0), get_face_color(tex_side), tex_side.is_valid(), true);
					}
					// +Y face (top)
					if (should_show_face(x, y + 1, z)) {
						add_face(*get_face_surface(tex_top),
								origin + Vector3(0, bs, 0),
								origin + Vector3(0, bs, bs),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(bs, bs, 0),
								Vector3(0, 1, 0), get_face_color(tex_top), tex_top.is_valid());
					}
					// -Y face (bottom)
					if (should_show_face(x, y - 1, z)) {
						add_face(*get_face_surface(tex_bottom),
								origin + Vector3(0, 0, bs),
								origin + Vector3(0, 0, 0),
								origin + Vector3(bs, 0, 0),
								origin + Vector3(bs, 0, bs),
								Vector3(0, -1, 0), get_face_color(tex_bottom), tex_bottom.is_valid());
					}
					// +Z face (side)
					if (should_show_face(x, y, z + 1)) {
						add_face(*get_face_surface(tex_side),
								origin + Vector3(bs, 0, bs),
								origin + Vector3(bs, bs, bs),
								origin + Vector3(0, bs, bs),
								origin + Vector3(0, 0, bs),
								Vector3(0, 0, 1), get_face_color(tex_side), tex_side.is_valid(), true);
					}
					// -Z face (side)
					if (should_show_face(x, y, z - 1)) {
						add_face(*get_face_surface(tex_side),
								origin + Vector3(0, 0, 0),
								origin + Vector3(0, bs, 0),
								origin + Vector3(bs, bs, 0),
								origin + Vector3(bs, 0, 0),
								Vector3(0, 0, -1), get_face_color(tex_side), tex_side.is_valid(), true);
					}
				}
			}
		}
	}

	// Build result surfaces.
	Vector<MeshSurface> result;

	// Untextured surface (vertex-colored).
	if (untextured_surface.vertices.size() > 0) {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = untextured_surface.vertices;
		arrays[Mesh::ARRAY_NORMAL] = untextured_surface.normals;
		arrays[Mesh::ARRAY_COLOR] = untextured_surface.colors;

		MeshSurface s;
		s.arrays = arrays;
		result.push_back(s);
	}

	// Textured surfaces (no custom shader).
	for (const KeyValue<Ref<Texture2D>, SurfaceData> &E : textured_surfaces) {
		if (E.value.vertices.size() == 0) {
			continue;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = E.value.vertices;
		arrays[Mesh::ARRAY_NORMAL] = E.value.normals;
		arrays[Mesh::ARRAY_COLOR] = E.value.colors;
		if (E.value.has_uv) {
			arrays[Mesh::ARRAY_TEX_UV] = E.value.uvs;
		}

		MeshSurface s;
		s.arrays = arrays;
		s.texture = E.key;
		if (textured_block_types.has(E.key)) {
			s.block_type = textured_block_types[E.key];
		}
		result.push_back(s);
	}

	// Shader surfaces (custom ShaderMaterial per block type).
	for (const KeyValue<int, ShaderSurfaceInfo> &E : shader_surfaces) {
		if (E.value.surface.vertices.size() == 0) {
			continue;
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = E.value.surface.vertices;
		arrays[Mesh::ARRAY_NORMAL] = E.value.surface.normals;
		arrays[Mesh::ARRAY_COLOR] = E.value.surface.colors;
		if (E.value.surface.has_uv) {
			arrays[Mesh::ARRAY_TEX_UV] = E.value.surface.uvs;
		}

		MeshSurface s;
		s.arrays = arrays;
		s.texture = E.value.texture;
		s.shader_material = E.value.shader_material;
		s.block_type = E.key;
		result.push_back(s);
	}

	return result;
}
