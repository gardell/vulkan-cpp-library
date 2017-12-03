#ifndef _GLTF_TO_VULKAN_H_
#define _GLTF_TO_VULKAN_H_

#include <filesystem>
#include <gltf/types.h>
#include <type/supplier.h>
#include <type/transform.h>
#include <vcc/command.h>
#include <vcc/device.h>
#include <vcc/render_pass.h>
#include <vcc/pipeline.h>

namespace gltf_to_vulkan {
#ifdef _MSC_VER
	namespace fs = std::experimental::filesystem::v1;
#else
	namespace fs = std::filesystem;
#endif

	template<typename T>
	struct reference_wrapper_hash_type {
		size_t operator()(const std::reference_wrapper<T> &value) const {
			return size_t(&value.get());
		}
	};

	template<typename T>
	struct reference_wrapper_equality_type {
		size_t operator()(const std::reference_wrapper<T> &v1, const std::reference_wrapper<T> &v2) const {
			return &v1.get() == &v2.get();
		}
	};

	template<typename K, typename V>
	using gltf_to_vulkan_map_type = std::unordered_map<
		std::reference_wrapper<K>,
		V,
		reference_wrapper_hash_type<K>,
		reference_wrapper_equality_type<K>>;

	typedef gltf_to_vulkan_map_type<
		const gltf::buffer_type,
		type::supplier<vcc::input_buffer::input_buffer_type>
	> vertex_buffers_from_gltf_type;

	struct shaders_type {
		std::shared_ptr<vcc::descriptor_set_layout::descriptor_set_layout_type> desc_layout;
		std::shared_ptr<vcc::pipeline_layout::pipeline_layout_type> pipeline_layout;
		std::shared_ptr<vcc::shader_module::shader_module_type> vert_shader_module, frag_shader_module, geom_shader_module;
	};

	shaders_type shaders(const type::supplier<vcc::device::device_type> &device);

	vertex_buffers_from_gltf_type buffers(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const gltf::format_type &format,
		const gltf::container_type<gltf::buffer_type> &in_buffers);

	vcc::input_buffer::input_buffer_type buffer(
		const fs::path &wd,
		const vcc::device::device_type &device,
		const gltf::format_type &format,
		const gltf::buffer_type &in_buffer);

	struct material_type {
		std::shared_ptr<vcc::input_buffer::input_buffer_type> material_uniform_buffer;
		std::shared_ptr<type::vec4> base_color_factor;
		std::shared_ptr<type::float_type> metallic_factor, roughness_factor, normal_scale, occlusion_strength;
		std::shared_ptr<type::vec3> emissive_factor;
		std::optional<vcc::descriptor_set::image_info>
			base_color_image_info,
			normal_image_info,
			emissive_image_info,
			metallic_roughness_image_info,
			occlusion_image_info;
	};

	material_type material(
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const fs::path &wd,
		const gltf::format_type &format,
		const gltf::material_type &material);

	typedef gltf_to_vulkan_map_type<
		const gltf::material_type,
		material_type
	> materials_from_gltf_type;

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
		const type::supplier<type::transform_primitive_type<glm::mat4>> &inverse_modelview_matrix);

	materials_from_gltf_type materials_from_gltf(
		const vcc::device::device_type &device,
		const type::supplier<const vcc::queue::queue_type> &queue,
		const fs::path &wd,
		const gltf::format_type &format,
		const gltf::container_type<gltf::material_type> &materials);

} // namespace gltf_to_vulkan

#endif // _GLTF_TO_VULKAN_H_
