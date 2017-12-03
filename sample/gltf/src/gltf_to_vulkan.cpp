#include <fstream>
#include <glm/gtx/transform.hpp>
#include <gltf_to_vulkan.h>
#include <gltf/optional.h>
#include <gltf/parser.h>
#include <type/transform.h>
#include <vcc/image.h>
#include <vcc/image_view.h>
#include <vcc/image_loader.h>
#include <vcc/memory.h>

// wingdi.h defines OPAQUE...
#ifdef OPAQUE
#undef OPAQUE
#endif // OPAQUE

namespace gltf_to_vulkan {

	VkFormat gltf_accessor_type_to_vulkan_format(gltf::accessor_type::component_type_type component_type, gltf::accessor_type::type_type type, bool normalized) {
		switch (type) {
		case gltf::accessor_type::SCALAR:
			switch (component_type) {
			case gltf::accessor_type::BYTE:
				return normalized ? VK_FORMAT_R8_SNORM : VK_FORMAT_R8_SINT;
			case gltf::accessor_type::UNSIGNED_BYTE:
				return normalized ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8_UINT;
			case gltf::accessor_type::SHORT:
				return normalized ? VK_FORMAT_R16_SNORM : VK_FORMAT_R16_SINT;
			case gltf::accessor_type::UNSIGNED_SHORT:
				return normalized ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16_UINT;
			case gltf::accessor_type::UNSIGNED_INT:
				if (normalized) {
					throw std::invalid_argument("normalized UNSIGNED_INT not supported");
				}
				return VK_FORMAT_R32_UINT;
			case gltf::accessor_type::FLOAT:
				return VK_FORMAT_R32_SFLOAT;
			}
		case gltf::accessor_type::VEC2:
			switch (component_type) {
			case gltf::accessor_type::BYTE:
				return normalized ? VK_FORMAT_R8G8_SNORM : VK_FORMAT_R8G8_SINT;
			case gltf::accessor_type::UNSIGNED_BYTE:
				return normalized ? VK_FORMAT_R8G8_UNORM : VK_FORMAT_R8G8_UINT;
			case gltf::accessor_type::SHORT:
				return normalized ? VK_FORMAT_R16G16_SNORM : VK_FORMAT_R16G16_SINT;
			case gltf::accessor_type::UNSIGNED_SHORT:
				return normalized ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R16G16_UINT;
			case gltf::accessor_type::UNSIGNED_INT:
				if (normalized) {
					throw std::invalid_argument("normalized UNSIGNED_INT not supported");
				}
				return VK_FORMAT_R32G32_UINT;
			case gltf::accessor_type::FLOAT:
				return VK_FORMAT_R32G32_SFLOAT;
			}
		case gltf::accessor_type::VEC3:
			switch (component_type) {
			case gltf::accessor_type::BYTE:
				return normalized ? VK_FORMAT_R8G8B8_SNORM : VK_FORMAT_R8G8B8_SINT;
			case gltf::accessor_type::UNSIGNED_BYTE:
				return normalized ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8_UINT;
			case gltf::accessor_type::SHORT:
				return normalized ? VK_FORMAT_R16G16B16_SNORM : VK_FORMAT_R16G16B16_SINT;
			case gltf::accessor_type::UNSIGNED_SHORT:
				return normalized ? VK_FORMAT_R16G16B16_UNORM : VK_FORMAT_R16G16B16_UINT;
			case gltf::accessor_type::UNSIGNED_INT:
				if (normalized) {
					throw std::invalid_argument("normalized UNSIGNED_INT not supported");
				}
				return VK_FORMAT_R32G32B32_UINT;
			case gltf::accessor_type::FLOAT:
				return VK_FORMAT_R32G32B32_SFLOAT;
			}
		case gltf::accessor_type::VEC4:
			switch (component_type) {
			case gltf::accessor_type::BYTE:
				return normalized ? VK_FORMAT_R8G8B8A8_SNORM : VK_FORMAT_R8G8B8A8_SINT;
			case gltf::accessor_type::UNSIGNED_BYTE:
				return normalized ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_UINT;
			case gltf::accessor_type::SHORT:
				return normalized ? VK_FORMAT_R16G16B16A16_SNORM : VK_FORMAT_R16G16B16A16_SINT;
			case gltf::accessor_type::UNSIGNED_SHORT:
				return normalized ? VK_FORMAT_R16G16B16A16_UNORM : VK_FORMAT_R16G16B16A16_UINT;
			case gltf::accessor_type::UNSIGNED_INT:
				if (normalized) {
					throw std::invalid_argument("normalized UNSIGNED_INT not supported");
				}
				return VK_FORMAT_R32G32B32A32_UINT;
			case gltf::accessor_type::FLOAT:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			}
		case gltf::accessor_type::MAT2:
		case gltf::accessor_type::MAT3:
		case gltf::accessor_type::MAT4:
			throw std::invalid_argument("matrix primitive attributes unsupported");
		}
		throw std::invalid_argument("unknown accessor type");
	}

	off_t gltf_accessor_type_element_size(gltf::accessor_type::component_type_type component_type, gltf::accessor_type::type_type type) {
		off_t component_size;
		switch (component_type) {
		case gltf::accessor_type::BYTE:
		case gltf::accessor_type::UNSIGNED_BYTE:
			component_size = sizeof(uint8_t);
			break;
		case gltf::accessor_type::SHORT:
		case gltf::accessor_type::UNSIGNED_SHORT:
			component_size = sizeof(uint16_t);
			break;
		case gltf::accessor_type::UNSIGNED_INT:
		case gltf::accessor_type::FLOAT:
			component_size = sizeof(uint32_t);
			break;
		default:
			throw std::invalid_argument("unknown accessor component type");
		}
		switch (type) {
		case gltf::accessor_type::SCALAR:
			return component_size;
		case gltf::accessor_type::VEC2:
			return 2 * component_size;
		case gltf::accessor_type::VEC3:
			return 3 * component_size;
		case gltf::accessor_type::VEC4:
			return 4 * component_size;
		case gltf::accessor_type::MAT2:
			return 2 * 2 * component_size;
		case gltf::accessor_type::MAT3:
			return 3 * 3 * component_size;
		case gltf::accessor_type::MAT4:
			return 4 * 4 * component_size;
		default:
			throw std::invalid_argument("unknown accessor type");
		}
	}

	uint32_t gltf_attribute_to_location(gltf::attribute_type attribute) {
		switch (attribute) {
		case gltf::POSITION:
			return 0;
		case gltf::NORMAL:
			return 1;
		case gltf::TANGENT:
			return 2;
		case gltf::TEXCOORD_0:
			return 3;
		case gltf::TEXCOORD_1:
			return 4;
		case gltf::COLOR_0:
		case gltf::JOINTS_0:
		case gltf::WEIGHTS_0:
		default:
			throw std::invalid_argument("shader does not support attribute type");
		}
	}

	VkIndexType gltf_component_type_to_vulkan_index_type(gltf::accessor_type::component_type_type component_type) {
		switch (component_type) {
		case gltf::accessor_type::UNSIGNED_SHORT:
			return VK_INDEX_TYPE_UINT16;
		case gltf::accessor_type::UNSIGNED_INT:
			return VK_INDEX_TYPE_UINT32;
		case gltf::accessor_type::BYTE:
		case gltf::accessor_type::UNSIGNED_BYTE:
		case gltf::accessor_type::SHORT:
		case gltf::accessor_type::FLOAT:
		default:
			throw std::invalid_argument("component type not supported for index");
		}
	}

	VkPrimitiveTopology gltf_mode_to_vulkan_topology(gltf::primitive_type::mode_type mode) {
		switch (mode) {
		case gltf::primitive_type::POINTS:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case gltf::primitive_type::LINES:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case gltf::primitive_type::LINE_STRIP:
			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case gltf::primitive_type::TRIANGLES:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case gltf::primitive_type::TRIANGLE_STRIP:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case gltf::primitive_type::TRIANGLE_FAN:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
		case gltf::primitive_type::LINE_LOOP:
		default:
			throw std::invalid_argument("primitive mode not supported");
		}
	}

	VkFilter gltf_mag_filter_to_vulkan(gltf::sampler_type::mag_filter_type mag_filter) {
		switch (mag_filter) {
		case gltf::sampler_type::MAG_FILTER_LINEAR:
			return VK_FILTER_LINEAR;
		case gltf::sampler_type::MAG_FILTER_NEAREST:
			return VK_FILTER_NEAREST;
		default:
			throw std::invalid_argument("invalid mag_filter");
		}
	}

	VkFilter gltf_min_filter_to_vulkan(gltf::sampler_type::min_filter_type min_filter) {
		switch (min_filter) {
		case gltf::sampler_type::MIN_FILTER_LINEAR:
		case gltf::sampler_type::MIN_FILTER_LINEAR_MIPMAP_NEAREST:
		case gltf::sampler_type::MIN_FILTER_LINEAR_MIPMAP_LINEAR:
			return VK_FILTER_LINEAR;
		case gltf::sampler_type::MIN_FILTER_NEAREST:
		case gltf::sampler_type::MIN_FILTER_NEAREST_MIPMAP_NEAREST:
		case gltf::sampler_type::MIN_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_FILTER_NEAREST;
		default:
			throw std::invalid_argument("invalid min_filter");
		}
	}

	VkSamplerMipmapMode gltf_sampler_mipmap_mode(gltf::sampler_type::min_filter_type min_filter) {
		switch (min_filter) {
		case gltf::sampler_type::MIN_FILTER_LINEAR_MIPMAP_LINEAR:
		case gltf::sampler_type::MIN_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case gltf::sampler_type::MIN_FILTER_LINEAR_MIPMAP_NEAREST:
		case gltf::sampler_type::MIN_FILTER_NEAREST_MIPMAP_NEAREST:
		default:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}
	}

	VkSamplerAddressMode gltf_sampler_address_mode_to_vulkan(gltf::sampler_type::wrap_type wrap) {
		switch (wrap) {
		case gltf::sampler_type::CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case gltf::sampler_type::MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case gltf::sampler_type::REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		default:
			throw std::invalid_argument("invalid wrap type");
		}
	}

	vcc::descriptor_set::image_info sampler(
			const vcc::device::device_type &device,
			const type::supplier<const vcc::queue::queue_type> &queue,
			const fs::path &wd,
			const gltf::format_type &format,
			const gltf::texture_type &texture) {

#if defined(__ANDROID__) || defined(ANDROID)
		auto image(vcc::image::create(queue,
			0, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, env, state->activity->clazz,
			"png_transparency_demonstration_1", true));
#else
		std::istringstream stream(
			std::visit(
				[](const auto &value) { return std::string(std::cbegin(value), std::cend(value)); },
				gltf::open(wd, format, *texture.source.value())), std::ios::in | std::ios::binary);
		auto image(vcc::image::create(queue,
			0, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {},
			std::move(stream), false));
#endif  // __ANDROID__

		auto image_view(vcc::image_view::create(std::move(image),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }));

		const auto default_sampler(gltf::sampler_type{
			gltf::sampler_type::MAG_FILTER_LINEAR,
			gltf::sampler_type::MIN_FILTER_LINEAR,
			gltf::sampler_type::REPEAT,
			gltf::sampler_type::REPEAT
		});
		const auto &sampler_in(map(texture.sampler, [](const auto &sampler) { return std::cref(*sampler); })
			.value_or(std::cref(default_sampler)));
		auto mag_filter(sampler_in.get().mag_filter.value_or(gltf::sampler_type::MAG_FILTER_NEAREST));
		auto min_filter(sampler_in.get().min_filter.value_or(gltf::sampler_type::MIN_FILTER_NEAREST));
		auto sampler(vcc::sampler::create(
			std::ref(device),
			gltf_mag_filter_to_vulkan(mag_filter),
			gltf_min_filter_to_vulkan(min_filter),
			gltf_sampler_mipmap_mode(min_filter),
			gltf_sampler_address_mode_to_vulkan(sampler_in.get().wrap_s),
			gltf_sampler_address_mode_to_vulkan(sampler_in.get().wrap_t),
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0, VK_TRUE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE));

		return vcc::descriptor_set::image_info{
			std::move(sampler),
			std::move(image_view),
		};
	}

	shaders_type shaders(const type::supplier<vcc::device::device_type> &device) {
		auto desc_layout(std::make_shared<vcc::descriptor_set_layout::descriptor_set_layout_type>(
			vcc::descriptor_set_layout::create(device,
			{
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					0,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					1,
					VK_SHADER_STAGE_VERTEX_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					1,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					2,
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					3,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					4,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					5,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					6,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
				vcc::descriptor_set_layout::descriptor_set_layout_binding{
					7,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					{},
				},
					})));

		auto pipeline_layout(std::make_shared<vcc::pipeline_layout::pipeline_layout_type>(vcc::pipeline_layout::create(
			device, { desc_layout })));

		vcc::shader_module::shader_module_type vert_shader_module(
			vcc::shader_module::create(device,
#if defined(__ANDROID__) || defined(ANDROID)
				android::asset_istream(state->activity->assetManager, "gltf-vert.spv")
#else
				std::ifstream("gltf-vert.spv",
					std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));
		vcc::shader_module::shader_module_type frag_shader_module(
			vcc::shader_module::create(device,
#if defined(__ANDROID__) || defined(ANDROID)
				android::asset_istream(state->activity->assetManager, "gltf-frag.spv")
#else
				std::ifstream("gltf-frag.spv",
					std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));
		vcc::shader_module::shader_module_type geom_shader_module(
			vcc::shader_module::create(device,
#if defined(__ANDROID__) || defined(ANDROID)
				android::asset_istream(state->activity->assetManager, "gltf-geom.spv")
#else
				std::ifstream("gltf-geom.spv",
					std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));

		return shaders_type{
			desc_layout,
			pipeline_layout,
			std::make_shared<vcc::shader_module::shader_module_type>(std::move(vert_shader_module)),
			std::make_shared<vcc::shader_module::shader_module_type>(std::move(frag_shader_module)),
			std::make_shared<vcc::shader_module::shader_module_type>(std::move(geom_shader_module)),
		};
	}

	vcc::input_buffer::input_buffer_type buffer(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const gltf::format_type &format,
		const gltf::buffer_type &in_buffer) {
		return std::visit(
			[&](const auto &value) {
				vcc::input_buffer::input_buffer_type buffer(vcc::input_buffer::create<type::linear>(
					// TODO(gardell): Inspect buffer views to figure out usage pattern.
					std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
					VK_SHARING_MODE_EXCLUSIVE, {}, type::ubyte_array(std::cbegin(value), std::cend(value))));
				vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, buffer);
				return std::move(buffer);
			},
			gltf::open(wd, format, in_buffer));
	}

	vertex_buffers_from_gltf_type buffers(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const gltf::format_type &format,
		const gltf::container_type<gltf::buffer_type> &in_buffers) {
		vertex_buffers_from_gltf_type buffers;
		buffers.reserve(in_buffers.size());
		std::transform(std::cbegin(in_buffers), std::cend(in_buffers), std::inserter(buffers, std::end(buffers)),
			[&](const auto &in_buffer) {
				return std::make_pair(
					std::cref(in_buffer),
					std::make_shared<vcc::input_buffer::input_buffer_type>(buffer(wd, device, format, in_buffer)));
			});
		return buffers;
	}

	material_type material(
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const fs::path &wd,
		const gltf::format_type &format,
		const gltf::material_type &material) {

		auto base_color_factor(std::make_shared<type::vec4>(material.pbr_metallic_roughness.base_color_factor.value_or(glm::vec4(1, 1, 1, 1))));
		auto metallic_factor(std::make_shared<type::float_type>(material.pbr_metallic_roughness.metallic_factor.value_or(1)));
		auto roughness_factor(std::make_shared<type::float_type>(material.pbr_metallic_roughness.roughness_factor.value_or(1)));
		float normal_scale_(map(material.normal_texture, [](const auto &normal_texture) { return normal_texture.scale.value_or(1); }).value_or(1));
		auto normal_scale(std::make_shared<type::float_type>(normal_scale_));
		float occlusion_strength_(map(material.occlusion_texture, [](const auto &occlusion_texture) { return occlusion_texture.strength.value_or(1); }).value_or(1));
		auto occlusion_strength(std::make_shared<type::float_type>(occlusion_strength_));
		auto emissive_factor(std::make_shared<type::vec3>(material.emissive_factor.value_or(glm::vec3(0, 0, 0))));

		auto material_uniform_buffer(std::make_shared<vcc::input_buffer::input_buffer_type>(
			vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
				base_color_factor,
				metallic_factor,
				roughness_factor,
				normal_scale,
				occlusion_strength,
				emissive_factor)));

		auto base_color_image_info(map(
			material.pbr_metallic_roughness.base_color_texture,
			[&](const auto &texture) {
				return sampler(device, queue, wd, format, *texture.index);
			}));
		auto normal_image_info(map(
			material.normal_texture,
			[&](const auto &texture) {
			return sampler(device, queue, wd, format, *texture.index);
		}));
		auto emissive_image_info(map(
			material.emissive_texture,
			[&](const auto &texture) {
			return sampler(device, queue, wd, format, *texture.index);
		}));
		auto metallic_roughness_image_info(map(
			material.pbr_metallic_roughness.metallic_roughness_texture,
			[&](const auto &texture) {
			return sampler(device, queue, wd, format, *texture.index);
		}));
		auto occlusion_image_info(map(
			material.occlusion_texture,
			[&](const auto &texture) {
			return sampler(device, queue, wd, format, *texture.index);
		}));

		return material_type{
			material_uniform_buffer,
			base_color_factor,
			metallic_factor,
			roughness_factor,
			normal_scale,
			occlusion_strength,
			emissive_factor,
			base_color_image_info,
			normal_image_info,
			emissive_image_info,
			metallic_roughness_image_info,
			occlusion_image_info
		};
	}

	vcc::command_buffer::command_buffer_type command_buffer_primitive(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const vcc::render_pass::render_pass_type &render_pass,
		const vcc::pipeline_cache::pipeline_cache_type &pipeline_cache,
		const vcc::command_pool::command_pool_type &cmd_pool,
		const type::supplier<vcc::input_buffer::input_buffer_type> &matrix_uniform_buffer,
		const gltf::material_type &gltf_material,
		const material_type &material,
		const type::supplier<vcc::input_buffer::input_buffer_type> &lights_uniform_buffer,
		const std::size_t num_lights,
		const gltf::format_type &format,
		const gltf::primitive_type &primitive,
		const vertex_buffers_from_gltf_type &vertex_buffers_from_gltf,
		const shaders_type &shaders,
		uint32_t instance_count) {
		std::set<gltf::iterator_type<gltf::buffer_type>> used_buffers;
		std::set<gltf::iterator_type<gltf::buffer_view_type>> used_buffer_views;
		if (!primitive.indices || !primitive.indices.value()->buffer_view) {
			// FIXME
			throw std::invalid_argument("don't support non-indiced meshes");
		}
		used_buffers.insert(primitive.indices.value()->buffer_view.value()->buffer);
		for (const auto &attribute : primitive.attributes) {
			if (attribute.second->buffer_view) {
				const auto &buffer_view(attribute.second->buffer_view.value());
				used_buffers.insert(buffer_view->buffer);
				used_buffer_views.insert(buffer_view);
			}
		}

		vcc::pipeline::vertex_input_state vertex_input_state;
		vcc::command::bind_vertex_data_buffers_type bind_vertex_data_buffers;
		vertex_input_state.vertexBindingDescriptions.reserve(used_buffer_views.size());
		bind_vertex_data_buffers.first_binding = 0;
		bind_vertex_data_buffers.buffers.reserve(used_buffer_views.size());
		bind_vertex_data_buffers.offsets.reserve(used_buffer_views.size());

		uint32_t binding(bind_vertex_data_buffers.first_binding);
		for (const auto &buffer_view : used_buffer_views) {
			uint32_t byte_stride;
			if (buffer_view->byte_stride) {
				byte_stride = buffer_view->byte_stride.value();
			} else {
				const auto last_attribute(std::max_element(
					std::cbegin(primitive.attributes),
					std::cend(primitive.attributes),
					[&](const auto &attribute1, const auto &attribute2) {
					bool attribute1_relevant(attribute1.second->buffer_view == buffer_view);
					bool attribute2_relevant(attribute2.second->buffer_view == buffer_view);
					if (!attribute1_relevant || !attribute2_relevant) {
						return attribute2_relevant;
					}
					else {
						return attribute1.second->byte_offset < attribute2.second->byte_offset;
					}
				}
				));
				if (last_attribute == std::cend(primitive.attributes)) {
					throw std::invalid_argument("no attributes targeting bufferview");
				}
				byte_stride = last_attribute->second->byte_offset
					+ gltf_accessor_type_element_size(
						last_attribute->second->component_type,
						last_attribute->second->type);
			}
			vertex_input_state.vertexBindingDescriptions.push_back(
				VkVertexInputBindingDescription{
				binding++,
				byte_stride,
				VK_VERTEX_INPUT_RATE_VERTEX
			}
			);
			// used_buffers is a set, so find not necessary
			const auto &used_buffer(std::find(std::cbegin(used_buffers), std::cend(used_buffers), buffer_view->buffer));
			bind_vertex_data_buffers.buffers.push_back(vertex_buffers_from_gltf.at(**used_buffer));
			bind_vertex_data_buffers.offsets.push_back(buffer_view->byte_offset);
		}

		vertex_input_state.vertexAttributeDescriptions.reserve(primitive.attributes.size());
		std::transform(
			std::cbegin(primitive.attributes),
			std::cend(primitive.attributes),
			std::back_inserter(vertex_input_state.vertexAttributeDescriptions),
			[&](const auto &attribute) {
			return VkVertexInputAttributeDescription{
				gltf_attribute_to_location(attribute.first),
				uint32_t(std::distance(
					std::cbegin(used_buffer_views),
					// used_buffer_views is a set, so find not necessary
					std::find(std::cbegin(used_buffer_views), std::cend(used_buffer_views), attribute.second->buffer_view))),
				gltf_accessor_type_to_vulkan_format(attribute.second->component_type, attribute.second->type, attribute.second->normalized),
				uint32_t(attribute.second->byte_offset)
			};
		}
		);

		vcc::descriptor_pool::descriptor_pool_type desc_pool(
			vcc::descriptor_pool::create(std::ref(device), 0, 1,
			{
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 }
			}));

		auto desc_set(std::move(vcc::descriptor_set::create(std::ref(device), std::move(desc_pool), { shaders.desc_layout })
			.front()));

		vcc::input_buffer::input_buffer_type material_uniform_buffer(
			vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
				material.base_color_factor,
				material.metallic_factor,
				material.roughness_factor,
				material.normal_scale,
				material.occlusion_strength,
				material.emissive_factor));

		vcc::memory::bind(
			std::ref(device),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			material_uniform_buffer);

		vcc::descriptor_set::update(device,
			vcc::descriptor_set::write_buffer(desc_set, 0, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(matrix_uniform_buffer) }),
			vcc::descriptor_set::write_buffer(desc_set, 1, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(std::move(material_uniform_buffer)) }),
			vcc::descriptor_set::write_buffer(desc_set, 2, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(lights_uniform_buffer) })
		);

		map(
			material.base_color_image_info,
			[&](const auto &texture) {
			vcc::descriptor_set::update(device,
				vcc::descriptor_set::write_image{ desc_set, 3, 0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{ texture }
				});
			});
		map(
			material.normal_image_info,
			[&](const auto &texture) {
			vcc::descriptor_set::update(device,
				vcc::descriptor_set::write_image{ desc_set, 4, 0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{ texture }
			});
		});
		map(
			material.emissive_image_info,
			[&](const auto &texture) {
			vcc::descriptor_set::update(device,
				vcc::descriptor_set::write_image{ desc_set, 5, 0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{ texture }
			});
		});
		map(
			material.metallic_roughness_image_info,
			[&](const auto &texture) {
			vcc::descriptor_set::update(device,
				vcc::descriptor_set::write_image{ desc_set, 6, 0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{ texture }
			});
		});
		map(
			material.occlusion_image_info,
			[&](const auto &texture) {
			vcc::descriptor_set::update(device,
				vcc::descriptor_set::write_image{ desc_set, 7, 0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{ texture }
			});
		});

		struct specialization_constants_type {
			int max_lights;
			bool
				enable_tangent,
				enable_texturing,
				enable_base_color_sampler,
				enable_emissive_sampler,
				enable_metallic_roughness_sampler,
				enable_occlusion_sampler;
			int base_color_sampler_texcoord_index,
				normal_sampler_texcoord_index,
				emissive_sampler_texcoord_index,
				metallic_roughness_sampler_texcoord_index,
				occlusion_sampler_texcoord_index;

			VCC_STRUCT_SERIALIZABLE(
				max_lights,
				enable_tangent,
				enable_texturing,
				enable_base_color_sampler,
				enable_emissive_sampler,
				enable_metallic_roughness_sampler,
				enable_occlusion_sampler,
				base_color_sampler_texcoord_index,
				normal_sampler_texcoord_index,
				emissive_sampler_texcoord_index,
				metallic_roughness_sampler_texcoord_index,
				occlusion_sampler_texcoord_index);
		} specialization_constants{
			int(num_lights),
			!!primitive.attributes.count(gltf::TANGENT) && !!primitive.attributes.count(gltf::NORMAL) && material.normal_image_info,
			!!primitive.attributes.count(gltf::TEXCOORD_0) || !!primitive.attributes.count(gltf::TEXCOORD_1),
			material.base_color_image_info.has_value(),
			material.emissive_image_info.has_value(),
			material.metallic_roughness_image_info.has_value(),
			material.occlusion_image_info.has_value(),
			map(gltf_material.pbr_metallic_roughness.base_color_texture, [](const auto &texture) { return texture.texcoord; }).value_or(0),
			map(gltf_material.normal_texture, [](const auto &texture) { return texture.texcoord; }).value_or(0),
			map(gltf_material.emissive_texture, [](const auto &texture) { return texture.texcoord; }).value_or(0),
			map(gltf_material.pbr_metallic_roughness.metallic_roughness_texture, [](const auto &texture) { return texture.texcoord; }).value_or(0),
			map(gltf_material.occlusion_texture, [](const auto &texture) { return texture.texcoord; }).value_or(0),
		};

		std::vector<vcc::pipeline::shader_stage_type> stages{
			vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT,
				type::make_supplier(shaders.vert_shader_module), "main",
				{
					VkSpecializationMapEntry{ 1, sizeof(int), sizeof(bool) },
					VkSpecializationMapEntry{ 2, sizeof(int) + sizeof(bool), sizeof(bool) },
				},
				type::t_array<specialization_constants_type>{specialization_constants}),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT,
				type::make_supplier(shaders.frag_shader_module), "main",
				{
					VkSpecializationMapEntry{ 0, sizeof(int) * 0, sizeof(int) },
					VkSpecializationMapEntry{ 1, sizeof(int) * 1 + sizeof(bool) * 0, sizeof(bool) },
					VkSpecializationMapEntry{ 2, sizeof(int) * 1 + sizeof(bool) * 1, sizeof(bool) },
					VkSpecializationMapEntry{ 3, sizeof(int) * 1 + sizeof(bool) * 2, sizeof(bool) },
					VkSpecializationMapEntry{ 4, sizeof(int) * 1 + sizeof(bool) * 3, sizeof(bool) },
					VkSpecializationMapEntry{ 5, sizeof(int) * 1 + sizeof(bool) * 4, sizeof(bool) },
					VkSpecializationMapEntry{ 6, sizeof(int) * 1 + sizeof(bool) * 5, sizeof(bool) },

					VkSpecializationMapEntry{ 7, sizeof(int) * 1 + sizeof(bool) * 6, sizeof(int) },
					VkSpecializationMapEntry{ 8, sizeof(int) * 2 + sizeof(bool) * 6, sizeof(int) },
					VkSpecializationMapEntry{ 9, sizeof(int) * 3 + sizeof(bool) * 6, sizeof(int) },
					VkSpecializationMapEntry{ 10, sizeof(int) * 4 + sizeof(bool) * 6, sizeof(int) },
					VkSpecializationMapEntry{ 11, sizeof(int) * 5 + sizeof(bool) * 6, sizeof(int) },
				},
				type::t_array<specialization_constants_type>{specialization_constants}),
		};
		if (!primitive.attributes.count(gltf::NORMAL)) {
			stages.push_back(
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_GEOMETRY_BIT,
					type::make_supplier(shaders.geom_shader_module), "main"));
		}

		VkBool32 blend_enable(gltf_material.alpha_mode == gltf::material_type::BLEND ? VK_TRUE : VK_FALSE);
		auto pipeline(vcc::pipeline::create_graphics(
			std::ref(device), pipeline_cache, 0,
			stages,
			std::move(vertex_input_state),
			vcc::pipeline::input_assembly_state{
				gltf_mode_to_vulkan_topology(primitive.mode),
				false
			},
			vcc::pipeline::viewport_state(1, 1),
			vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
				VK_POLYGON_MODE_FILL,
				VkCullModeFlags(gltf_material.double_sided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT),
				VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1 },
			vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT,
				VK_FALSE, 0,{}, VK_FALSE, VK_FALSE },
			vcc::pipeline::depth_stencil_state{
				VkBool32(gltf_material.alpha_mode != gltf::material_type::BLEND ? VK_TRUE : VK_FALSE),
				VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
					VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
					VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				0, 0 },
			vcc::pipeline::color_blend_state{
				blend_enable,
				VK_LOGIC_OP_COPY,
				{
					VkPipelineColorBlendAttachmentState{
						blend_enable, VK_BLEND_FACTOR_SRC_ALPHA,
						VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
						VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE,
						VK_BLEND_OP_ADD,
						VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
					}
				},
				{ 0, 0, 0, 0 } },
			vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR } },
			type::make_supplier(shaders.pipeline_layout), std::ref(render_pass), 0));

		auto subcommand_buffer(std::move(vcc::command_buffer::allocate(
			std::ref(device), std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_SECONDARY, 1).front()));

		// TODO(gardell): build() overload that takes render_pass and framebuffer should be used
		vcc::command::compile(
			vcc::command::build(
				std::ref(subcommand_buffer),
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
				VK_FALSE,
				0,
				0),
			vcc::command::bind_pipeline{
			VK_PIPELINE_BIND_POINT_GRAPHICS, std::move(pipeline) },
			std::move(bind_vertex_data_buffers),
			vcc::command::bind_index_data_buffer_type{
			vertex_buffers_from_gltf.at(*primitive.indices.value()->buffer_view.value()->buffer),
			VkDeviceSize(
				primitive.indices.value()->byte_offset
				+ primitive.indices.value()->buffer_view.value()->byte_offset),
			gltf_component_type_to_vulkan_index_type(primitive.indices.value()->component_type)
		},
			vcc::command::bind_descriptor_sets{
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				type::make_supplier(shaders.pipeline_layout), 0,
				{ type::make_supplier(std::move(desc_set)) },{} },
			vcc::command::draw_indexed{
				uint32_t(primitive.indices.value()->count), instance_count, 0, 0, 0
			}
		);

		return subcommand_buffer;
	}

	void scene(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const vcc::render_pass::render_pass_type &render_pass,
		const vcc::pipeline_cache::pipeline_cache_type &pipeline_cache,
		const vcc::command_pool::command_pool_type &cmd_pool,
		const gltf::format_type &format,
		const vertex_buffers_from_gltf_type &vertex_buffers_from_gltf,
		const materials_from_gltf_type &materials_from_gltf,
		const type::supplier<vcc::input_buffer::input_buffer_type> &lights_uniform_buffer,
		const std::size_t num_lights,
		const shaders_type &shaders,
		const gltf::node_type &node,
		const type::supplier<type::mat4> &projection_matrix,
		const type::supplier<type::mat4> &modelview_matrix,
		const type::supplier<type::transform_primitive_type<glm::mat4>> &inverse_projection_matrix,
		const type::supplier<type::transform_primitive_type<glm::mat4>> &inverse_modelview_matrix,
		std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>> &command_buffers) {
		for (const auto &node : node.children) {
			scene(wd, device, queue, render_pass, pipeline_cache, cmd_pool, format,
				vertex_buffers_from_gltf, materials_from_gltf, lights_uniform_buffer,
				num_lights, shaders, *node, projection_matrix, modelview_matrix,
				inverse_projection_matrix, inverse_modelview_matrix, command_buffers);
		}
		if (node.mesh) {
			glm::mat4 transformation(1.f), inverse_transformation(1.f);
			if (auto t = std::get_if<glm::mat4>(&node.transformation)) {
				transformation = *t;
				inverse_transformation = glm::inverse(*t);
			} else if (auto translation_rotation_scale = std::get_if<gltf::translation_rotation_scale_type>(&node.transformation)) {
				if (translation_rotation_scale->translation) {
					transformation = glm::translate(*translation_rotation_scale->translation);
					inverse_transformation = glm::translate(-*translation_rotation_scale->translation);
				}
				if (translation_rotation_scale->rotation) {
					transformation *= glm::mat4(glm::mat3_cast(*translation_rotation_scale->rotation));
					inverse_transformation = glm::mat4(glm::mat3_cast(glm::inverse(*translation_rotation_scale->rotation))) * inverse_transformation;
				}
				if (translation_rotation_scale->scale) {
					transformation *= glm::scale(*translation_rotation_scale->scale);
					inverse_transformation = glm::scale(glm::vec3(
						1 / translation_rotation_scale->scale->x,
						1 / translation_rotation_scale->scale->y,
						1 / translation_rotation_scale->scale->z)) * inverse_transformation;
				}
			}
			auto transform_modelview_matrix(std::make_shared<type::transform_primitive_type<glm::mat4>>(type::make_transform(
				type::mat4(),
				[=](const auto &input, auto &&output) {
					output[0] = input[0] * transformation;
				},
				modelview_matrix)));
			auto transform_modelview_projection_matrix(type::make_transform(
				type::mat4(),
				[=](const auto &modelview_matrix, const auto &projection_matrix, auto &&output) {
					output[0] = projection_matrix[0] * modelview_matrix[0];
				},
				transform_modelview_matrix, projection_matrix));
			auto transform_normal_matrix(type::make_transform(
				type::mat3(),
				[=](const auto &inverse_modelview_matrix, auto &&output) {
					output[0] = glm::transpose(glm::mat3(inverse_transformation * inverse_modelview_matrix[0]));
				},
				inverse_modelview_matrix));
			const auto matrix_uniform_buffer(std::make_shared<vcc::input_buffer::input_buffer_type>(
				vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
					std::make_shared<type::transform_primitive_type<glm::mat4>>(std::move(transform_modelview_projection_matrix)),
					transform_modelview_matrix,
					std::make_shared<type::transform_primitive_type<glm::mat3>>(std::move(transform_normal_matrix)))));
			vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, *matrix_uniform_buffer);

			std::optional<material_type> default_material;
			if (node.mesh) {
				for (const auto &primitive : node.mesh.value()->primitives) {
					if (!primitive.material) {
						if (!default_material) {
							default_material = material(device, queue, wd, format,
								gltf::material_type{
									std::optional<std::string>(),
									json(), json(),

									gltf::material_type::pbr_metallic_roughness_type{},
									std::optional<gltf::normal_texture_info_type>(),
									std::optional<gltf::occlusion_texture_info_type>(),
									std::optional<gltf::texture_info_type>(),
									std::optional<glm::vec3>(),
									gltf::material_type::OPAQUE,
									std::optional<gltf::decimal_type>(),
									false,
								});
						}
					};
					if (primitive.material) {
						command_buffers.push_back(command_buffer_primitive(
							wd, device, queue, render_pass, pipeline_cache, cmd_pool,
							matrix_uniform_buffer,
							*primitive.material.value(),
							primitive.material ? materials_from_gltf.at(*primitive.material.value()) : default_material.value(),
							lights_uniform_buffer, num_lights, format, primitive,
							vertex_buffers_from_gltf, shaders, 1));
					}
				}
			}
		}
	}

	std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>> scene(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const vcc::render_pass::render_pass_type &render_pass,
		const vcc::pipeline_cache::pipeline_cache_type &pipeline_cache,
		const vcc::command_pool::command_pool_type &cmd_pool,
		const gltf::format_type &format,
		const vertex_buffers_from_gltf_type &vertex_buffers_from_gltf,
		const materials_from_gltf_type &materials_from_gltf,
		const type::supplier<vcc::input_buffer::input_buffer_type> &lights_uniform_buffer,
		const std::size_t num_lights,
		const shaders_type &shaders,
		const gltf::scene_type &scene_,
		const type::supplier<type::mat4> &projection_matrix,
		const type::supplier<type::mat4> &modelview_matrix,
		const type::supplier<type::transform_primitive_type<glm::mat4>> &inverse_projection_matrix,
		const type::supplier<type::transform_primitive_type<glm::mat4>> &inverse_modelview_matrix) {

		std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>> command_buffers;
		if (scene_.nodes) {
			for (const auto &node : *scene_.nodes) {
				scene(wd, device, queue, render_pass, pipeline_cache, cmd_pool, format,
					vertex_buffers_from_gltf, materials_from_gltf, lights_uniform_buffer,
					num_lights,
					shaders, *node, projection_matrix, modelview_matrix,
					inverse_projection_matrix, inverse_modelview_matrix, command_buffers);
			}
		}
		return command_buffers;
	}

	materials_from_gltf_type materials_from_gltf(
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const fs::path &wd,
		const gltf::format_type &format,
		const gltf::container_type<gltf::material_type> &materials) {
		materials_from_gltf_type materials_from_gltf;
		materials_from_gltf.reserve(materials.size());
		std::transform(
			std::cbegin(materials),
			std::cend(materials),
			std::inserter(materials_from_gltf, std::end(materials_from_gltf)),
			[&](const auto &m) {
				return std::make_pair(
					std::cref(m),
					material(device, queue, wd, format, m)
				);
			});
		return materials_from_gltf;
	}

} // namespace gltf_to_vulkan
