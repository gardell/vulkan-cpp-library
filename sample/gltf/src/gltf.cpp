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
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <gltf_to_vulkan.h>
#include <gltf/parser.h>
#include <type/transform.h>
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

#ifdef _MSC_VER
	namespace fs = std::experimental::filesystem::v1;
#else
	namespace fs = std::filesystem;
#endif

#if defined(__ANDROID__) || defined(ANDROID)
void android_main(struct android_app* state) {
	app_dummy();
	JNIEnv* env;
	state->activity->vm->AttachCurrentThread(&env, NULL);
#else
int main(int argc, const char **argv) {
	assert(argc == 2);
	const std::string_view path(argv[1]);
	const fs::path wd(fs::path(std::string(path)).parent_path());
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
		const auto instance_layer_properties(vcc::enumerate::instance_layer_properties());
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

	vcc::debug::debug_type debug;

	try {
		debug = vcc::debug::create(std::ref(instance),
			VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_INFORMATION_BIT_EXT
			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_DEBUG_BIT_EXT);
	}
	catch (const vcc::vcc_exception &) {
		VCC_PRINT("Failed to load debug extension.");
	}

	vcc::device::device_type device;
	{
		const VkPhysicalDevice physical_device(
			vcc::physical_device::enumerate(instance).front());

		const auto device_layer_properties(vcc::enumerate::device_layer_properties(physical_device));

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

	vcc::queue::queue_type queue(vcc::queue::get_graphics_queue(std::ref(device)));

	const auto gltf_format(gltf::parse_format(std::ifstream(path.data(), std::ios::binary | std::ios::in)));
	const auto model(gltf::parse(gltf_format));
	const auto buffers(gltf_to_vulkan::buffers(wd, device, gltf_format, model.buffers));
	const auto shaders(gltf_to_vulkan::shaders(std::ref(device)));
	const auto materials(gltf_to_vulkan::materials_from_gltf(device, std::ref(queue), wd, gltf_format, model.materials));

	vcc::window::window_type window(vcc::window::create(
#ifdef WIN32
		GetModuleHandle(NULL),
#elif defined(__ANDROID__) || defined(ANDROID)
		state,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		nullptr, nullptr,
#endif // __ANDROID__
		std::ref(instance), std::ref(device), std::ref(queue),
		VkExtent2D{ 500, 500 }, VK_FORMAT_A8B8G8R8_UINT_PACK32, "glTF demo"));

	vcc::pipeline_cache::pipeline_cache_type pipeline_cache(
		vcc::pipeline_cache::create(std::ref(device)));

	vcc::command_pool::command_pool_type cmd_pool(vcc::command_pool::create(
		std::ref(device), 0, vcc::queue::get_family_index(queue)));
	std::vector<vcc::command_buffer::command_buffer_type> command_buffers;
	vcc::render_pass::render_pass_type render_pass;

	const float camera_scroll_delta_multiplier(.005f);
	float start_camera_distance = 6.f;
	float camera_distance = start_camera_distance;
	glm::vec2 angle(1, 0);
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
				45.f, float(extent.width) / extent.height, .01f, 100.f);

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
						{}, { { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } }, {},
						{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }, {}
					}
				}, {});

			std::vector<type::supplier<const vcc::command_buffer::command_buffer_type>> sub_command_buffers(gltf_to_vulkan::scene(
				wd,
				device,
				std::ref(queue),
				render_pass,
				pipeline_cache,
				cmd_pool,
				gltf_format,
				buffers,
				materials,
				std::ref(light_uniform_buffer),
				lights.size(),
				shaders,
				**model.scene,
				std::ref(projection_matrix),
				std::ref(modelview_matrix),
				std::ref(inverse_projection_matrix),
				std::ref(inverse_modelview_matrix)));

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
					depth_image, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } } }));

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
					vcc::command::set_viewport{ 0, { { 0.f, 0.f, float(extent.width), float(extent.height), 0.f, 1.f } } },
					vcc::command::set_scissor{ 0,{ { { 0, 0 }, extent } } },
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
