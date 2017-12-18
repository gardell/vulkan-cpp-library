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
#define NOMINMAX
#include <algorithm>
#include <cassert>
#include <codecvt>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <type/types.h>
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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

type::vec3_array vertices({
	glm::vec3(-1, -.5f, 1),
	glm::vec3(-1, -.5f, -1),
	glm::vec3(1, -.5f, -1),
	glm::vec3(1, -.5f, 1),
	glm::vec3(-1, .5f, 1),
	glm::vec3(-1, -.5f, 1),
	glm::vec3(1, -.5f, 1),
	glm::vec3(1, .5f, 1),
	glm::vec3(-1, .5f, -1),
	glm::vec3(-1, .5f, 1),
	glm::vec3(1, .5f, 1),
	glm::vec3(1, .5f, -1),
	glm::vec3(1, .5f, -1),
	glm::vec3(1, -.5f, -1),
	glm::vec3(-1, -.5f, -1),
	glm::vec3(-1, .5f, -1),
	glm::vec3(1, .5f, 1),
	glm::vec3(1, -.5f, 1),
	glm::vec3(1, -.5f, -1),
	glm::vec3(1, .5f, -1),
	glm::vec3(-1, .5f, -1),
	glm::vec3(-1, -.5f, -1),
	glm::vec3(-1, -.5f, 1),
	glm::vec3(-1, .5f, 1)
});

type::vec2_array texcoords({
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0),
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0),
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0),
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0),
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0),
	glm::vec2(0, 0),
	glm::vec2(0, 1),
	glm::vec2(1, 1),
	glm::vec2(1, 0)
});

type::ushort_array indices({
	0, 1, 2, 0, 2, 3,
	4, 5, 6, 4, 6, 7,
	8, 9, 10, 8, 10, 11,
	12, 13, 14, 12, 14, 15,
	16, 17, 18, 16, 18, 19,
	20, 21, 22, 20, 22, 23
});

#ifdef _MSC_VER
typedef unsigned int u32char_type;
#else
typedef char32_t u32char_type;
#endif

typedef std::basic_string<u32char_type> u32string_type;

u32string_type utf8_to_utf32(const std::string &string) {
	std::wstring_convert<std::codecvt_utf8<u32char_type>, unsigned int> convert;
	return convert.from_bytes(string);
}

struct measurement_type {
	VkExtent2D extent;
	int32_t baseline;
};

measurement_type measure_string(FT_Face face, const u32string_type &string) {
	uint32_t x(0);
	int32_t y_max(std::numeric_limits<int32_t>::min()), y_min(std::numeric_limits<int32_t>::max());

	FT_UInt previous_glyph_index(0);

	for (const auto character : string) {
		const auto glyph_index = FT_Get_Char_Index(face, character);

		if (glyph_index && previous_glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(
				face,
				previous_glyph_index,
				glyph_index,
				FT_KERNING_DEFAULT,
				&delta);

			x += delta.x;
		}

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) {
			throw std::invalid_argument("FT_Load_Glyph");
		}

		FT_Glyph glyph;
		if (FT_Get_Glyph(face->glyph, &glyph)) {
			throw std::invalid_argument("FT_Get_Glyph");
		}

		FT_BBox bbox;
		FT_Glyph_Get_CBox(glyph, 0, &bbox);

		x += face->glyph->advance.x;
		y_max = std::max(y_max, int32_t(bbox.yMax));
		y_min = std::min(y_min, int32_t(bbox.yMin));
		previous_glyph_index = glyph_index;
	}

	return measurement_type{
		VkExtent2D{ x >> 6, uint32_t(y_max - y_min) >> 6 },
		y_max >> 6,
	};
}

vcc::image::image_type draw_string(
	const type::supplier<vcc::device::device_type> &device,
	FT_Face face, const u32string_type &string, VkImageUsageFlags usage, VkSharingMode sharing_mode,
	const std::vector<uint32_t> &queue_family_indices) {
	glm::uvec2 dim(0, 0);
	FT_UInt previous_glyph_index(0);

	const auto measurement(measure_string(face, string));

	std::string buffer(measurement.extent.width * measurement.extent.height, 0);

	for (const auto character : string) {
		const auto glyph_index = FT_Get_Char_Index(face, character);

		if (glyph_index && previous_glyph_index) {
			FT_Vector delta;

			FT_Get_Kerning(
				face,
				previous_glyph_index,
				glyph_index,
				FT_KERNING_DEFAULT,
				&delta);

			dim.x += delta.x >> 6;
		}

		if (FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER)) {
			throw std::invalid_argument("FT_Load_Glyph");
		}

		for (int y = 0; y < face->glyph->bitmap.rows; ++y) {
			for (int x = 0; x < face->glyph->bitmap.width; ++x) {
				buffer[(dim.y + measurement.baseline - face->glyph->bitmap_top + y) * measurement.extent.width + dim.x + x] = face->glyph->bitmap.buffer[y * face->glyph->bitmap.pitch + x];
			}
		}

		dim.x += face->glyph->advance.x >> 6;
		previous_glyph_index = glyph_index;
	}

	const auto format(VK_FORMAT_R8_UNORM);
	auto image(vcc::image::create(
		device, 0, VK_IMAGE_TYPE_2D, format, { measurement.extent.width, measurement.extent.height, 1 },
		1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_LINEAR, usage, sharing_mode, queue_family_indices,
		VK_IMAGE_LAYOUT_PREINITIALIZED));
	vcc::memory::bind(device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, image);
	vcc::image::copy_to_linear_image(
		format,
		VK_IMAGE_ASPECT_COLOR_BIT,
		{ measurement.extent.width, measurement.extent.height },
		buffer.data(),
		1,
		measurement.extent.width,
		image);

	return std::move(image);
}

#if defined(__ANDROID__) || defined(ANDROID)
void android_main(struct android_app* state) {
	app_dummy();
	JNIEnv* env;
	state->activity->vm->AttachCurrentThread(&env, NULL);
#else
int main(int argc, const char **argv) {
#endif // __ANDROID__

	FT_Library library;
	if (FT_Init_FreeType(&library)) {
		throw std::runtime_error("FT_Init_FreeType");
	}

	FT_Face face;
	if (FT_New_Face(library,
		"fonts/Roboto-Regular.ttf",
		0,
		&face)) {
		throw std::invalid_argument("FT_New_Face");
	}

	FT_Set_Pixel_Sizes(face, 0, 64);

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
		instance = vcc::instance::create({
			"VK_LAYER_LUNARG_core_validation",
			"VK_LAYER_LUNARG_device_limits",
			"VK_LAYER_LUNARG_image",
			"VK_LAYER_LUNARG_object_tracker",
			"VK_LAYER_LUNARG_parameter_validation",
			"VK_LAYER_LUNARG_swapchain",
			"VK_LAYER_LUNARG_standard_validation",
		}, extensions);
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
			{}, extensions, {});
	}

	vcc::descriptor_set_layout::descriptor_set_layout_type desc_layout(
		vcc::descriptor_set_layout::create(std::ref(device),
		{
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
				VK_SHADER_STAGE_VERTEX_BIT,{} },
			vcc::descriptor_set_layout::descriptor_set_layout_binding{ 1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
				VK_SHADER_STAGE_FRAGMENT_BIT,{} }
		}));

	vcc::pipeline_layout::pipeline_layout_type pipeline_layout(vcc::pipeline_layout::create(
		std::ref(device), { std::ref(desc_layout) }));

	vcc::descriptor_pool::descriptor_pool_type desc_pool(
		vcc::descriptor_pool::create(std::ref(device), 0, 1,
		{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } }));

	vcc::descriptor_set::descriptor_set_type desc_set(std::move(
		vcc::descriptor_set::create(std::ref(device),
			std::ref(desc_pool),
			{ std::ref(desc_layout) }).front()));

	glm::mat4 projection_matrix;
	type::mat4 projection_modelview_matrix;
	vcc::input_buffer::input_buffer_type matrix_uniform_buffer(
		vcc::input_buffer::create<type::linear>(std::ref(device), 0,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(projection_modelview_matrix)));

	vcc::input_buffer::input_buffer_type vertex_buffer(
		vcc::input_buffer::create<type::interleaved_std140>(std::ref(device), 0,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, {},
			std::ref(vertices), std::ref(texcoords)));

	vcc::input_buffer::input_buffer_type index_buffer(vcc::input_buffer::create<type::linear>(
		std::ref(device), 0, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_SHARING_MODE_EXCLUSIVE, {}, std::ref(indices)));

	vcc::memory::bind(std::ref(device), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		vertex_buffer, index_buffer, matrix_uniform_buffer);

	vcc::queue::queue_type queue(vcc::queue::get_graphics_queue(
		std::ref(device)));
	{
#if defined(__ANDROID__) || defined(ANDROID)
		auto image(vcc::image::create(std::ref(queue),
			0, VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
			VK_SHARING_MODE_EXCLUSIVE, {}, env, state->activity->clazz,
			"png_transparency_demonstration_1", true));
#else
		auto image(draw_string(std::ref(device), face, utf8_to_utf32(u8"Köttfärssås!"), VK_IMAGE_USAGE_SAMPLED_BIT, VK_SHARING_MODE_EXCLUSIVE, {}));
#endif  // __ANDROID__

		auto image_view(vcc::image_view::create(std::move(image),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }));

		auto sampler(vcc::sampler::create(
			std::ref(device), VK_FILTER_LINEAR, VK_FILTER_LINEAR,
			VK_SAMPLER_MIPMAP_MODE_NEAREST,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			0, VK_TRUE, 1, VK_FALSE, VK_COMPARE_OP_NEVER, 0, 0,
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE, VK_FALSE));

		vcc::descriptor_set::update(device,
			vcc::descriptor_set::write_buffer(desc_set, 0, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				{ vcc::descriptor_set::buffer_info(std::ref(matrix_uniform_buffer)) }),
			vcc::descriptor_set::write_image{ desc_set, 1, 0,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				{
					vcc::descriptor_set::image_info{ std::move(sampler),
						std::move(image_view), VK_IMAGE_LAYOUT_GENERAL }
				} });
	}

	vcc::window::window_type window(vcc::window::create(
#ifdef WIN32
		GetModuleHandle(NULL),
#elif defined(__ANDROID__) || defined(ANDROID)
		state,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		nullptr, nullptr,
#endif // __ANDROID__
		std::ref(instance), std::ref(device), std::ref(queue),
		VkExtent2D{ 500, 500 }, VK_FORMAT_A8B8G8R8_UINT_PACK32, "Text demo"));

	vcc::shader_module::shader_module_type vert_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "text-vert.spv")
#else
			std::ifstream("text-vert.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));
	vcc::shader_module::shader_module_type frag_shader_module(
		vcc::shader_module::create(std::ref(device),
#if defined(__ANDROID__) || defined(ANDROID)
			android::asset_istream(state->activity->assetManager, "text-frag.spv")
#else
			std::ifstream("text-frag.spv",
				std::ios_base::binary | std::ios_base::in)
#endif  // __ ANDROID__
			));

	vcc::pipeline_cache::pipeline_cache_type pipeline_cache(
		vcc::pipeline_cache::create(std::ref(device)));

	vcc::command_pool::command_pool_type cmd_pool(vcc::command_pool::create(
		std::ref(device), 0, vcc::queue::get_family_index(queue)));
	std::vector<vcc::command_buffer::command_buffer_type> command_buffers;
	vcc::render_pass::render_pass_type render_pass;
	vcc::pipeline::pipeline_type pipeline;

	const float camera_scroll_delta_multiplier(.01f);
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
			projection_matrix = glm::perspective(45.f,
				float(extent.width) / extent.height, 1.f, 100.f);

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

			pipeline = vcc::pipeline::create_graphics(
				std::ref(device), pipeline_cache, 0,
				{
					vcc::pipeline::shader_stage(VK_SHADER_STAGE_VERTEX_BIT,
						std::ref(vert_shader_module), "main"),
					vcc::pipeline::shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT,
						std::ref(frag_shader_module), "main")
				},
				vcc::pipeline::vertex_input_state{
					{
						VkVertexInputBindingDescription{ 0, sizeof(glm::vec4) * 2,
							VK_VERTEX_INPUT_RATE_VERTEX }
					},
					{
						VkVertexInputAttributeDescription{ 0, 0,
							VK_FORMAT_R32G32B32_SFLOAT, 0 },
						VkVertexInputAttributeDescription{ 1, 0,
							VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec4) }
					}
				},
				vcc::pipeline::input_assembly_state
					{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE },
				vcc::pipeline::viewport_state(1, 1),
				vcc::pipeline::rasterization_state{ VK_FALSE, VK_FALSE,
					VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE,
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
				vcc::pipeline::dynamic_state{ { VK_DYNAMIC_STATE_VIEWPORT,
					VK_DYNAMIC_STATE_SCISSOR } },
				std::ref(pipeline_layout), std::ref(render_pass), 0);

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
						VK_SUBPASS_CONTENTS_INLINE,
						vcc::command::bind_pipeline{
							VK_PIPELINE_BIND_POINT_GRAPHICS, std::ref(pipeline) },
						vcc::command::bind_vertex_buffers(0, { std::ref(vertex_buffer) }, { 0, 0 }),
						vcc::command::bind_index_data_buffer(
							std::ref(index_buffer), 0, VK_INDEX_TYPE_UINT16),
						vcc::command::bind_descriptor_sets{
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							std::ref(pipeline_layout), 0,
							{ std::ref(desc_set) },{} },
						vcc::command::set_viewport{
							0, { { 0.f, 0.f, float(extent.width),
								float(extent.height), 0.f, 1.f } } },
						vcc::command::set_scissor{
							0, { { { 0, 0 }, extent } } },
						vcc::command::draw_indexed{
							(uint32_t)indices.size(), 1, 0, 0, 0 }),
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
			type::write(projection_modelview_matrix)[0] = projection_matrix
				* view_matrix * glm::rotate(angle.y, glm::vec3(1, 0, 0))
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
