#include "voxel_mesher.h"

void VoxelMesher::add_face(
		Vector<Vector3> &r_vertices,
		Vector<Vector3> &r_normals,
		Vector<Color> &r_colors,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color) {
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
}

Array VoxelMesher::build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size) {
	const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	const uint8_t *blocks = p_blocks.ptr();

	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Color> colors;

	vertices.resize(0);
	normals.resize(0);
	colors.resize(0);

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

				if (VoxelBlockData::is_transparent(get_block(blocks, x + 1, y, z))) {
					add_face(vertices, normals, colors,
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, 0, bs),
							Vector3(1, 0, 0), col);
				}
				if (VoxelBlockData::is_transparent(get_block(blocks, x - 1, y, z))) {
					add_face(vertices, normals, colors,
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, 0, 0),
							Vector3(-1, 0, 0), col);
				}
				if (VoxelBlockData::is_transparent(get_block(blocks, x, y + 1, z))) {
					add_face(vertices, normals, colors,
							origin + Vector3(0, bs, 0),
							origin + Vector3(0, bs, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(bs, bs, 0),
							Vector3(0, 1, 0), col);
				}
				if (VoxelBlockData::is_transparent(get_block(blocks, x, y - 1, z))) {
					add_face(vertices, normals, colors,
							origin + Vector3(0, 0, bs),
							origin + Vector3(0, 0, 0),
							origin + Vector3(bs, 0, 0),
							origin + Vector3(bs, 0, bs),
							Vector3(0, -1, 0), col);
				}
				if (VoxelBlockData::is_transparent(get_block(blocks, x, y, z + 1))) {
					add_face(vertices, normals, colors,
							origin + Vector3(bs, 0, bs),
							origin + Vector3(bs, bs, bs),
							origin + Vector3(0, bs, bs),
							origin + Vector3(0, 0, bs),
							Vector3(0, 0, 1), col);
				}
				if (VoxelBlockData::is_transparent(get_block(blocks, x, y, z - 1))) {
					add_face(vertices, normals, colors,
							origin + Vector3(0, 0, 0),
							origin + Vector3(0, bs, 0),
							origin + Vector3(bs, bs, 0),
							origin + Vector3(bs, 0, 0),
							Vector3(0, 0, -1), col);
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

	return arrays;
}
