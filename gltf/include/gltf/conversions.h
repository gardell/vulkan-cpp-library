#ifndef _GLTF_CONVERSIONS_H_
#define _GLTF_CONVERSIONS_H_

#include <gltf/types.h>
#include <json.hpp>

namespace gltf {
	accessor_type::component_type_type component_type_from_json(integer_type component_type);
	accessor_type::type_type accessor_type_from_json(const json &json);
	container_type<number_type> number_container_from_json(const json &json);
	animation_sampler_type::interpolation_type animation_sampler_interpolation_from_json(
		const json &value);
	channel_type::target_type::path_type channel_target_path_from_json(const json &value);
	buffer_view_type::target_type buffer_view_target_from_json(unsigned_integer_type target);
	mime_type image_mime_type_from_json(const json &mime_type);
	material_type::alpha_mode_type material_alpha_mode_from_json(const json &json);
	attribute_type attribute_from_json(const json &attribute);
	sampler_type::mag_filter_type sampler_mag_filter_from_json(unsigned_integer_type value);
	sampler_type::min_filter_type sampler_min_filter_from_json(unsigned_integer_type value);
	sampler_type::wrap_type sampler_wrap_from_json(unsigned_integer_type value);
	primitive_type::morph_target_attribute_type morph_target_attribute_from_json(const json &attribute);
} // namespace gltf

#endif // _GLTF_CONVERSIONS_H_
