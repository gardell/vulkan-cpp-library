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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <type/types.h>
#include <vcc/android_asset_istream.h>
#include <vcc/buffer.h>
#include <vcc/command.h>
#include <vcc/command_buffer.h>
#include <vcc/command_pool.h>
#include <vcc/debug.h>
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

namespace teapot {

#include "teapot.h"

}  // namespace teapot

const bool validate = true;

#if defined(__ANDROID__) || defined(ANDROID)
void android_main(struct android_app* state) {
	app_dummy();
	JNIEnv* env;
	state->activity->vm->AttachCurrentThread(&env, NULL);
#else
int main(int argc, const char **argv) {
#endif // __ANDROID__

	vcc::instance::instance_type instance;
	vcc::debug::debug_type debug;

	{
		std::set<std::string> instance_validation_layers;
		if (validate) {
			std::set<std::string> required_instance_validation_layers{
				//"VK_LAYER_LUNARG_standard_validation"
			};
			assert(vcc::enumerate::contains_all(vcc::enumerate::instance_layer_properties(),
				required_instance_validation_layers));
			instance_validation_layers = required_instance_validation_layers;
		}

		const std::set<std::string> required_extensions = { VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef WIN32
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
			VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#else
			VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
		};
		const std::vector<VkExtensionProperties> instance_extensions(
			vcc::enumerate::instance_extension_properties(""));
		assert(vcc::enumerate::contains_all(instance_extensions, required_extensions));
		const std::vector<std::string> optional(vcc::enumerate::filter(instance_extensions,
		  { VK_EXT_DEBUG_REPORT_EXTENSION_NAME }));
		std::set<std::string> extensions(required_extensions.begin(), required_extensions.end());
		extensions.insert(optional.begin(), optional.end());

		instance = vcc::instance::create(instance_validation_layers, extensions);
	}

	vcc::device::device_type device;
	{
		const std::vector<VkPhysicalDevice> physical_devices(vcc::physical_device::enumerate(instance));
		const VkPhysicalDevice physical_device = physical_devices.front();

		std::set<std::string> device_validation_layers;
		if (validate) {
			const std::set<std::string> required_validation_layers{
				//"VK_LAYER_LUNARG_standard_validation"
			};
			assert(vcc::enumerate::contains_all(vcc::enumerate::device_layer_properties(physical_device),
				required_validation_layers));
			device_validation_layers = required_validation_layers;
		}

		const std::set<std::string> dev_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		const std::vector<VkExtensionProperties> device_extensions(
			vcc::enumerate::device_extension_properties(physical_device, ""));
		assert(vcc::enumerate::contains_all(device_extensions, dev_extensions));

		if (validate) {
			try {
				debug = vcc::debug::create(std::ref(instance),
					VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
					| VK_DEBUG_REPORT_INFORMATION_BIT_EXT
					| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
					| VK_DEBUG_REPORT_DEBUG_BIT_EXT);
			} catch (const vcc::vcc_exception &) {
				VCC_PRINT("Failed to load debug extension.");
			}
		}

		device = vcc::device::create(physical_device,
			{ vcc::device::queue_create_info_type{
				vcc::physical_device::get_queue_family_properties_with_flag(
					vcc::physical_device::queue_famility_properties(physical_device), VK_QUEUE_GRAPHICS_BIT),
				{ 0 } }
			}, device_validation_layers, dev_extensions, {});
	}

	vcc::queue::queue_type queue(vcc::queue::get_graphics_queue(std::ref(device)));

	vcc::descriptor_set_layout::descriptor_set_layout_type desc_layout(
		vcc::descriptor_set_layout::create(std::ref(device),
		{
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, {} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 2,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, {} }
		}));

	const int num_push_colors = 5;
	vcc::pipeline_layout::pipeline_layout_type pipeline_layout(
		vcc::pipeline_layout::create<type::memory_layout::linear_std430>(std::ref(device),
			{ std::ref(desc_layout) },
			{ VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4) * num_push_colors} },
			type::vec4_array({
				glm::vec4(1.f, 0.f, 0.f, 1.f), glm::vec4(1.f, 1.f, 0.f, 1.f),
				glm::vec4(0.f, 1.f, 1.f, 1.f), glm::vec4(0.f, 0.f, 1.f, 1.f),
				glm::vec4(1.f, 1.f, 1.f, 1.f) })));

	vcc::pipeline_cache::pipeline_cache_type pipeline_cache(
		vcc::pipeline_cache::create(std::ref(device)));

	vcc::shader_module::shader_module_type vert_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "teapot-vert.spv")
#else
			std::ifstream("teapot-vert.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));
	vcc::shader_module::shader_module_type frag_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "teapot-frag.spv")
#else
			std::ifstream("teapot-frag.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));

	vcc::descriptor_pool::descriptor_pool_type desc_pool(
		vcc::descriptor_pool::create(std::ref(device), 0, 1, {
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } }));
	vcc::descriptor_set::descriptor_set_type desc_set(std::move(
		vcc::descriptor_set::create(std::ref(device), std::ref(desc_pool),
			{ std::ref(desc_layout) }).front()));

	const glm::mat4 view_matrix(glm::lookAt(glm::vec3(0.0f, 30.0f, 30.0f),
		glm::vec3(0, 0, 0), glm::vec3(0.0f, 1.0f, 0.0)));

	const uint32_t num_instanced_drawings = 256;
	type::mat4 projection_matrix;
	type::mat4_array modelview_matrix_array(num_instanced_drawings);
	type::mat3_array normal_matrix_array(num_instanced_drawings);

	struct {
		type::vec4 position;
		type::vec3 attenuation;
		type::vec3 spot_direction;
		type::float_type spot_cos_cutoff;
		type::vec4 ambient;
		type::vec4 diffuse;
		type::vec4 specular;
		type::float_type spot_exponent;
	} light{
		type::vec4(glm::vec4(0.f, 10.f, 0.f, 1.f)),
		type::vec3(glm::vec3(1, 0, 0)), type::vec3(glm::vec3(0, 0, -1)),
		type::float_type(-1), type::vec4(glm::vec4(0.2f, 0.2f, 0.2f, 1)),
		type::vec4(glm::vec4(0.2f, 0.2f, 0.2f, 1.f)),
		type::vec4(glm::vec4(1.f, 1.f, 1.f, 1.f)), type::float_type(120.f)
	};

	vcc::input_buffer::input_buffer_type matrix_uniform_buffer(
		vcc::input_buffer::create<type::linear>(std::ref(device), 0,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(projection_matrix)));

	vcc::input_buffer::input_buffer_type modelview_matrix_uniform_buffer(
		vcc::input_buffer::create<type::interleaved_std140>(std::ref(device), 0,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(modelview_matrix_array), std::ref(normal_matrix_array)));
	vcc::input_buffer::input_buffer_type light_uniform_buffer(
		vcc::input_buffer::create<type::linear_std140>(std::ref(device), 0,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(light.position), std::ref(light.attenuation),
			std::ref(light.spot_direction), std::ref(light.spot_cos_cutoff),
			std::ref(light.ambient), std::ref(light.diffuse),
			std::ref(light.specular), std::ref(light.spot_exponent)));

	vcc::input_buffer::input_buffer_type vertex_buffer(
		vcc::input_buffer::create<type::interleaved_std140>(std::ref(device), 0,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(teapot::vertices), std::ref(teapot::texcoords), std::ref(teapot::normals)));
	vcc::input_buffer::input_buffer_type index_buffer(vcc::input_buffer::create<type::linear>(
		std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
		std::ref(teapot::indices)));
	vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, light_uniform_buffer,
		index_buffer, vertex_buffer, matrix_uniform_buffer, modelview_matrix_uniform_buffer);
	const VkFormat depth_format = VK_FORMAT_D16_UNORM;

	vcc::sampler::sampler_type sampler(vcc::sampler::create(
		std::ref(device), VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0, VK_TRUE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE));

	vcc::image::image_type image(vcc::image::create(std::ref(queue),
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "storforsen_etc2_rgb.ktx"),
#else
			std::ifstream("textures/storforsen4/storforsen4.ktx",
				std::ios_base::binary | std::ios_base::in),
#endif  // __ ANDROID__
			true
		));
	const VkFormat image_format(vcc::image::get_format(image));

	vcc::descriptor_set::update(device,
		vcc::descriptor_set::write_buffer(desc_set, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			{ vcc::descriptor_set::buffer_info(std::ref(matrix_uniform_buffer)) }),
		vcc::descriptor_set::write_buffer(desc_set, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			{ vcc::descriptor_set::buffer_info(std::ref(light_uniform_buffer)) }),
		vcc::descriptor_set::write_image{ desc_set, 1, 0,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			{
				vcc::descriptor_set::image_info{
					std::ref(sampler),
					vcc::image_view::create(std::move(image),
						VK_IMAGE_VIEW_TYPE_CUBE, image_format,
						{
							VK_COMPONENT_SWIZZLE_IDENTITY,
							VK_COMPONENT_SWIZZLE_IDENTITY,
							VK_COMPONENT_SWIZZLE_IDENTITY,
							VK_COMPONENT_SWIZZLE_IDENTITY
						},
						{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 }),
					VK_IMAGE_LAYOUT_GENERAL
				}
			} });

	vcc::window::window_type window(vcc::window::create(
#ifdef WIN32
		GetModuleHandle(NULL),
#elif defined(__ANDROID__) || defined(ANDROID)
		state,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		nullptr, nullptr,
#endif // __ANDROID__
		std::ref(instance), std::ref(device), std::ref(queue), VkExtent2D{ 500, 500 },
		VK_FORMAT_A8B8G8R8_UINT_PACK32, "Teapot demo"));

	vcc::command_pool::command_pool_type cmd_pool(vcc::command_pool::create(
		std::ref(device), 0, vcc::queue::get_family_index(queue)));
	std::vector<vcc::command_buffer::command_buffer_type> command_buffers;
	vcc::render_pass::render_pass_type render_pass;
	vcc::pipeline::pipeline_type pipeline;

	const float camera_scroll_delta_multiplier(.01f);
	float start_camera_distance = 40.f;
	float camera_distance = start_camera_distance;
	glm::vec2 angle(0, .5f);
	glm::ivec2 start[2], current[2], mouse;
	bool is_down[2] = {false, false};
	const float scale(128);

#if !defined(__ANDROID__) && !defined(ANDROID)
	return
#endif // __ANDROID__
	vcc::window::run(window,
		[&](VkExtent2D extent, VkFormat format,
			std::vector<std::shared_ptr<vcc::image::image_type>> &&swapchain_images) {
		type::write(projection_matrix)[0] = glm::perspective(
			45.f, float(extent.width) / extent.height, 1.f, 100.f);
		command_buffers.clear();

		render_pass = vcc::render_pass::create(std::ref(device),
			{
				VkAttachmentDescription{ 0, format,
					VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR,
					VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
				VkAttachmentDescription{ 0, depth_format, VK_SAMPLE_COUNT_1_BIT,
					VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
			},
			{
				vcc::render_pass::subpass_description_type{
					{},
					{ VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } },
					{},
					VkAttachmentReference{ 1,
						VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL },
					{}
				}
			},
			{});
		pipeline = vcc::pipeline::create_graphics(std::ref(device), pipeline_cache, 0,
			{
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT, std::ref(vert_shader_module),
					"main", {}),
				vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, std::ref(frag_shader_module),
					"main",
					{ VkSpecializationMapEntry{ 2, 0, sizeof(int) } },
					type::int_type(num_push_colors))
			},
			vcc::pipeline::vertex_input_state{
				{
					VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 3,
						VK_VERTEX_INPUT_RATE_VERTEX },
					VkVertexInputBindingDescription{ 1, sizeof(glm::mat4) + sizeof(glm::vec4) * 3,
						VK_VERTEX_INPUT_RATE_INSTANCE }
				},
				{
					VkVertexInputAttributeDescription{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
					VkVertexInputAttributeDescription{ 1, 0, VK_FORMAT_R32G32_SFLOAT,
						sizeof(glm::vec4) },
					VkVertexInputAttributeDescription{ 2, 0, VK_FORMAT_R32G32B32_SFLOAT,
						sizeof(glm::vec4) * 2 },

					VkVertexInputAttributeDescription{ 3, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 0 },
					VkVertexInputAttributeDescription{ 4, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 1 },
					VkVertexInputAttributeDescription{ 5, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 2 },
					VkVertexInputAttributeDescription{ 6, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 3 },

					VkVertexInputAttributeDescription{ 7, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 4 },
					VkVertexInputAttributeDescription{ 8, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 5 },
					VkVertexInputAttributeDescription{ 9, 1,
						VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 6 }
				}
			},
			vcc::pipeline::input_assembly_state{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE },
			vcc::pipeline::viewport_state({ VkViewport{ 0, 0, 0, 0, 0, 0 } },
				{ VkRect2D{ VkOffset2D{}, VkExtent2D{} } }),
			vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL,
				VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1 },
			vcc::pipeline::multisample_state{ VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, {}, VK_FALSE,
				VK_FALSE },
			vcc::pipeline::depth_stencil_state{
				VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE,
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS,
					0, 0, 0 },
				{ VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS,
					0, 0, 0 },
				0, 0 },
			vcc::pipeline::color_blend_state{ VK_FALSE, VK_LOGIC_OP_CLEAR,
				{ VkPipelineColorBlendAttachmentState{ VK_FALSE,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF } },
				{ 0, 0, 0, 0 } },
			vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR } },
			std::ref(pipeline_layout), std::ref(render_pass), 0);

		std::shared_ptr<vcc::image::image_type> depth_image(
			std::make_shared<vcc::image::image_type>(vcc::image::create(
				std::ref(device), 0, VK_IMAGE_TYPE_2D, depth_format,
				{ extent.width, extent.height, 1 }, 1, 1,
				VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_SHARING_MODE_EXCLUSIVE, {}, VK_IMAGE_LAYOUT_UNDEFINED)));
		vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, *depth_image);

		vcc::command_buffer::command_buffer_type command_buffer(std::move(
			vcc::command_buffer::allocate(std::ref(device), std::ref(cmd_pool),
				VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1).front()));
		vcc::command::compile(vcc::command::build(std::ref(command_buffer), 0, VK_FALSE, 0, 0),
			vcc::command::pipeline_barrier(
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, {}, {},
				{ vcc::command::image_memory_barrier{ 0,
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					depth_image, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } } }));
		vcc::fence::fence_type fence(vcc::fence::create(std::ref(device)));
		vcc::queue::submit(queue, {}, { command_buffer }, {}, fence);
		vcc::fence::wait(device, { fence }, true);

		const vcc::queue::queue_type &present_queue(vcc::window::get_present_queue(window));

		command_buffers = vcc::command_buffer::allocate(std::ref(device), std::ref(cmd_pool),
			VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t) swapchain_images.size());
		for (std::size_t i = 0; i < swapchain_images.size(); ++i) {
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
				vcc::command::render_pass(std::ref(render_pass),
					vcc::framebuffer::create(std::ref(device), std::ref(render_pass),
						{
							vcc::image_view::create(swapchain_images[i],
								{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }),
							vcc::image_view::create(depth_image,
								{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 })
						}, extent, 1),
					VkRect2D{ 0, 0, extent.width, extent.height },
					{
						vcc::command::clear_color(VkClearColorValue{ { .2f, .2f, .2f, .2f } }),
						vcc::command::clear_depth_stencil(VkClearDepthStencilValue{ 1, 0 })
					},
					VK_SUBPASS_CONTENTS_INLINE,
					vcc::command::bind_pipeline{ VK_PIPELINE_BIND_POINT_GRAPHICS, std::ref(pipeline) },
					vcc::command::bind_vertex_buffers(0, { std::ref(vertex_buffer),
						std::ref(modelview_matrix_uniform_buffer) }, { 0, 0 }),
					vcc::command::bind_index_data_buffer(std::ref(index_buffer), 0, VK_INDEX_TYPE_UINT16),
					vcc::command::bind_descriptor_sets{ VK_PIPELINE_BIND_POINT_GRAPHICS,
						std::ref(pipeline_layout), 0, { std::ref(desc_set) }, {} },
					vcc::command::set_viewport{ 0,
						{ VkViewport{ 0.f, 0.f, float(extent.width), float(extent.height), 0.f, 1.f } } },
					vcc::command::set_scissor{ 0, { VkRect2D{ VkOffset2D{ 0, 0 }, extent } } },
					vcc::command::draw_indexed{ (uint32_t) teapot::indices.size(),
						num_instanced_drawings, 0, 0, 0 }),
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
			{
				const glm::mat4 view_matrix(glm::lookAt(glm::vec3(0, 0, camera_distance),
					glm::vec3(0, 0, 0), glm::vec3(0, 1, 0))
					* glm::rotate(angle.y, glm::vec3(1, 0, 0))
					* glm::rotate(angle.x, glm::vec3(0, 1, 0)));

				auto modelview_matrix(type::write(modelview_matrix_array));
				auto normal_matrix(type::write(normal_matrix_array));

				for (std::size_t i = 0; i < num_instanced_drawings; ++i) {
					const int num_per_row = (int)sqrt(num_instanced_drawings);
					const int x = int(i) % num_per_row;
					const int y = int(i) / num_per_row;
					const glm::mat4 model_view(view_matrix
						* glm::translate(glm::vec3(6 * (int(x) - int(num_per_row) / 2), 0,
							6 * (int(y) - int(num_instanced_drawings / num_per_row) / 2))));
					modelview_matrix[int(i)] = model_view;
					normal_matrix[int(i)] =
						glm::mat3(glm::transpose(glm::inverse(model_view)));
				}
			}
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
