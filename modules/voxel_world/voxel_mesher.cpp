#include "voxel_mesher.h"

// Helper to pack a float [0,1] to uint8_t [0,255].
static _FORCE_INLINE_ uint8_t float_to_u8(float v) {
	return (uint8_t)CLAMP((int)(v * 255.0f + 0.5f), 0, 255);
}

static void push_custom0_vertex(Vector<uint8_t> &r_custom0, const VoxelMesher::VertexLight &vl) {
	r_custom0.push_back(float_to_u8(vl.sun));
	r_custom0.push_back(float_to_u8(vl.block));
	r_custom0.push_back(float_to_u8(vl.ao));
	r_custom0.push_back(255); // Reserved alpha.
}

VoxelMesher::VertexLight VoxelMesher::compute_vertex_light(const uint16_t *p_blocks, const uint8_t *p_light,
		int bx, int by, int bz,
		const Vector3 &p_normal, int p_corner_u, int p_corner_v,
		const NeighborBlocks &p_nb, const NeighborLight &p_nl,
		const VoxelBlockRegistry *p_reg) {
	VertexLight result;

	if (!p_light) {
		result.sun = 1.0f;
		result.block = 0.0f;
		result.ao = 1.0f;
		return result;
	}

	// Determine the axis-aligned sampling offsets based on face normal and corner index.
	// For a face with a given normal, the vertex at corner (u, v) samples
	// the face-adjacent block and 3 neighboring blocks for smooth lighting.
	int nx = (int)p_normal.x;
	int ny = (int)p_normal.y;
	int nz = (int)p_normal.z;

	// The face-adjacent block (in normal direction).
	int fx = bx + nx;
	int fy = by + ny;
	int fz = bz + nz;

	// Determine the two tangent axes (u, v) orthogonal to the normal.
	int du_x = 0, du_y = 0, du_z = 0;
	int dv_x = 0, dv_y = 0, dv_z = 0;

	if (ny != 0) {
		// Horizontal face (top/bottom): u = X, v = Z.
		du_x = p_corner_u;
		dv_z = p_corner_v;
	} else if (nx != 0) {
		// X-facing face: u = Z, v = Y.
		du_z = p_corner_u;
		dv_y = p_corner_v;
	} else {
		// Z-facing face: u = X, v = Y.
		du_x = p_corner_u;
		dv_y = p_corner_v;
	}

	// Sample the 4 blocks contributing to this vertex: face, face+u, face+v, face+u+v.
	int samples[4][3] = {
		{ fx, fy, fz },
		{ fx + du_x, fy + du_y, fz + du_z },
		{ fx + dv_x, fy + dv_y, fz + dv_z },
		{ fx + du_x + dv_x, fy + du_y + dv_y, fz + du_z + dv_z }
	};

	float sun_sum = 0.0f;
	float bl_sum = 0.0f;
	int count = 0;

	for (int i = 0; i < 4; i++) {
		int stype = get_block(p_blocks, samples[i][0], samples[i][1], samples[i][2], p_nb);
		if (!p_reg->is_solid(stype)) {
			sun_sum += (float)get_light_sun(p_light, samples[i][0], samples[i][1], samples[i][2], p_nl);
			bl_sum += (float)get_light_block(p_light, samples[i][0], samples[i][1], samples[i][2], p_nl);
			count++;
		}
	}

	if (count > 0) {
		result.sun = (sun_sum / (float)count) / 15.0f;
		result.block = (bl_sum / (float)count) / 15.0f;
	} else {
		result.sun = 0.0f;
		result.block = 0.0f;
	}

	// AO: check solidity of the 3 relevant neighbors (side1, side2, corner).
	bool side1 = p_reg->is_solid(get_block(p_blocks, fx + du_x, fy + du_y, fz + du_z, p_nb));
	bool side2 = p_reg->is_solid(get_block(p_blocks, fx + dv_x, fy + dv_y, fz + dv_z, p_nb));
	bool corner = p_reg->is_solid(get_block(p_blocks, fx + du_x + dv_x, fy + du_y + dv_y, fz + du_z + dv_z, p_nb));
	result.ao = compute_ao(side1, side2, corner);

	return result;
}

void VoxelMesher::add_face(
		SurfaceData &r_surface,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color,
		bool p_has_uv,
		bool p_flip_v,
		const VertexLight &p_vl0, const VertexLight &p_vl1, const VertexLight &p_vl2, const VertexLight &p_vl3) {
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

	// Custom0 light data (matching CCW triangle vertex order).
	push_custom0_vertex(r_surface.custom0, p_vl0);
	push_custom0_vertex(r_surface.custom0, p_vl2);
	push_custom0_vertex(r_surface.custom0, p_vl1);
	push_custom0_vertex(r_surface.custom0, p_vl0);
	push_custom0_vertex(r_surface.custom0, p_vl3);
	push_custom0_vertex(r_surface.custom0, p_vl2);

	if (p_has_uv) {
		r_surface.has_uv = true;
		if (p_flip_v) {
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
		for (int i = 0; i < 6; i++) {
			r_surface.uvs.push_back(Vector2(0, 0));
		}
	}
}

// Overload without light data — uses full bright defaults.
void VoxelMesher::add_face(
		SurfaceData &r_surface,
		const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
		const Vector3 &p_normal,
		const Color &p_color,
		bool p_has_uv,
		bool p_flip_v) {
	VertexLight full_bright = { 1.0f, 0.0f, 1.0f };
	add_face(r_surface, p_v0, p_v1, p_v2, p_v3, p_normal, p_color, p_has_uv, p_flip_v,
			full_bright, full_bright, full_bright, full_bright);
}

// Helper struct for face corner offsets.
// Each face has 4 vertices. For each vertex, we need two tangent-direction offsets
// (u, v) relative to the face-adjacent cell, to compute smooth lighting.
struct FaceCorners {
	int u[4];
	int v[4];
};

// Corner offsets for each face direction.
// v0..v3 order matches the winding in build_chunk_mesh.
// Values are -1 or +1 offsets along the two tangent axes.

// +X face: v0=(0,-1), v1=(0,+1), v2=(+1,+1), v3=(+1,-1) (u=Z, v=Y)
static const FaceCorners corners_px = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };
// -X face: v0=(+1,-1), v1=(+1,+1), v2=(0,+1), v3=(0,-1) (u=Z, v=Y)
static const FaceCorners corners_nx = { { 0, 0, -1, -1 }, { -1, 0, 0, -1 } };
// +Y face: v0=(-1,-1), v1=(-1,+1), v2=(+1,+1), v3=(+1,-1) (u=X, v=Z)
static const FaceCorners corners_py = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };
// -Y face: v0=(-1,+1), v1=(-1,-1), v2=(+1,-1), v3=(+1,+1) (u=X, v=Z)
static const FaceCorners corners_ny = { { -1, -1, 0, 0 }, { 0, -1, -1, 0 } };
// +Z face: v0=(+1,-1), v1=(+1,+1), v2=(-1,+1), v3=(-1,-1) (u=X, v=Y)
static const FaceCorners corners_pz = { { 0, 0, -1, -1 }, { -1, 0, 0, -1 } };
// -Z face: v0=(-1,-1), v1=(-1,+1), v2=(+1,+1), v3=(+1,-1) (u=X, v=Y)
static const FaceCorners corners_nz = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };

Vector<VoxelMesher::MeshSurface> VoxelMesher::build_chunk_mesh(const Vector<uint16_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry, const NeighborBlocks &p_neighbors, const uint8_t *p_light_data, const NeighborLight &p_neighbor_light) {
	const uint16_t *blocks = p_blocks.ptr();

	SurfaceData untextured_surface;
	HashMap<Ref<Texture2D>, SurfaceData> textured_surfaces;
	HashMap<Ref<Texture2D>, int> textured_block_types;

	struct ShaderSurfaceInfo {
		SurfaceData surface;
		Ref<Texture2D> texture;
		Ref<ShaderMaterial> shader_material;
	};
	HashMap<int, ShaderSurfaceInfo> shader_surfaces;

	bool use_registry = p_registry.is_valid();
	const VoxelBlockRegistry *reg = use_registry ? p_registry.ptr() : nullptr;

	for (int y = 0; y < CY; y++) {
		for (int z = 0; z < CZ; z++) {
			for (int x = 0; x < CX; x++) {
				int type = (int)blocks[VoxelTerrainGenerator::block_index(x, y, z)];

				if (type == VOXEL_BLOCK_AIR) {
					continue;
				}

				bool solid = use_registry ? reg->is_solid(type) : true;
				if (!solid) {
					if (!use_registry || !reg->block_has_shader(type)) {
						continue;
					}
				}

				Color col = use_registry ? reg->get_color(type) : Color(1, 1, 1);
				float bs = p_block_size;
				Vector3 origin(x * bs, y * bs, z * bs);

				auto should_show_face = [&](int nx, int ny, int nz) -> bool {
					int neighbor = get_block(blocks, nx, ny, nz, p_neighbors);
					if (!solid) {
						return neighbor == VOXEL_BLOCK_AIR;
					}
					if (use_registry && reg->is_transparent(neighbor)) {
						return true;
					}
					if (use_registry && reg->is_uses_alpha(neighbor)) {
						return true;
					}
					return false;
				};

				Ref<ShaderMaterial> block_shader;
				if (use_registry) {
					block_shader = p_registry->get_block_shader_material(type);
				}

				Ref<Texture2D> tex_top, tex_bottom, tex_side;
				if (use_registry) {
					tex_top = p_registry->get_block_texture_for_face(type, Vector3(0, 1, 0));
					tex_bottom = p_registry->get_block_texture_for_face(type, Vector3(0, -1, 0));
					tex_side = p_registry->get_block_texture_for_face(type, Vector3(1, 0, 0));
				}

				float mesh_h = use_registry ? p_registry->get_block_mesh_height(type) : 1.0f;
				const float top_y = bs * mesh_h;

				// Shared array for vertex lights — filled per face.
				VertexLight vl[4];
				auto fill_face_vl = [&](const Vector3 &normal, const FaceCorners &corners) {
					for (int i = 0; i < 4; i++) {
						vl[i] = compute_vertex_light(blocks, p_light_data, x, y, z, normal, corners.u[i], corners.v[i], p_neighbors, p_neighbor_light, reg);
					}
				};

				if (block_shader.is_valid()) {
					if (!shader_surfaces.has(type)) {
						ShaderSurfaceInfo info;
						info.shader_material = block_shader;
						info.texture = tex_top.is_valid() ? tex_top : (tex_side.is_valid() ? tex_side : tex_bottom);
						shader_surfaces[type] = info;
					}
					SurfaceData *surf = &shader_surfaces[type].surface;
					bool has_tex = tex_top.is_valid() || tex_side.is_valid() || tex_bottom.is_valid();
					Color face_col = has_tex ? Color(1, 1, 1) : col;

					if (should_show_face(x + 1, y, z)) {
						fill_face_vl(Vector3(1, 0, 0), corners_px);
						add_face(*surf, origin + Vector3(bs, 0, 0), origin + Vector3(bs, top_y, 0), origin + Vector3(bs, top_y, bs), origin + Vector3(bs, 0, bs), Vector3(1, 0, 0), face_col, has_tex, true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x - 1, y, z)) {
						fill_face_vl(Vector3(-1, 0, 0), corners_nx);
						add_face(*surf, origin + Vector3(0, 0, bs), origin + Vector3(0, top_y, bs), origin + Vector3(0, top_y, 0), origin + Vector3(0, 0, 0), Vector3(-1, 0, 0), face_col, has_tex, true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y + 1, z)) {
						fill_face_vl(Vector3(0, 1, 0), corners_py);
						add_face(*surf, origin + Vector3(0, top_y, 0), origin + Vector3(0, top_y, bs), origin + Vector3(bs, top_y, bs), origin + Vector3(bs, top_y, 0), Vector3(0, 1, 0), face_col, has_tex, false, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y - 1, z)) {
						fill_face_vl(Vector3(0, -1, 0), corners_ny);
						add_face(*surf, origin + Vector3(0, 0, bs), origin + Vector3(0, 0, 0), origin + Vector3(bs, 0, 0), origin + Vector3(bs, 0, bs), Vector3(0, -1, 0), face_col, has_tex, false, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y, z + 1)) {
						fill_face_vl(Vector3(0, 0, 1), corners_pz);
						add_face(*surf, origin + Vector3(bs, 0, bs), origin + Vector3(bs, top_y, bs), origin + Vector3(0, top_y, bs), origin + Vector3(0, 0, bs), Vector3(0, 0, 1), face_col, has_tex, true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y, z - 1)) {
						fill_face_vl(Vector3(0, 0, -1), corners_nz);
						add_face(*surf, origin + Vector3(0, 0, 0), origin + Vector3(0, top_y, 0), origin + Vector3(bs, top_y, 0), origin + Vector3(bs, 0, 0), Vector3(0, 0, -1), face_col, has_tex, true, vl[0], vl[1], vl[2], vl[3]);
					}
				} else {
					auto get_face_surface = [&](const Ref<Texture2D> &p_tex) -> SurfaceData * {
						if (p_tex.is_valid()) {
							if (!textured_block_types.has(p_tex)) {
								textured_block_types[p_tex] = type;
							}
							return &textured_surfaces[p_tex];
						}
						return &untextured_surface;
					};

					auto get_face_color = [&](const Ref<Texture2D> &p_tex) -> Color {
						return p_tex.is_valid() ? Color(1, 1, 1) : col;
					};

					if (should_show_face(x + 1, y, z)) {
						fill_face_vl(Vector3(1, 0, 0), corners_px);
						add_face(*get_face_surface(tex_side), origin + Vector3(bs, 0, 0), origin + Vector3(bs, top_y, 0), origin + Vector3(bs, top_y, bs), origin + Vector3(bs, 0, bs), Vector3(1, 0, 0), get_face_color(tex_side), tex_side.is_valid(), true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x - 1, y, z)) {
						fill_face_vl(Vector3(-1, 0, 0), corners_nx);
						add_face(*get_face_surface(tex_side), origin + Vector3(0, 0, bs), origin + Vector3(0, top_y, bs), origin + Vector3(0, top_y, 0), origin + Vector3(0, 0, 0), Vector3(-1, 0, 0), get_face_color(tex_side), tex_side.is_valid(), true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y + 1, z)) {
						fill_face_vl(Vector3(0, 1, 0), corners_py);
						add_face(*get_face_surface(tex_top), origin + Vector3(0, top_y, 0), origin + Vector3(0, top_y, bs), origin + Vector3(bs, top_y, bs), origin + Vector3(bs, top_y, 0), Vector3(0, 1, 0), get_face_color(tex_top), tex_top.is_valid(), false, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y - 1, z)) {
						fill_face_vl(Vector3(0, -1, 0), corners_ny);
						add_face(*get_face_surface(tex_bottom), origin + Vector3(0, 0, bs), origin + Vector3(0, 0, 0), origin + Vector3(bs, 0, 0), origin + Vector3(bs, 0, bs), Vector3(0, -1, 0), get_face_color(tex_bottom), tex_bottom.is_valid(), false, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y, z + 1)) {
						fill_face_vl(Vector3(0, 0, 1), corners_pz);
						add_face(*get_face_surface(tex_side), origin + Vector3(bs, 0, bs), origin + Vector3(bs, top_y, bs), origin + Vector3(0, top_y, bs), origin + Vector3(0, 0, bs), Vector3(0, 0, 1), get_face_color(tex_side), tex_side.is_valid(), true, vl[0], vl[1], vl[2], vl[3]);
					}
					if (should_show_face(x, y, z - 1)) {
						fill_face_vl(Vector3(0, 0, -1), corners_nz);
						add_face(*get_face_surface(tex_side), origin + Vector3(0, 0, 0), origin + Vector3(0, top_y, 0), origin + Vector3(bs, top_y, 0), origin + Vector3(bs, 0, 0), Vector3(0, 0, -1), get_face_color(tex_side), tex_side.is_valid(), true, vl[0], vl[1], vl[2], vl[3]);
					}
				}
			}
		}
	}

	// Compute the custom format flags for CUSTOM0 (RGBA8_UNORM).
	// ARRAY_FORMAT_CUSTOM0 signals that custom0 data is present.
	// The format type is encoded in bits at ARRAY_FORMAT_CUSTOM0_SHIFT.
	uint64_t custom0_flags = (uint64_t)Mesh::ARRAY_FORMAT_CUSTOM0 | ((uint64_t)Mesh::ARRAY_CUSTOM_RGBA8_UNORM << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT);

	// Helper lambda to build the surface arrays including CUSTOM0.
	auto build_surface_arrays = [&](const SurfaceData &sd) -> Array {
		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = sd.vertices;
		arrays[Mesh::ARRAY_NORMAL] = sd.normals;
		arrays[Mesh::ARRAY_COLOR] = sd.colors;
		if (sd.has_uv) {
			arrays[Mesh::ARRAY_TEX_UV] = sd.uvs;
		}
		if (sd.custom0.size() > 0) {
			PackedByteArray pba;
			pba.resize(sd.custom0.size());
			memcpy(pba.ptrw(), sd.custom0.ptr(), sd.custom0.size());
			arrays[Mesh::ARRAY_CUSTOM0] = pba;
		}
		return arrays;
	};

	Vector<MeshSurface> result;

	// Untextured surface.
	if (untextured_surface.vertices.size() > 0) {
		MeshSurface s;
		s.arrays = build_surface_arrays(untextured_surface);
		s.custom_format_flags = (untextured_surface.custom0.size() > 0) ? custom0_flags : 0;
		result.push_back(s);
	}

	// Textured surfaces.
	for (const KeyValue<Ref<Texture2D>, SurfaceData> &E : textured_surfaces) {
		if (E.value.vertices.size() == 0) {
			continue;
		}
		MeshSurface s;
		s.arrays = build_surface_arrays(E.value);
		s.texture = E.key;
		s.custom_format_flags = (E.value.custom0.size() > 0) ? custom0_flags : 0;
		if (textured_block_types.has(E.key)) {
			s.block_type = textured_block_types[E.key];
		}
		result.push_back(s);
	}

	// Shader surfaces.
	for (const KeyValue<int, ShaderSurfaceInfo> &E : shader_surfaces) {
		if (E.value.surface.vertices.size() == 0) {
			continue;
		}
		MeshSurface s;
		s.arrays = build_surface_arrays(E.value.surface);
		s.texture = E.value.texture;
		s.shader_material = E.value.shader_material;
		s.block_type = E.key;
		s.custom_format_flags = (E.value.surface.custom0.size() > 0) ? custom0_flags : 0;
		result.push_back(s);
	}

	return result;
}

// =============================================================================
// build_volume_mesh — finite-volume meshing for VoxelScene.
// =============================================================================
// Independent of chunk dimensions. Out-of-bounds neighbors are treated as air.
// AO is baked directly into vertex colors (no CUSTOM0 lighting data).

namespace {

struct VolumeSurface {
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Color> colors;
	Vector<Vector2> uvs;
	bool has_uv = false;
};

_FORCE_INLINE_ int volume_index(int x, int y, int z, const Vector3i &size) {
	return (y * size.z + z) * size.x + x;
}

_FORCE_INLINE_ int get_block_volume(const uint16_t *blocks, int x, int y, int z, const Vector3i &size) {
	if (x < 0 || y < 0 || z < 0 || x >= size.x || y >= size.y || z >= size.z) {
		return VOXEL_BLOCK_AIR;
	}
	return (int)blocks[volume_index(x, y, z, size)];
}

_FORCE_INLINE_ float ao_factor(bool side1, bool side2, bool corner) {
	int occluders = (int)side1 + (int)side2 + (int)(corner && !(side1 && side2));
	if (side1 && side2) {
		occluders = 3;
	}
	return 1.0f - occluders * 0.22f; // [0.34, 1.0]
}

// Compute AO at a face vertex. Samples 3 neighbors of the face-adjacent cell:
// face+u, face+v, face+u+v (where (u, v) are tangent offsets ±1).
float compute_face_vertex_ao(const uint16_t *blocks, const Vector3i &size,
		int bx, int by, int bz, const Vector3 &normal, int corner_u, int corner_v,
		const VoxelBlockRegistry *reg) {
	int nx = (int)normal.x;
	int ny = (int)normal.y;
	int nz = (int)normal.z;
	int fx = bx + nx, fy = by + ny, fz = bz + nz;
	int du_x = 0, du_y = 0, du_z = 0;
	int dv_x = 0, dv_y = 0, dv_z = 0;
	if (ny != 0) {
		du_x = corner_u;
		dv_z = corner_v;
	} else if (nx != 0) {
		du_z = corner_u;
		dv_y = corner_v;
	} else {
		du_x = corner_u;
		dv_y = corner_v;
	}
	auto solid = [&](int x, int y, int z) -> bool {
		int t = get_block_volume(blocks, x, y, z, size);
		return reg ? reg->is_solid(t) : (t != VOXEL_BLOCK_AIR);
	};
	bool s1 = solid(fx + du_x, fy + du_y, fz + du_z);
	bool s2 = solid(fx + dv_x, fy + dv_y, fz + dv_z);
	bool sc = solid(fx + du_x + dv_x, fy + du_y + dv_y, fz + du_z + dv_z);
	return ao_factor(s1, s2, sc);
}

void volume_add_face(VolumeSurface &surf,
		const Vector3 &v0, const Vector3 &v1, const Vector3 &v2, const Vector3 &v3,
		const Vector3 &normal, const Color &base_color, bool has_uv, bool flip_v,
		float ao0, float ao1, float ao2, float ao3) {
	// CCW winding: tri1 = v0,v2,v1 ; tri2 = v0,v3,v2.
	surf.vertices.push_back(v0);
	surf.vertices.push_back(v2);
	surf.vertices.push_back(v1);
	surf.vertices.push_back(v0);
	surf.vertices.push_back(v3);
	surf.vertices.push_back(v2);

	for (int i = 0; i < 6; i++) {
		surf.normals.push_back(normal);
	}

	auto shade = [&](float ao) {
		return Color(base_color.r * ao, base_color.g * ao, base_color.b * ao, base_color.a);
	};
	surf.colors.push_back(shade(ao0));
	surf.colors.push_back(shade(ao2));
	surf.colors.push_back(shade(ao1));
	surf.colors.push_back(shade(ao0));
	surf.colors.push_back(shade(ao3));
	surf.colors.push_back(shade(ao2));

	if (has_uv) {
		surf.has_uv = true;
		if (flip_v) {
			surf.uvs.push_back(Vector2(0, 1));
			surf.uvs.push_back(Vector2(1, 0));
			surf.uvs.push_back(Vector2(0, 0));
			surf.uvs.push_back(Vector2(0, 1));
			surf.uvs.push_back(Vector2(1, 1));
			surf.uvs.push_back(Vector2(1, 0));
		} else {
			surf.uvs.push_back(Vector2(0, 0));
			surf.uvs.push_back(Vector2(1, 1));
			surf.uvs.push_back(Vector2(0, 1));
			surf.uvs.push_back(Vector2(0, 0));
			surf.uvs.push_back(Vector2(1, 0));
			surf.uvs.push_back(Vector2(1, 1));
		}
	} else {
		for (int i = 0; i < 6; i++) {
			surf.uvs.push_back(Vector2(0, 0));
		}
	}
}

Array make_surface_arrays(const VolumeSurface &sd) {
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = sd.vertices;
	arrays[Mesh::ARRAY_NORMAL] = sd.normals;
	arrays[Mesh::ARRAY_COLOR] = sd.colors;
	if (sd.has_uv) {
		arrays[Mesh::ARRAY_TEX_UV] = sd.uvs;
	}
	return arrays;
}

// Same per-face corner offset table used by build_chunk_mesh, duplicated here
// to keep the volume mesher self-contained.
struct VolumeFaceCorners {
	int u[4];
	int v[4];
};
static const VolumeFaceCorners vc_px = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };
static const VolumeFaceCorners vc_nx = { { 0, 0, -1, -1 }, { -1, 0, 0, -1 } };
static const VolumeFaceCorners vc_py = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };
static const VolumeFaceCorners vc_ny = { { -1, -1, 0, 0 }, { 0, -1, -1, 0 } };
static const VolumeFaceCorners vc_pz = { { 0, 0, -1, -1 }, { -1, 0, 0, -1 } };
static const VolumeFaceCorners vc_nz = { { -1, -1, 0, 0 }, { -1, 0, 0, -1 } };

} // namespace

Vector<VoxelMesher::MeshSurface> VoxelMesher::build_volume_mesh(const Vector<uint16_t> &p_blocks, const Vector3i &p_size, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry, const VolumeMeshOptions &p_options) {
	Vector<MeshSurface> result;
	if (p_size.x <= 0 || p_size.y <= 0 || p_size.z <= 0) {
		return result;
	}
	const int expected = p_size.x * p_size.y * p_size.z;
	if (p_blocks.size() != expected) {
		return result;
	}

	const uint16_t *blocks = p_blocks.ptr();
	const bool use_registry = p_registry.is_valid();
	const VoxelBlockRegistry *reg = use_registry ? p_registry.ptr() : nullptr;
	const float bs = p_block_size;
	const bool bake_ao = p_options.bake_ao;

	VolumeSurface untextured_surface;
	HashMap<Ref<Texture2D>, VolumeSurface> textured_surfaces;
	HashMap<Ref<Texture2D>, int> textured_block_types;

	auto neighbor_hides_face = [&](int type, int nx, int ny, int nz) -> bool {
		int n = get_block_volume(blocks, nx, ny, nz, p_size);
		if (n == VOXEL_BLOCK_AIR) {
			return false;
		}
		bool n_solid = reg ? reg->is_solid(n) : true;
		if (!n_solid) {
			return false;
		}
		if (reg && reg->is_transparent(n)) {
			return false;
		}
		if (reg && reg->is_uses_alpha(n)) {
			return false;
		}
		// If the current block is non-solid (e.g. transparent/cross), still hide if neighbor is opaque.
		(void)type;
		return true;
	};

	float vao[4];
	auto fill_ao = [&](int bx, int by, int bz, const Vector3 &n, const VolumeFaceCorners &c) {
		if (!bake_ao) {
			vao[0] = vao[1] = vao[2] = vao[3] = 1.0f;
			return;
		}
		for (int i = 0; i < 4; i++) {
			vao[i] = compute_face_vertex_ao(blocks, p_size, bx, by, bz, n, c.u[i], c.v[i], reg);
		}
	};

	for (int y = 0; y < p_size.y; y++) {
		for (int z = 0; z < p_size.z; z++) {
			for (int x = 0; x < p_size.x; x++) {
				int type = (int)blocks[volume_index(x, y, z, p_size)];
				if (type == VOXEL_BLOCK_AIR) {
					continue;
				}
				bool solid = use_registry ? reg->is_solid(type) : true;
				// Skip non-solid, non-emissive, non-shadered blocks.
				if (!solid) {
					continue;
				}

				Color col = use_registry ? reg->get_color(type) : Color(1, 1, 1);
				Vector3 origin(x * bs, y * bs, z * bs);
				float mesh_h = use_registry ? reg->get_block_mesh_height(type) : 1.0f;
				const float top_y = bs * mesh_h;

				Ref<Texture2D> tex_top, tex_bottom, tex_side;
				if (use_registry) {
					tex_top = reg->get_block_texture_for_face(type, Vector3(0, 1, 0));
					tex_bottom = reg->get_block_texture_for_face(type, Vector3(0, -1, 0));
					tex_side = reg->get_block_texture_for_face(type, Vector3(1, 0, 0));
				}

				auto get_face_surface = [&](const Ref<Texture2D> &p_tex) -> VolumeSurface * {
					if (p_tex.is_valid()) {
						if (!textured_block_types.has(p_tex)) {
							textured_block_types[p_tex] = type;
						}
						return &textured_surfaces[p_tex];
					}
					return &untextured_surface;
				};
				auto face_color_for = [&](const Ref<Texture2D> &p_tex) -> Color {
					return p_tex.is_valid() ? Color(1, 1, 1) : col;
				};

				if (!neighbor_hides_face(type, x + 1, y, z)) {
					fill_ao(x, y, z, Vector3(1, 0, 0), vc_px);
					volume_add_face(*get_face_surface(tex_side),
							origin + Vector3(bs, 0, 0), origin + Vector3(bs, top_y, 0),
							origin + Vector3(bs, top_y, bs), origin + Vector3(bs, 0, bs),
							Vector3(1, 0, 0), face_color_for(tex_side), tex_side.is_valid(), true,
							vao[0], vao[1], vao[2], vao[3]);
				}
				if (!neighbor_hides_face(type, x - 1, y, z)) {
					fill_ao(x, y, z, Vector3(-1, 0, 0), vc_nx);
					volume_add_face(*get_face_surface(tex_side),
							origin + Vector3(0, 0, bs), origin + Vector3(0, top_y, bs),
							origin + Vector3(0, top_y, 0), origin + Vector3(0, 0, 0),
							Vector3(-1, 0, 0), face_color_for(tex_side), tex_side.is_valid(), true,
							vao[0], vao[1], vao[2], vao[3]);
				}
				if (!neighbor_hides_face(type, x, y + 1, z)) {
					fill_ao(x, y, z, Vector3(0, 1, 0), vc_py);
					volume_add_face(*get_face_surface(tex_top),
							origin + Vector3(0, top_y, 0), origin + Vector3(0, top_y, bs),
							origin + Vector3(bs, top_y, bs), origin + Vector3(bs, top_y, 0),
							Vector3(0, 1, 0), face_color_for(tex_top), tex_top.is_valid(), false,
							vao[0], vao[1], vao[2], vao[3]);
				}
				if (!neighbor_hides_face(type, x, y - 1, z)) {
					fill_ao(x, y, z, Vector3(0, -1, 0), vc_ny);
					volume_add_face(*get_face_surface(tex_bottom),
							origin + Vector3(0, 0, bs), origin + Vector3(0, 0, 0),
							origin + Vector3(bs, 0, 0), origin + Vector3(bs, 0, bs),
							Vector3(0, -1, 0), face_color_for(tex_bottom), tex_bottom.is_valid(), false,
							vao[0], vao[1], vao[2], vao[3]);
				}
				if (!neighbor_hides_face(type, x, y, z + 1)) {
					fill_ao(x, y, z, Vector3(0, 0, 1), vc_pz);
					volume_add_face(*get_face_surface(tex_side),
							origin + Vector3(bs, 0, bs), origin + Vector3(bs, top_y, bs),
							origin + Vector3(0, top_y, bs), origin + Vector3(0, 0, bs),
							Vector3(0, 0, 1), face_color_for(tex_side), tex_side.is_valid(), true,
							vao[0], vao[1], vao[2], vao[3]);
				}
				if (!neighbor_hides_face(type, x, y, z - 1)) {
					fill_ao(x, y, z, Vector3(0, 0, -1), vc_nz);
					volume_add_face(*get_face_surface(tex_side),
							origin + Vector3(0, 0, 0), origin + Vector3(0, top_y, 0),
							origin + Vector3(bs, top_y, 0), origin + Vector3(bs, 0, 0),
							Vector3(0, 0, -1), face_color_for(tex_side), tex_side.is_valid(), true,
							vao[0], vao[1], vao[2], vao[3]);
				}
			}
		}
	}

	if (untextured_surface.vertices.size() > 0) {
		MeshSurface s;
		s.arrays = make_surface_arrays(untextured_surface);
		result.push_back(s);
	}
	for (const KeyValue<Ref<Texture2D>, VolumeSurface> &E : textured_surfaces) {
		if (E.value.vertices.size() == 0) {
			continue;
		}
		MeshSurface s;
		s.arrays = make_surface_arrays(E.value);
		s.texture = E.key;
		if (textured_block_types.has(E.key)) {
			s.block_type = textured_block_types[E.key];
		}
		result.push_back(s);
	}
	return result;
}
