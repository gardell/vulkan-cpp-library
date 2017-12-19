#include <gltf/conversions.h>

namespace gltf {

	accessor_type::component_type_type component_type_from_json(integer_type component_type) {
		switch (integer_type(component_type)) {
		case accessor_type::BYTE:
		case accessor_type::UNSIGNED_BYTE:
		case accessor_type::SHORT:
		case accessor_type::UNSIGNED_SHORT:
		case accessor_type::UNSIGNED_INT:
		case accessor_type::FLOAT:
			return accessor_type::component_type_type(component_type);
		default:
			throw std::invalid_argument("componentType");
		}
	}

	accessor_type::type_type accessor_type_from_json(const nlohmann::json &json) {
		if (json == "SCALAR") {
			return accessor_type::SCALAR;
		}
		else if (json == "VEC2") {
			return accessor_type::VEC2;
		}
		else if (json == "VEC3") {
			return accessor_type::VEC3;
		}
		else if (json == "VEC4") {
			return accessor_type::VEC4;
		}
		else if (json == "MAT2") {
			return accessor_type::MAT2;
		}
		else if (json == "MAT3") {
			return accessor_type::MAT3;
		}
		else if (json == "MAT4") {
			return accessor_type::MAT4;
		}
		else {
			throw new std::invalid_argument("type");
		}
	}

	container_type<number_type> number_container_from_json(const nlohmann::json &json) {
		if (!json.is_array()) {
			throw std::invalid_argument("Not an array");
		}
		else {
			return container_type<number_type>(json.begin(), json.end());
		}
	}

	animation_sampler_type::interpolation_type animation_sampler_interpolation_from_json(
		const nlohmann::json &value) {
		if (value == "LINEAR") {
			return animation_sampler_type::LINEAR;
		}
		else if (value == "STEP") {
			return animation_sampler_type::LINEAR;
		}
		else if (value == "CATMULLROMSPLINE") {
			return animation_sampler_type::LINEAR;
		}
		else if (value == "CUBICSPLINE") {
			return animation_sampler_type::CUBICSPLINE;
		}
		else {
			throw new std::invalid_argument("interpolation");
		}
	}

	channel_type::target_type::path_type channel_target_path_from_json(const nlohmann::json &value) {
		if (value == "translation") {
			return channel_type::target_type::translation;
		}
		else if (value == "rotation") {
			return channel_type::target_type::rotation;
		}
		else if (value == "scale") {
			return channel_type::target_type::scale;
		}
		else if (value == "weights") {
			return channel_type::target_type::weights;
		}
		else {
			throw std::invalid_argument("channel target path");
		}
	}

	buffer_view_type::target_type buffer_view_target_from_json(unsigned_integer_type target) {
		switch (target) {
		case buffer_view_type::ARRAY_BUFFER:
		case buffer_view_type::ELEMENT_ARRAY_BUFFER:
			return (buffer_view_type::target_type) target;
		default:
			throw std::invalid_argument("bufferView target");
		}
	}

	mime_type image_mime_type_from_json(const nlohmann::json &mime_type) {
		if (mime_type == "image/jpeg") {
			return image_jpeg;
		}
		else if (mime_type == "image/png") {
			return image_png;
		}
		else {
			throw std::invalid_argument("image mime_type");
		}
	}

	material_type::alpha_mode_type material_alpha_mode_from_json(const nlohmann::json &json) {
		if (json == "OPAQUE") {
			return material_type::alpha_mode_type::OPAQUE;
		}
		else if (json == "MASK") {
			return material_type::alpha_mode_type::MASK;
		}
		else if (json == "BLEND") {
			return material_type::alpha_mode_type::BLEND;
		}
		else {
			throw std::invalid_argument("alphaMode");
		}
	}

	attribute_type attribute_from_json(const nlohmann::json &attribute) {
		if (attribute == "POSITION") {
			return attribute_type::POSITION;
		}
		else if (attribute == "NORMAL") {
			return attribute_type::NORMAL;
		}
		else if (attribute == "TANGENT") {
			return attribute_type::TANGENT;
		}
		else if (attribute == "TEXCOORD_0") {
			return attribute_type::TEXCOORD_0;
		}
		else if (attribute == "TEXCOORD_1") {
			return attribute_type::TEXCOORD_1;
		}
		else if (attribute == "COLOR_0") {
			return attribute_type::COLOR_0;
		}
		else if (attribute == "JOINTS_0") {
			return attribute_type::JOINTS_0;
		}
		else if (attribute == "WEIGHTS_0") {
			return attribute_type::WEIGHTS_0;
		}
		else {
			throw std::invalid_argument("attribute");
		}
	}

	primitive_type::morph_target_attribute_type morph_target_attribute_from_json(const nlohmann::json &attribute) {
		if (attribute == "POSITION") {
			return primitive_type::morph_target_attribute_type::POSITION;
		}
		else if (attribute == "NORMAL") {
			return primitive_type::morph_target_attribute_type::NORMAL;
		}
		else if (attribute == "TANGENT") {
			return primitive_type::morph_target_attribute_type::TANGENT;
		}
		else {
			throw std::invalid_argument("morph target attribute");
		}
	}

	sampler_type::mag_filter_type sampler_mag_filter_from_json(unsigned_integer_type value) {
		switch (value) {
		case sampler_type::MAG_FILTER_NEAREST:
		case sampler_type::MAG_FILTER_LINEAR:
			return sampler_type::mag_filter_type(value);
		default:
			throw std::invalid_argument("magFilter");
		}
	}

	sampler_type::min_filter_type sampler_min_filter_from_json(unsigned_integer_type value) {
		switch (value) {
		case sampler_type::MIN_FILTER_NEAREST:
		case sampler_type::MIN_FILTER_LINEAR:
		case sampler_type::MIN_FILTER_NEAREST_MIPMAP_NEAREST:
		case sampler_type::MIN_FILTER_LINEAR_MIPMAP_NEAREST:
		case sampler_type::MIN_FILTER_NEAREST_MIPMAP_LINEAR:
		case sampler_type::MIN_FILTER_LINEAR_MIPMAP_LINEAR:
			return sampler_type::min_filter_type(value);
		default:
			throw std::invalid_argument("minFilter");
		}
	}

	sampler_type::wrap_type sampler_wrap_from_json(unsigned_integer_type value) {
		switch (value) {
		case sampler_type::CLAMP_TO_EDGE:
		case sampler_type::MIRRORED_REPEAT:
		case sampler_type::REPEAT:
			return (sampler_type::wrap_type) value;
		default:
			throw std::invalid_argument("sampler wrap");
		}
	}

} // namespace gltf
