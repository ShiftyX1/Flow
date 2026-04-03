#include "voxel_mesher.h"

void VoxelMesher::add_face(
		SurfaceData &r_surface,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color,
		bool p_has_uv) {
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
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(1, 1));
		r_surface.uvs.push_back(Vector2(0, 1));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(1, 0));
		r_surface.uvs.push_back(Vector2(1, 1));
	} else {
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
		r_surface.uvs.push_back(Vector2(0, 0));
	}
}

Vector<VoxelMesher::MeshSurface> VoxelMesher::build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry) {
	const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	const uint8_t *blocks = p_blocks.ptr();

	// Surface index 0 = untextured (vertex colors).
	// Each unique Texture2D pointer gets its own surface.
	SurfaceData untextured_surface;
	HashMap<Ref<Texture2D>, SurfaceData> textured_surfaces;

	bool use_registry = p_registry.is_valid();

	for (int y = 0; y < CY; y++) {
		for (int z = 0; z < CZ; z++) {
			for (int x = 0; x < CX; x++) {
				VoxelBlockType type = (VoxelBlockType)blocks[VoxelTerrainGenerator::block_index(x, y, z)];

				if (!VoxelBlockData::is_solid(type)) {
					continue;
				}

				Color col = VoxelBlockData::block_colors[type];
				float bs = p_block_size;
				Vector3 origin(x * bs, y * bs, z * bs);

				auto is_neighbor_transparent = [&](int nx, int ny, int nz) -> bool {
					VoxelBlockType neighbor = get_block(blocks, nx, ny, nz);
					return VoxelBlockData::is_transparent(neighbor);
				};

				// Lookup per-face textures from registry.
				Ref<Texture2D> tex_top, tex_bottom, tex_side;
				if (use_registry) {
					tex_top = p_registry->get_block_texture_for_face((int)type, Vector3(0, 1, 0));
					tex_bottom = p_registry->get_block_texture_for_face((int)type, Vector3(0, -1, 0));
					tex_side = p_registry->get_block_texture_for_face((int)type, Vector3(1, 0, 0));
				}

				// Helper: pick surface + color for a face based on its texture.
				auto get_face_surface = [&](const Ref<Texture2D> &p_tex) -> SurfaceData * {
					if (p_tex.is_valid()) {
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
				if (is_neighbor_transparent(x + 1, y, z)) {
					add_face(*get_face_surface(tex_side), 
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, 0, bs),
							Vector3(1, 0, 0), get_face_color(tex_side), tex_side.is_valid());
				}
				// -X face (side)
				if (is_neighbor_transparent(x - 1, y, z)) {
					add_face(*get_face_surface(tex_side),
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, 0, 0),
							Vector3(-1, 0, 0), get_face_color(tex_side), tex_side.is_valid());
				}
				// +Y face (top)
				if (is_neighbor_transparent(x, y + 1, z)) {
					add_face(*get_face_surface(tex_top),
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, bs, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, bs, 0),
							Vector3(0, 1, 0), get_face_color(tex_top), tex_top.is_valid());
				}
				// -Y face (bottom)
				if (is_neighbor_transparent(x, y - 1, z)) {
					add_face(*get_face_surface(tex_bottom),
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, 0, 0),
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, 0, bs),
							Vector3(0, -1, 0), get_face_color(tex_bottom), tex_bottom.is_valid());
				}
				// +Z face (side)
				if (is_neighbor_transparent(x, y, z + 1)) {
					add_face(*get_face_surface(tex_side),
							origin + Vector3(bs, 0, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, 0, bs),
							Vector3(0, 0, 1), get_face_color(tex_side), tex_side.is_valid());
				}
				// -Z face (side)
				if (is_neighbor_transparent(x, y, z - 1)) {
					add_face(*get_face_surface(tex_side),
							origin + Vector3(0, 0, 0),
							origin + Vector3(0, bs, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, 0, 0),
							Vector3(0, 0, -1), get_face_color(tex_side), tex_side.is_valid());
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

	// Textured surfaces.
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
		result.push_back(s);
	}

	return result;
}
