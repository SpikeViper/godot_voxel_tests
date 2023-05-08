#include "voxel_blocky_type_viewer.h"
#include "../../../util/godot/classes/grid_container.h"
#include "../../../util/godot/classes/label.h"
#include "../../../util/godot/classes/mesh_instance_3d.h"
#include "../../../util/godot/classes/option_button.h"
#include "../../../util/godot/core/callable.h"
#include "../../../util/godot/editor_scale.h"
#include "../../constants/voxel_string_names.h"
#include "model_viewer.h"

namespace zylann::voxel {

VoxelBlockyTypeViewer::VoxelBlockyTypeViewer() {
	const float editor_scale = EDSCALE;

	ZN_ModelViewer *model_viewer = memnew(ZN_ModelViewer);
	model_viewer->set_h_size_flags(Container::SIZE_EXPAND_FILL);
	model_viewer->set_v_size_flags(Container::SIZE_EXPAND_FILL);
	model_viewer->set_custom_minimum_size(Vector2(100, 150 * editor_scale));
	model_viewer->set_camera_distance(1.9f);
	add_child(model_viewer);
	Node *viewer_root = model_viewer->get_viewer_root_node();

	_attributes_container = memnew(GridContainer);
	_attributes_container->set_columns(2);
	add_child(_attributes_container);

	_mesh_instance = memnew(MeshInstance3D);
	viewer_root->add_child(_mesh_instance);
}

void VoxelBlockyTypeViewer::set_type(Ref<VoxelBlockyType> type) {
	if (_type.is_valid()) {
		_type->disconnect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_type_changed));
	}

	_type = type;

	if (_type.is_valid()) {
		_type->connect(VoxelStringNames::get_singleton().changed,
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_type_changed));
	}

	update_model();
	update_attribute_editors();
}

bool VoxelBlockyTypeViewer::get_attribute_editor_index(const StringName &attrib_name, unsigned int &out_index) const {
	unsigned int i = 0;
	for (const AttributeEditor &ed : _attribute_editors) {
		if (ed.name == attrib_name) {
			out_index = i;
			return true;
		}
		++i;
	}
	return false;
}

bool VoxelBlockyTypeViewer::get_preview_attribute_value(const StringName &attrib_name, uint8_t &out_value) const {
	unsigned int i;
	if (get_attribute_editor_index(attrib_name, i)) {
		out_value = _attribute_editors[i].value;
		return true;
	}
	return false;
}

void VoxelBlockyTypeViewer::update_model() {
	ZN_ASSERT_RETURN(_type.is_valid());

	std::vector<Ref<VoxelBlockyAttribute>> attributes;
	_type->get_checked_attributes(attributes);

	update_model(to_span(attributes));
}

void VoxelBlockyTypeViewer::update_model(Span<const Ref<VoxelBlockyAttribute>> attributes) {
	VoxelBlockyType::VariantKey key;
	for (unsigned int i = 0; i < attributes.size(); ++i) {
		const Ref<VoxelBlockyAttribute> attrib = attributes[i];
		ZN_ASSERT_RETURN(attrib.is_valid());
		const StringName attrib_name = attrib->get_attribute_name();
		key.attribute_names[i] = attrib_name;
		uint8_t value;
		if (!get_preview_attribute_value(attrib_name, value)) {
			value = attrib->get_default_value();
		}
		key.attribute_values[i] = value;
	}

	// Can be null
	Ref<Mesh> mesh = _type->get_preview_mesh(key);
	_mesh_instance->set_mesh(mesh);
}

void VoxelBlockyTypeViewer::remove_attribute_editor(unsigned int index) {
	ZN_ASSERT_RETURN(index < _attribute_editors.size());
	AttributeEditor &ed = _attribute_editors[index];

	ZN_ASSERT_RETURN(ed.label != nullptr);
	ed.label->queue_free();

	ZN_ASSERT_RETURN(ed.selector != nullptr);
	ed.selector->queue_free();

	_attribute_editors.erase(_attribute_editors.begin() + index);
}

bool contains_attribute_with_name(const std::vector<Ref<VoxelBlockyAttribute>> &attribs, const StringName &name) {
	for (const Ref<VoxelBlockyAttribute> &attrib : attribs) {
		if (attrib->get_attribute_name() == name) {
			return true;
		}
	}
	return false;
}

void VoxelBlockyTypeViewer::update_attribute_editors() {
	std::vector<Ref<VoxelBlockyAttribute>> attributes;
	_type->get_checked_attributes(attributes);

	// Add new attributes
	for (const Ref<VoxelBlockyAttribute> &attrib : attributes) {
		ZN_ASSERT_RETURN(attrib.is_valid());

		// Check if already present
		unsigned int editor_index = 0;
		if (get_attribute_editor_index(attrib->get_attribute_name(), editor_index)) {
			AttributeEditor &existing_editor = _attribute_editors[editor_index];
			ZN_ASSERT_RETURN(existing_editor.attribute_copy.is_valid());
			if (existing_editor.attribute_copy->is_equivalent(**attrib)) {
				// Already present, skip
				continue;
			} else {
				// Present, but is different now, remove.
				remove_attribute_editor(editor_index);
			}
		}

		// Add new

		editor_index = _attribute_editors.size();

		AttributeEditor ed;
		ed.name = attrib->get_attribute_name();
		ed.value = attrib->get_default_value();

		ed.label = memnew(Label);
		ed.label->set_text(attrib->get_attribute_name());

		ed.selector = memnew(OptionButton);
		int index_to_select = -1;
		for (unsigned int v : attrib->get_used_values()) {
			String value_name = attrib->get_name_from_value(v);
			if (value_name == "") {
				value_name = String::num_int64(v);
			}
			const int selector_index = ed.selector->get_item_count();
			ed.selector->add_item(value_name, v);
			if (v == ed.value) {
				index_to_select = selector_index;
			}
		}

		// Note, the default value can be invalid...
		if (index_to_select == -1) {
			ed.value = attrib->get_used_values()[0];
			index_to_select = 0;
		}

		// Select before connecting the signal, we don't need the notification at this stage
		ed.selector->select(index_to_select);

#ifdef ZN_GODOT
		ed.selector->connect("item_selected",
				ZN_GODOT_CALLABLE_MP(this, VoxelBlockyTypeViewer, _on_attribute_editor_value_selected)
						.bind(editor_index));
#else
		// TODO GDX: Callable::bind() isn't implemented, can't use this signal
		// See https://github.com/godotengine/godot-cpp/issues/802
#endif

		// Make a copy so we can detect changes later. It should be cheap as attributes are small resources.
		ed.attribute_copy = attrib->duplicate();

		_attributes_container->add_child(ed.label);
		_attributes_container->add_child(ed.selector);

		_attribute_editors.push_back(ed);
	}

	// Remove editors from attributes no longer in the type
	for (unsigned int editor_index = 0; editor_index < _attribute_editors.size();) {
		const AttributeEditor &ed = _attribute_editors[editor_index];
		if (!contains_attribute_with_name(attributes, ed.name)) {
			remove_attribute_editor(editor_index);
		} else {
			++editor_index;
		}
	}
}

void VoxelBlockyTypeViewer::_on_type_changed() {
	update_attribute_editors();
	update_model();
}

void VoxelBlockyTypeViewer::_on_attribute_editor_value_selected(int value_index, int editor_index) {
	AttributeEditor &ed = _attribute_editors[editor_index];
	ed.value = ed.selector->get_item_id(value_index);
	update_model();
}

void VoxelBlockyTypeViewer::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_type_changed"), &VoxelBlockyModelViewer::_on_type_changed);
	ClassDB::bind_method(D_METHOD("_on_attribute_editor_value_selected"),
			&VoxelBlockyModelViewer::_on_attribute_editor_value_selected);
#endif
}

} // namespace zylann::voxel