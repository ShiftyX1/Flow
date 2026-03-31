#include "voxel_mesher.h"

void VoxelMesher::add_face(
		Vector<Vector3> &r_vertices,
		Vector<Vector3> &r_normals,
		Vector<Color> &r_colors,
		Vector<Vector2> &r_uvs,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color,
		bool p_has_uv) {
	// CCW winding: tri1 = v0,v2,v1 ; tri2 = v0,v3,v2
	r_vertices.push_back(p_v0);
	r_vertices.push_back(p_v2);
	r_vertices.push_back(p_v1);
	r_vertices.push_back(p_v0);
	r_vertices.push_back(p_v3);
	r_vertices.push_back(p_v2);

	for (int i = 0; i < 6; i++) {
		r_normals.push_back(p_normal);
		r_colors.push_back(p_color);
	}

	if (p_has_uv) {
		// Standard quad UVs: (0,0) (1,1) (0,1) , (0,0) (1,0) (1,1)
		r_uvs.push_back(Vector2(0, 0));
		r_uvs.push_back(Vector2(1, 1));
		r_uvs.push_back(Vector2(0, 1));
		r_uvs.push_back(Vector2(0, 0));
		r_uvs.push_back(Vector2(1, 0));
		r_uvs.push_back(Vector2(1, 1));
	}
}

// --- Mesher: always uses VoxelBlockData for logic, registry only for textures ---

Array VoxelMesher::build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry) {
	const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	const uint8_t *blocks = p_blocks.ptr();

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Color> colors;
	Vector<Vector2> uvs;

	bool any_uv = false;

	for (int y = 0; y < CY; y++) {
		for (int z = 0; z < CZ; z++) {
			for (int x = 0; x < CX; x++) {
				VoxelBlockType type = (VoxelBlockType)blocks[VoxelTerrainGenerator::block_index(x, y, z)];

				// Always use engine-level block data for logic.
				if (!VoxelBlockData::is_solid(type)) {
					continue;
				}

				Color col = VoxelBlockData::block_colors[type];

				// Check if this block has a texture in the registry.
				bool block_textured = p_registry.is_valid() && p_registry->block_has_texture((int)type);
				if (block_textured) {
					// When textured, use white vertex color (texture provides color).
					col = Color(1, 1, 1);
					any_uv = true;
				}

				float bs = p_block_size;
				Vector3 origin(x * bs, y * bs, z * bs);

				auto is_neighbor_transparent = [&](int nx, int ny, int nz) -> bool {
					VoxelBlockType neighbor = get_block(blocks, nx, ny, nz);
					return VoxelBlockData::is_transparent(neighbor);
				};

				// +X face
				if (is_neighbor_transparent(x + 1, y, z)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, 0, bs),
							Vector3(1, 0, 0), col, block_textured);
				}
				// -X face
				if (is_neighbor_transparent(x - 1, y, z)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, 0, 0),
							Vector3(-1, 0, 0), col, block_textured);
				}
				// +Y face (top)
				if (is_neighbor_transparent(x, y + 1, z)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, bs, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, bs, 0),
							Vector3(0, 1, 0), col, block_textured);
				}
				// -Y face (bottom)
				if (is_neighbor_transparent(x, y - 1, z)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, 0, 0),
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, 0, bs),
							Vector3(0, -1, 0), col, block_textured);
				}
				// +Z face
				if (is_neighbor_transparent(x, y, z + 1)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(bs, 0, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, 0, bs),
							Vector3(0, 0, 1), col, block_textured);
				}
				// -Z face
				if (is_neighbor_transparent(x, y, z - 1)) {
					add_face(vertices, normals, colors, uvs,
							origin + Vector3(0, 0, 0),
							origin + Vector3(0, bs, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, 0, 0),
							Vector3(0, 0, -1), col, block_textured);
				}
			}
		}
	}

	if (vertices.size() == 0) {
		return Array();
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_COLOR] = colors;
	if (any_uv && uvs.size() > 0) {
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
	}

	return arrays;
}

// --- Legacy mesher (no registry, uses static VoxelBlockData) ---

Array VoxelMesher::build_chunk_mesh_legacy(const Vector<uint8_t> &p_blocks, float p_block_size) {
	Ref<VoxelBlockRegistry> null_reg;
	return build_chunk_mesh(p_blocks, p_block_size, null_reg);
}
