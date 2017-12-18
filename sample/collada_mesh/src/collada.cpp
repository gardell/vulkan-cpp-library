/*
* Copyright 2016 Google Inc. All Rights Reserved.

* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <transform_iterator.h>
#include <triangulate.h>

#include <algorithm>
#include <cassert>
#include <collada_parser.h>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iterator>
#include <optional>
#include <type/transform.h>
#include <type/types.h>
#include <unordered_set>
#include <variant>
#include <vcc/android_asset_istream.h>
#include <vcc/buffer.h>
#include <vcc/command.h>
#include <vcc/command_buffer.h>
#include <vcc/command_pool.h>
#include <vcc/descriptor_pool.h>
#include <vcc/descriptor_set.h>
#include <vcc/descriptor_set_layout.h>
#include <vcc/device.h>
#include <vcc/enumerate.h>
#include <vcc/framebuffer.h>
#include <vcc/image.h>
#include <vcc/image_view.h>
#include <vcc/image_loader.h>
#include <vcc/input_buffer.h>
#include <vcc/instance.h>
#include <vcc/memory.h>
#include <vcc/physical_device.h>
#include <vcc/pipeline.h>
#include <vcc/pipeline_cache.h>
#include <vcc/pipeline_layout.h>
#include <vcc/queue.h>
#include <vcc/render_pass.h>
#include <vcc/sampler.h>
#include <vcc/semaphore.h>
#include <vcc/shader_module.h>
#include <vcc/surface.h>
#include <vcc/swapchain.h>
#include <vcc/util.h>
#include <vcc/window.h>

auto triangulate_polygon_mesh(const collada_parser::polygon_mesh_type &mesh) {
	std::vector<uint32_t> polygons;
	const auto vertices([&](auto index) { return mesh.vertices[index]; });
	for (const auto &polygon : mesh.polygons) {
		// TODO(gardell): Append to global mesh, not as a polygon_type but a global indices.
		// TODO(gardell): Also, indices here are relative to the polygon
		// TODO(gardell): Let polygon_triangulate take an output iterator, then we can just use the global iterator and do the index transform in one sweep
		const auto triangulated(polygon_triangulate(
			make_input_transform_iterator(std::cbegin(polygon.indices), vertices),
			make_input_transform_iterator(std::cend(polygon.indices), vertices)));
		std::transform(std::cbegin(triangulated), std::cend(triangulated), std::back_inserter(polygons), [&](auto index) { return polygon.indices[index]; });
	}
	return std::move(polygons);
}

template<typename T1, typename T2, typename H1 = std::hash<T1>, typename H2 = std::hash<T2>>
struct pair_hash_type {

	pair_hash_type(const H1 &h1 = H1(), const H2 &h2 = H2()) : h1(h1), h2(h2) {}

	size_t operator()(const std::pair<T1, T2> &pair) const {
		return 31 * h1(std::get<0>(pair)) + h2(std::get<1>(pair));
	}

	H1 h1;
	H2 h2;
};

template<typename T1, typename T2>
struct pair_equal_type {

	bool operator()(const std::pair<T1, T2> &p1, const std::pair<T1, T2> &p2) const {
		return p1.first == p2.first && p1.second == p2.second;
	}
};

auto polygon_mesh_lines(const collada_parser::polygon_mesh_type &mesh) {
	std::unordered_set<std::pair<uint32_t, uint32_t>, pair_hash_type<uint32_t, uint32_t>, pair_equal_type<uint32_t, uint32_t>> line_pairs;
	for (const auto &polygon : mesh.polygons) {
		if (polygon.indices.size() >= 2) {
			const auto begin(std::cbegin(polygon.indices)), end(std::cend(polygon.indices));
			for (auto it(begin); it != end; ++it) {
				line_pairs.emplace(*it, *(it + 1 != end ? it + 1 : begin));
			}
		}
	}
	std::vector<uint32_t> lines;
	lines.reserve(line_pairs.size() * 2);
	for (const auto &pair : line_pairs) {
		lines.push_back(std::get<0>(pair));
		lines.push_back(std::get<1>(pair));
	}
	return std::move(lines);
}

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

struct mesh_buffers_type {
	std::shared_ptr<vcc::input_buffer::input_buffer_type> vertex_buffer, indices_buffer;
	uint32_t indices_count, vertices_count;
};

typedef std::unordered_map<
	std::reference_wrapper<const collada_parser::polygon_mesh_type>,
	mesh_buffers_type,
	reference_wrapper_hash_type<const collada_parser::polygon_mesh_type>,
	reference_wrapper_equality_type<const collada_parser::polygon_mesh_type>
> mesh_buffers_map_type;

struct renderable_mesh_type {
	const collada_parser::polygon_mesh_type &polygon_mesh;
	std::shared_ptr<vcc::descriptor_set::descriptor_set_type> desc_set;
};

std::vector<renderable_mesh_type> mesh(
	vcc::device::device_type &device,
	type::mat4 &projection_matrix,
	type::mat4 &modelview_matrix,
	type::transform_primitive_type<glm::mat4> &inverse_modelview_matrix,
	const vcc::input_buffer::input_buffer_type &light_uniform_buffer,
	size_t num_lights,
	const vcc::descriptor_set_layout::descriptor_set_layout_type &desc_layout,
	const collada_parser::node_type &node) {
	std::vector<renderable_mesh_type> renderable_meshes;

	for (const auto &child : node.child_nodes) {
		auto meshes(mesh(device, projection_matrix, modelview_matrix, inverse_modelview_matrix, light_uniform_buffer, num_lights, desc_layout, child));
		std::copy(
			std::make_move_iterator(std::begin(meshes)),
			std::make_move_iterator(std::end(meshes)),
			std::back_inserter(renderable_meshes));
	}

	for (const auto &mesh : node.meshes) {

		vcc::descriptor_pool::descriptor_pool_type desc_pool(
			vcc::descriptor_pool::create(std::ref(device), 0, 1,
			{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 } }));

		vcc::descriptor_set::descriptor_set_type desc_set(std::move(
			vcc::descriptor_set::create(std::ref(device),
				std::move(desc_pool),
				{ std::ref(desc_layout) }).front()));

		// TODO(gardell): Add transformation by iterating transformations and merging into a matrix
		const glm::mat4 transformation(1), inverse_transformation(1);

		auto transform_modelview_matrix(std::make_shared<type::transform_primitive_type<glm::mat4>>(type::make_transform(
			type::mat4(),
			[=](const auto &modelview_matrix, auto &&output) {
				output[0] = modelview_matrix[0] * transformation;
			},
			std::ref(modelview_matrix))));
		auto transform_normal_matrix(type::make_transform(
			type::mat3(),
			[=](const auto &inverse_modelview_matrix, auto &&output) {
			output[0] = glm::transpose(glm::mat3(inverse_transformation * inverse_modelview_matrix[0]));
		},
			std::ref(inverse_modelview_matrix)));

		auto projection_modelview_matrix(type::make_transform(
			type::mat4(),
			[=](const auto &projection_matrix, const auto &modelview_matrix, auto &&output) {
				output[0] = projection_matrix[0] * modelview_matrix[0];
			},
			std::ref(projection_matrix),
			transform_modelview_matrix));

		vcc::input_buffer::input_buffer_type matrix_uniform_buffer(
			vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
				std::move(projection_modelview_matrix),
				transform_modelview_matrix,
				std::move(transform_normal_matrix)));
		vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, matrix_uniform_buffer);
		vcc::descriptor_set::update(device,
			vcc::descriptor_set::write_buffer(desc_set, 0, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(std::move(matrix_uniform_buffer)) }),
			vcc::descriptor_set::write_buffer(desc_set, 1, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(std::ref(light_uniform_buffer)) }));

		renderable_meshes.push_back(std::move(renderable_mesh_type{
			*mesh,
			std::make_shared<vcc::descriptor_set::descriptor_set_type>(std::move(desc_set)),
		}));
	}

	return std::move(renderable_meshes);
}

vcc::command_buffer::command_buffer_type renderable_mesh_to_command_buffer(
	const mesh_buffers_map_type &mesh_buffers,
	vcc::device::device_type &device,
	vcc::command_pool::command_pool_type &cmd_pool,
	vcc::pipeline_layout::pipeline_layout_type &pipeline_layout,
	vcc::pipeline::pipeline_type &pipeline,
	const renderable_mesh_type &renderable_mesh) {
	const auto &mesh_buffer(mesh_buffers.at(renderable_mesh.polygon_mesh));
	vcc::command_buffer::command_buffer_type command_buffer(
		std::move(vcc::command_buffer::allocate(std::ref(device),
			std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_SECONDARY, 1).front()));
	vcc::command::compile(
		vcc::command::build(std::ref(command_buffer),
			VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			VK_FALSE, 0, 0),
		vcc::command::bind_pipeline{
			VK_PIPELINE_BIND_POINT_GRAPHICS, std::ref(pipeline)
		},
		vcc::command::bind_vertex_buffers(0, { mesh_buffer.vertex_buffer }, { 0, 0 }),
		vcc::command::bind_index_data_buffer(mesh_buffer.indices_buffer, 0, VK_INDEX_TYPE_UINT32),
		vcc::command::bind_descriptor_sets{
			VK_PIPELINE_BIND_POINT_GRAPHICS, std::ref(pipeline_layout), 0,{ renderable_mesh.desc_set }, {} },
		vcc::command::draw_indexed{ mesh_buffer.indices_count, 1, 0, 0, 0 }
	);
	return std::move(command_buffer);
}

#if defined(__ANDROID__) || defined(ANDROID)
void android_main(struct android_app* state) {
	app_dummy();
	JNIEnv* env;
	state->activity->vm->AttachCurrentThread(&env, NULL);
#else
int main(int argc, const char **argv) {
#endif // __ANDROID__

	vcc::instance::instance_type instance;
	{
		const std::set<std::string> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
			VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#else
			VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif // __ANDROID__
		};
		assert(vcc::enumerate::contains_all(
			vcc::enumerate::instance_extension_properties(""),
			extensions));
		instance = vcc::instance::create(
			{
				//"VK_LAYER_LUNARG_api_dump",
				"VK_LAYER_LUNARG_core_validation",
				"VK_LAYER_LUNARG_device_limits",
				"VK_LAYER_LUNARG_image",
				"VK_LAYER_LUNARG_object_tracker",
				"VK_LAYER_LUNARG_parameter_validation",
				//"VK_LAYER_LUNARG_screenshot",
				"VK_LAYER_LUNARG_swapchain",
				//"VK_LAYER_GOOGLE_threading",
				//"VK_LAYER_GOOGLE_unique_objects",
				//"VK_LAYER_LUNARG_vktrace",
				//"VK_LAYER_LUNARG_api_dump",
				//"VK_LAYER_LUNARG_core_validation",
				//"VK_LAYER_LUNARG_device_simulation",
				//"VK_LAYER_LUNARG_monitor",
				//"VK_LAYER_LUNARG_object_tracker",
				//"VK_LAYER_LUNARG_parameter_validation",
				//"VK_LAYER_LUNARG_screenshot",
				"VK_LAYER_LUNARG_standard_validation",
				//"VK_LAYER_GOOGLE_threading",
				"VK_LAYER_GOOGLE_unique_objects",
				//"VK_LAYER_LUNARG_vktrace",
				//"VK_LAYER_NV_optimus",
				//"VK_LAYER_RENDERDOC_Capture",
			},
			extensions);
	}

	vcc::device::device_type device;
	{
		const VkPhysicalDevice physical_device(
			vcc::physical_device::enumerate(instance).front());

		const std::set<std::string> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		assert(vcc::enumerate::contains_all(
			vcc::enumerate::device_extension_properties(physical_device, ""),
			extensions));

		device = vcc::device::create(physical_device,
			{ vcc::device::queue_create_info_type{
				vcc::physical_device::get_queue_family_properties_with_flag(
					vcc::physical_device::queue_famility_properties(physical_device),
					VK_QUEUE_GRAPHICS_BIT),
					{ 0 } }
			},
			{
				//"VK_LAYER_NV_optimus",
				//"VK_LAYER_LUNARG_api_dump",
				"VK_LAYER_LUNARG_core_validation",
				"VK_LAYER_LUNARG_device_limits",
				"VK_LAYER_LUNARG_image",
				"VK_LAYER_LUNARG_object_tracker",
				"VK_LAYER_LUNARG_parameter_validation",
				"VK_LAYER_LUNARG_standard_validation",
				"VK_LAYER_LUNARG_swapchain",
				"VK_LAYER_GOOGLE_unique_objects",
			}, extensions, {});
	}

	vcc::descriptor_set_layout::descriptor_set_layout_type desc_layout(
		vcc::descriptor_set_layout::create(std::ref(device),
		{
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
				VK_SHADER_STAGE_VERTEX_BIT,{} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 1,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
				VK_SHADER_STAGE_FRAGMENT_BIT,{} }
		}));

	vcc::pipeline_layout::pipeline_layout_type pipeline_layout(vcc::pipeline_layout::create(
		std::ref(device), { std::ref(desc_layout) }));

	vcc::descriptor_pool::descriptor_pool_type desc_pool(
		vcc::descriptor_pool::create(std::ref(device), 0, 1,
		{ { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } }));

	type::mat4 projection_matrix, modelview_matrix;

	auto inverse_projection_matrix(type::make_transform(
		type::mat4(),
		[](const auto &input, auto &&output) {
			output[0] = glm::inverse(input[0]);
		},
		std::ref(projection_matrix)));
	auto inverse_modelview_matrix(type::make_transform(
		type::mat4(),
		[](const auto &input, auto &&output) {
			output[0] = glm::inverse(input[0]);
		},
		std::ref(modelview_matrix)));

	struct light_type {
		glm::vec3 direction, color;

		VCC_STRUCT_SERIALIZABLE(direction, color);
	};
	type::t_array<light_type> lights{
		{
			glm::normalize(glm::vec3(1.f, .5f, .5f)),
			glm::vec3(1.f, 1.f, 1.f),
		},
	};
	vcc::input_buffer::input_buffer_type light_uniform_buffer(
		vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(lights)));
	vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, light_uniform_buffer);

	struct specialization_constants_type {
		int max_lights;

		VCC_STRUCT_SERIALIZABLE(
			max_lights);
		} specialization_constants{
		int(lights.size()),
	};

#if 0
	auto collada(collada_parser::parse("models/cube_UTF16LE.dae"));
#else
	auto collada(collada_parser::parse("models/duck.dae"));
#endif

	mesh_buffers_map_type mesh_buffers;
	mesh_buffers.reserve(collada.polygon_meshes.size());

	for (const auto &mesh : collada.polygon_meshes) {
		vcc::input_buffer::input_buffer_type vertex_buffer(
			vcc::input_buffer::create<type::interleaved_std140>(std::ref(device), 0,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
				type::vec3_array(std::cbegin(mesh.vertices), std::cend(mesh.vertices))));

		auto indices(triangulate_polygon_mesh(mesh));
		vcc::input_buffer::input_buffer_type index_buffer(vcc::input_buffer::create<type::linear>(
			std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, type::uint_array(std::cbegin(indices), std::cend(indices))));

		vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertex_buffer, index_buffer);

		mesh_buffers.insert(std::make_pair(
			std::cref(mesh),
			mesh_buffers_type{
				std::make_shared<vcc::input_buffer::input_buffer_type>(std::move(vertex_buffer)),
				std::make_shared<vcc::input_buffer::input_buffer_type>(std::move(index_buffer)),
				uint32_t(indices.size()),
				uint32_t(mesh.vertices.size())
			}
		));
	}

	mesh_buffers_map_type edge_mesh_buffers;
	edge_mesh_buffers.reserve(collada.polygon_meshes.size());

	for (const auto &mesh : collada.polygon_meshes) {
		const auto &mesh_buffer(mesh_buffers.at(mesh));
		const auto indices(polygon_mesh_lines(mesh));
		vcc::input_buffer::input_buffer_type index_buffer(vcc::input_buffer::create<type::linear>(
			std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, type::uint_array(std::cbegin(indices), std::cend(indices))));

		vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, index_buffer);

		edge_mesh_buffers.insert(std::make_pair(
			std::cref(mesh),
			mesh_buffers_type{
				mesh_buffer.vertex_buffer,
				std::make_shared<vcc::input_buffer::input_buffer_type>(std::move(index_buffer)),
				uint32_t(indices.size()),
				uint32_t(mesh_buffer.vertices_count),
			}
		));
	}

	mesh_buffers_map_type point_mesh_buffers;
	point_mesh_buffers.reserve(collada.polygon_meshes.size());

	for (const auto &mesh : collada.polygon_meshes) {
		const auto &buffer(mesh_buffers.at(mesh));
		std::vector<uint32_t> indices(buffer.vertices_count);
		std::iota(std::begin(indices), std::end(indices), 0);

		vcc::input_buffer::input_buffer_type index_buffer(vcc::input_buffer::create<type::linear>(
			std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, type::uint_array(std::cbegin(indices), std::cend(indices))));

		vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, index_buffer);

		point_mesh_buffers.insert(std::make_pair(
			std::cref(mesh),
			mesh_buffers_type{
				buffer.vertex_buffer,
				std::make_shared<vcc::input_buffer::input_buffer_type>(std::move(index_buffer)),
				uint32_t(indices.size()),
				uint32_t(buffer.vertices_count),
			}
		));
	}

	vcc::queue::queue_type queue(vcc::queue::get_graphics_queue(std::ref(device)));
	vcc::command_pool::command_pool_type cmd_pool(vcc::command_pool::create(
		std::ref(device), 0, vcc::queue::get_family_index(queue)));

	vcc::window::window_type window(vcc::window::create(
#ifdef WIN32
		GetModuleHandle(NULL),
#elif defined(__ANDROID__) || defined(ANDROID)
		state,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		nullptr, nullptr,
#endif // __ANDROID__
		std::ref(instance), std::ref(device), std::ref(queue),
		VkExtent2D{ 500, 500 }, VK_FORMAT_A8B8G8R8_UINT_PACK32, "Collada demo"));

	vcc::shader_module::shader_module_type vert_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-vert.spv")
#else
			std::ifstream("collada-vert.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));
	vcc::shader_module::shader_module_type geom_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-geom.spv")
#else
			std::ifstream("collada-geom.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));
	vcc::shader_module::shader_module_type frag_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-frag.spv")
#else
			std::ifstream("collada-frag.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));

	vcc::shader_module::shader_module_type edge_vert_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-edge-vert.spv")
#else
			std::ifstream("collada-edge-vert.spv", std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));
	vcc::shader_module::shader_module_type edge_frag_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-edge-frag.spv")
#else
			std::ifstream("collada-edge-frag.spv", std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));

	vcc::shader_module::shader_module_type point_vert_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-point-vert.spv")
#else
			std::ifstream("collada-point-vert.spv", std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));
	vcc::shader_module::shader_module_type point_frag_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "collada-point-frag.spv")
#else
			std::ifstream("collada-point-frag.spv", std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
		));

	vcc::pipeline_cache::pipeline_cache_type pipeline_cache(
		vcc::pipeline_cache::create(std::ref(device)));

	std::vector<vcc::command_buffer::command_buffer_type> command_buffers;
	vcc::render_pass::render_pass_type render_pass;
	vcc::pipeline::pipeline_type pipeline, edge_pipeline, vertex_pipeline;

	const float camera_scroll_delta_multiplier(.01f);
	float start_camera_distance = 6.f;
	float camera_distance = start_camera_distance;
	glm::vec2 angle(1, 0);
	glm::ivec2 start[2], current[2], mouse;
	bool is_down[2] = { false, false };
	const float scale(128);

#if !defined(__ANDROID__) && !defined(ANDROID)
	return
#endif // __ANDROID__
		vcc::window::run(window,
			[&](VkExtent2D extent, VkFormat format,
				std::vector<std::shared_ptr<vcc::image::image_type>> &&swapchain_images) {
		type::write(projection_matrix)[0] = glm::perspective(
			45.f, float(extent.width) / extent.height, 1.f, 100.f);

		render_pass = vcc::render_pass::create(
			std::ref(device),
			{
				VkAttachmentDescription{ 0, format,
					VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
					VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
				VkAttachmentDescription{ 0, VK_FORMAT_D16_UNORM,
					VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
					VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
			},
			{
				vcc::render_pass::subpass_description_type{
					{},{ { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } },{},
					{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },{}
		}
			}, {});

		pipeline = vcc::pipeline::create_graphics(
			std::ref(device), pipeline_cache, 0,
			{
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT,
					std::ref(vert_shader_module), "main"),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT,
					std::ref(frag_shader_module), "main",
					{
						VkSpecializationMapEntry{ 0, 0, sizeof(int) },
					},
					type::t_array<specialization_constants_type>{specialization_constants}),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_GEOMETRY_BIT,
					std::ref(geom_shader_module), "main")
			},
			vcc::pipeline::vertex_input_state{
				{
					VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 1, VK_VERTEX_INPUT_RATE_VERTEX }
				},
				{
					VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }
				}
			},
			vcc::pipeline::input_assembly_state{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE },
			vcc::pipeline::viewport_state(1, 1),
			vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
					VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
					VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1 },
			vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT,
				VK_FALSE, 0,{}, VK_FALSE, VK_FALSE },
			vcc::pipeline::depth_stencil_state{
				VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				0, 0 },
			vcc::pipeline::color_blend_state{ VK_FALSE, VK_LOGIC_OP_CLEAR,
				{ VkPipelineColorBlendAttachmentState{ VK_FALSE, VK_BLEND_FACTOR_ZERO,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
					VK_BLEND_OP_ADD, 0xF } },
				{ 0, 0, 0, 0 } },
			vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR } },
			std::ref(pipeline_layout), std::ref(render_pass), 0);

		edge_pipeline = vcc::pipeline::create_graphics(
			std::ref(device), pipeline_cache, 0,
			{
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT, std::ref(edge_vert_shader_module), "main"),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, std::ref(edge_frag_shader_module), "main",
					{
						VkSpecializationMapEntry{ 0, 0 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 1, 1 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 2, 2 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 3, 3 * sizeof(float), sizeof(float) },
					},
					type::vec4(glm::vec4(0, 0, 0, 1))),
			},
			vcc::pipeline::vertex_input_state{
				{ VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 1, VK_VERTEX_INPUT_RATE_VERTEX } },
				{ VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } }
			},
			vcc::pipeline::input_assembly_state{ VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE },
			vcc::pipeline::viewport_state(1, 1),
			vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
				VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_TRUE, -.1f, 0, 0, 2 },
			vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0,{}, VK_FALSE, VK_FALSE },
			vcc::pipeline::depth_stencil_state{
				VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				0, 0 },
			vcc::pipeline::color_blend_state{ VK_FALSE, VK_LOGIC_OP_CLEAR,
				{ VkPipelineColorBlendAttachmentState{ VK_FALSE, VK_BLEND_FACTOR_ZERO,
				VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
				VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
				VK_BLEND_OP_ADD, 0xF } },
				{ 0, 0, 0, 0 } },
			vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR } },
			std::ref(pipeline_layout), std::ref(render_pass), 0);

		vertex_pipeline = vcc::pipeline::create_graphics(
			std::ref(device), pipeline_cache, 0,
			{
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT, std::ref(point_vert_shader_module), "main",
					{
						VkSpecializationMapEntry{ 0, 0, sizeof(float) },
					},
					type::float_type(10.f)),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, std::ref(point_frag_shader_module), "main",
					{
						VkSpecializationMapEntry{ 0, 0 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 1, 1 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 2, 2 * sizeof(float), sizeof(float) },
						VkSpecializationMapEntry{ 3, 3 * sizeof(float), sizeof(float) },
					},
					type::vec4(glm::vec4(0, 0, 0, 1))),
			},
			vcc::pipeline::vertex_input_state{
				{ VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 1, VK_VERTEX_INPUT_RATE_VERTEX } },
				{ VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } }
			},
			vcc::pipeline::input_assembly_state{ VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE },
			vcc::pipeline::viewport_state(1, 1),
			vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
				VK_POLYGON_MODE_POINT, VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_TRUE, .1f, 0, 0, 1 },
			vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0,{}, VK_FALSE, VK_FALSE },
			vcc::pipeline::depth_stencil_state{
				VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 },
				0, 0 },
			vcc::pipeline::color_blend_state{ VK_FALSE, VK_LOGIC_OP_CLEAR,
				{ VkPipelineColorBlendAttachmentState{ VK_FALSE, VK_BLEND_FACTOR_ZERO,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
					VK_BLEND_OP_ADD, 0xF } },
				{ 0, 0, 0, 0 } },
			vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR } },
			std::ref(pipeline_layout), std::ref(render_pass), 0);

		const auto renderable_meshes(mesh(
			device,
			projection_matrix,
			modelview_matrix,
			inverse_modelview_matrix,
			light_uniform_buffer,
			lights.size(),
			desc_layout,
			*collada.scene));
		std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>> sub_command_buffers;
		sub_command_buffers.reserve(renderable_meshes.size() * 3);
		
		std::transform(std::cbegin(renderable_meshes), std::cend(renderable_meshes), std::back_inserter(sub_command_buffers),
			std::bind(
				renderable_mesh_to_command_buffer,
				std::cref(mesh_buffers),
				std::ref(device),
				std::ref(cmd_pool),
				std::ref(pipeline_layout),
				std::ref(pipeline),
				std::placeholders::_1));
		std::transform(std::cbegin(renderable_meshes), std::cend(renderable_meshes), std::back_inserter(sub_command_buffers),
			std::bind(
				renderable_mesh_to_command_buffer,
				std::cref(edge_mesh_buffers),
				std::ref(device),
				std::ref(cmd_pool),
				std::ref(pipeline_layout),
				std::ref(edge_pipeline),
				std::placeholders::_1));
		std::transform(std::cbegin(renderable_meshes), std::cend(renderable_meshes), std::back_inserter(sub_command_buffers),
			std::bind(
				renderable_mesh_to_command_buffer,
				std::cref(point_mesh_buffers),
				std::ref(device),
				std::ref(cmd_pool),
				std::ref(pipeline_layout),
				std::ref(vertex_pipeline),
				std::placeholders::_1));

		command_buffers = vcc::command_buffer::allocate(std::ref(device),
			std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			(uint32_t)swapchain_images.size());

		auto depth_image(std::make_shared<vcc::image::image_type>(
			vcc::image::create(
				std::ref(device), 0, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM,
				{ extent.width, extent.height, 1 }, 1, 1,
				VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_SHARING_MODE_EXCLUSIVE, {}, VK_IMAGE_LAYOUT_UNDEFINED)));
		vcc::memory::bind(std::ref(device),
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *depth_image);

		vcc::command_buffer::command_buffer_type command_buffer(
			std::move(vcc::command_buffer::allocate(std::ref(device),
				std::ref(cmd_pool), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1).front()));
		vcc::command::compile(vcc::command::build(std::ref(command_buffer), 0, VK_FALSE, 0, 0),
			vcc::command::pipeline_barrier(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, {}, {},
				{ vcc::command::image_memory_barrier{ 0,
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				depth_image,{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } } }));

		vcc::queue::submit(queue, {}, { command_buffer }, {});
		vcc::queue::wait_idle(queue);

		const vcc::queue::queue_type &present_queue(vcc::window::get_present_queue(window));

		for (std::size_t i = 0; i < swapchain_images.size(); ++i) {
			auto framebuffer(vcc::framebuffer::create(std::ref(device), std::ref(render_pass),
			{
				vcc::image_view::create(swapchain_images[i],
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }),
				vcc::image_view::create(depth_image,
				{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 })
			}, extent, 1));
			vcc::command::compile(
				vcc::command::build(std::ref(command_buffers[i]), 0, VK_FALSE, 0, 0),
				vcc::command::pipeline_barrier(
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, {}, {},
					{
						vcc::command::image_memory_barrier{ 0,
							VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
							| VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							vcc::queue::get_family_index(present_queue),
							vcc::queue::get_family_index(queue),
							swapchain_images[i],
							{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
						}
					}),
				vcc::command::set_viewport{
					0,{ { 0.f, 0.f, float(extent.width),
				float(extent.height), 0.f, 1.f } } },
				vcc::command::set_scissor{
					0,{ { { 0, 0 }, extent } } },
				vcc::command::render_pass(std::ref(render_pass),
					std::move(framebuffer), VkRect2D{ { 0, 0 }, extent },
					{
						vcc::command::clear_color({ { .2f, .2f, .2f, .2f } }),
						vcc::command::clear_depth_stencil({ 1, 0 })
					},
					VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS,
					vcc::command::execute_commands{ sub_command_buffers }),
				vcc::command::pipeline_barrier(
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, {}, {},
					{
						vcc::command::image_memory_barrier{
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				0,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				vcc::queue::get_family_index(queue),
				vcc::queue::get_family_index(present_queue),
				swapchain_images[i],
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } }
					}));
		}
	},
			[]() {},
		[&](uint32_t index, const vcc::semaphore::semaphore_type &wait_semaphore,
			const vcc::semaphore::semaphore_type &signal_semaphore) {
		glm::mat4 view_matrix(glm::lookAt(glm::vec3(0, 0, camera_distance),
			glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
		type::write(modelview_matrix)[0] = view_matrix
			* glm::rotate(angle.y, glm::vec3(1, 0, 0))
			* glm::rotate(angle.x, glm::vec3(0, 1, 0));
		vcc::queue::submit(queue,
		{
			vcc::queue::wait_semaphore{ std::ref(wait_semaphore),
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
		}, { command_buffers[index] }, { signal_semaphore });
	},
		vcc::window::input_callbacks_type()
		.set_mouse_down_callback([&](
			vcc::window::mouse_button_type button, int x, int y) {
		mouse = glm::ivec2(x, y);
		if (button >= 0 && button < 2) {
			is_down[button] = true;
		}
		return true;
	}).set_mouse_up_callback([&](
		vcc::window::mouse_button_type button, int x, int y) {
		if (button >= 0 && button < 2) {
			is_down[button] = false;
		}
		return true;
	}).set_mouse_move_callback([&](int x, int y) {
		if (is_down[0]) {
			angle += (glm::vec2(x, y) - glm::vec2(mouse)) / scale;
			mouse = glm::ivec2(x, y);
		}
		return true;
	}).set_mouse_scroll_callback([&](int delta) {
		camera_distance += delta * camera_scroll_delta_multiplier;
		return true;
	}).set_touch_down_callback([&](int id, int x, int y) {
		if (id >= 0 && id < 2) {
			start[id] = glm::ivec2(x, y);
			current[id] = start[id];
			is_down[id] = true;
		}
		return true;
	}).set_touch_up_callback([&](int id, int x, int y) {
		is_down[0] = is_down[1] = false;
		start_camera_distance = camera_distance;
		return true;
	}).set_touch_move_callback([&](int id, int x, int y) {
		if (id == 0) {
			angle += (glm::vec2(x, y) - glm::vec2(current[0])) / scale;
		}
		if (id >= 0 && id < 2) {
			current[id] = glm::ivec2(x, y);
			if (!is_down[id]) {
				start[id] = current[id];
				is_down[id] = true;
			}
		}
		if (is_down[1]) {
			const float start_distance(glm::length(
				glm::vec2(start[0] - start[1])));
			const float current_distance(glm::length(
				glm::vec2(current[0] - current[1])));
			camera_distance = start_camera_distance * start_distance
				/ current_distance;
		}
		return true;
	}));
}
