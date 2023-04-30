#include "voxel_blocky_model_empty.h"

namespace zylann::voxel {

VoxelBlockyModelEmpty::VoxelBlockyModelEmpty() {
	set_collision_aabbs(Span<const AABB>());
}

void VoxelBlockyModelEmpty::bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) {
	baked_data.clear();
	VoxelBlockyModel::bake(baked_data, bake_tangents, materials);
}

void VoxelBlockyModelEmpty::rotate_90(Vector3i::Axis axis, bool clockwise) {
	rotate_collision_boxes_90(axis, clockwise);
}

Ref<Mesh> VoxelBlockyModelEmpty::get_preview_mesh() const {
	return Ref<Mesh>();
}

} // namespace zylann::voxel