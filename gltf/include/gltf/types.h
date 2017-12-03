#ifndef _GLTF_TYPES_H_
#define _GLTF_TYPES_H_

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gltf/format.h>
#include <json.hpp>
#include <limits>
#include <map>
#include <string>
#include <variant>
#include <vector>

using json = nlohmann::json;

namespace gltf {

	typedef int32_t integer_type;
	typedef uint32_t unsigned_integer_type;
	typedef float decimal_type;

	template<typename T>
	using container_type = std::vector<T>;
	template<typename T>
	using iterator_type = typename container_type<T>::const_iterator;
	template<typename K, typename V>
	using map_type = std::map<K, V>;

	class number_type {
	private:
		std::variant<integer_type, unsigned_integer_type, decimal_type> value;

	public:
		number_type(integer_type value) : value(value) {}
		number_type(unsigned_integer_type value) : value(value) {}
		number_type(decimal_type value) : value(value) {}
		number_type(const json &json) {
			if (json.is_number_float()) {
				value = decimal_type(json);
			}
			else if (json.is_number_unsigned()) {
				value = unsigned_integer_type(json);
			}
			else if (json.is_number_integer()) {
				value = integer_type(json);
			}
			else {
				throw std::invalid_argument("Not a number");
			}
		}

		std::optional<integer_type> as_integer() {
			if (const auto value = std::get_if<integer_type>(&this->value)) {
				return std::make_optional(*value);
			}
			else if (const auto value = std::get_if<unsigned_integer_type>(&this->value)) {
				return *value < unsigned_integer_type(std::numeric_limits<integer_type>::max())
					? std::make_optional((integer_type)*value)
					: std::optional<integer_type>();
			}
			else {
				return std::optional<integer_type>();
			}
		}

		std::optional<unsigned_integer_type> as_unsigned_integer() {
			if (const auto value = std::get_if<unsigned_integer_type>(&this->value)) {
				return std::make_optional(*value);
			}
			else if (const auto value = std::get_if<integer_type>(&this->value)) {
				return *value >= 0
					? std::make_optional((unsigned_integer_type)*value)
					: std::optional<unsigned_integer_type>();
			}
			else {
				return std::optional<unsigned_integer_type>();
			}
		}

		std::optional<decimal_type> as_decimal() {
			if (const auto value = std::get_if<decimal_type>(&this->value)) {
				return std::make_optional(*value);
			}
			else if (const auto value = std::get_if<integer_type>(&this->value)) {
				return std::make_optional(decimal_type(*value));
			}
			else if (const auto value = std::get_if<unsigned_integer_type>(&this->value)) {
				return std::make_optional(decimal_type(*value));
			}
			else {
				return std::optional<decimal_type>();
			}
		}
	};

	struct uri_type {
		enum mime_type_type {
			application_octet_stream,
			image_jpeg,
			image_png,
		};
		struct data_type {
			std::string value;
			mime_type_type mime_type;
		};
		struct external_type {
			std::string path;
		};

		explicit uri_type(std::string &&value);

		std::variant<data_type, external_type> value;
	};

	struct asset_type {
		std::optional<std::string> copyright, generator;
		std::string version;
		std::optional<std::string> min_version;
		json extensions, extras;
	};

	struct buffer_type {
		buffer_type(const buffer_type &) = delete;
		buffer_type(buffer_type &&) = default;
		buffer_type &operator=(const buffer_type &) = delete;
		buffer_type &operator=(buffer_type &&) = default;

		std::optional<uri_type> uri;
		size_t byte_length;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct buffer_view_type {
		buffer_view_type(const buffer_view_type &) = delete;
		buffer_view_type(buffer_view_type &&) = default;
		buffer_view_type &operator=(const buffer_view_type &) = delete;
		buffer_view_type &operator=(buffer_view_type &&) = default;

		enum target_type {
			ARRAY_BUFFER = 34962,
			ELEMENT_ARRAY_BUFFER = 34963,
		};

		iterator_type<buffer_type> buffer;
		off_t byte_offset;
		size_t byte_length;
		std::optional<off_t> byte_stride;
		std::optional<target_type> target;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct accessor_type {
		accessor_type(const accessor_type &) = delete;
		accessor_type(accessor_type &&) = default;
		accessor_type &operator=(const accessor_type &) = delete;
		accessor_type &operator=(accessor_type &&) = default;

		enum component_type_type {
			BYTE = 5120,
			UNSIGNED_BYTE = 5121,
			SHORT = 5122,
			UNSIGNED_SHORT = 5123,
			UNSIGNED_INT = 5125,
			FLOAT = 5126,
		};

		enum type_type {
			SCALAR,
			VEC2,
			VEC3,
			VEC4,
			MAT2,
			MAT3,
			MAT4,
		};

		std::optional<iterator_type<buffer_view_type>> buffer_view;
		off_t byte_offset;
		component_type_type component_type;
		bool normalized;
		size_t count;
		type_type type;
		std::optional<container_type<number_type>> max, min;
		json sparse;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct camera_type {
		camera_type(const camera_type &) = delete;
		camera_type(camera_type &&) = default;
		camera_type &operator=(const camera_type &) = delete;
		camera_type &operator=(camera_type &&) = default;

		struct orthographic_type {
			number_type xmag, ymag, zfar, znear;
			json extensions, extras;
		};

		struct perspective_type {
			std::optional<number_type> aspect_ratio;
			number_type yfov;
			std::optional<number_type> zfar;
			number_type znear;
			json extensions, extras;
		};

		std::variant<orthographic_type, perspective_type> type;
		std::optional<std::string> name;
		json extensions, extras;
	};

	enum mime_type {
		image_jpeg,
		image_png,
	};

	struct image_type {
		image_type(const image_type &) = delete;
		image_type(image_type &&) = default;
		image_type &operator=(const image_type &) = delete;
		image_type &operator=(image_type &&) = default;

		std::variant<uri_type, iterator_type<buffer_view_type>> uri_buffer_view;
		std::optional<mime_type> mime_type; // required with buffer_view
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct sampler_type {
		sampler_type(const sampler_type &) = delete;
		sampler_type(sampler_type &&) = default;
		sampler_type &operator=(const sampler_type &) = delete;
		sampler_type &operator=(sampler_type &&) = default;

		enum mag_filter_type {
			MAG_FILTER_NEAREST = 9728,
			MAG_FILTER_LINEAR = 9729,
		};
		enum min_filter_type {
			MIN_FILTER_NEAREST = 9728,
			MIN_FILTER_LINEAR = 9729,
			MIN_FILTER_NEAREST_MIPMAP_NEAREST = 9984,
			MIN_FILTER_LINEAR_MIPMAP_NEAREST = 9985,
			MIN_FILTER_NEAREST_MIPMAP_LINEAR = 9986,
			MIN_FILTER_LINEAR_MIPMAP_LINEAR = 9987,
		};
		enum wrap_type {
			CLAMP_TO_EDGE = 33071,
			MIRRORED_REPEAT = 33648,
			REPEAT = 10497,
		};

		std::optional<mag_filter_type> mag_filter;
		std::optional<min_filter_type> min_filter;
		wrap_type wrap_s, wrap_t;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct texture_type {
		texture_type(const texture_type &) = delete;
		texture_type(texture_type &&) = default;
		texture_type &operator=(const texture_type &) = delete;
		texture_type &operator=(texture_type &&) = default;

		// TODO(gardell): "When undefined, a sampler with repeat wrapping and auto filtering should be used."
		// i.e. create a default sampler.
		std::optional<iterator_type<sampler_type>> sampler;
		std::optional<iterator_type<image_type>> source;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct texture_info_type {
		texture_info_type(const texture_info_type &) = delete;
		texture_info_type(texture_info_type &&) = default;
		texture_info_type &operator=(const texture_info_type &) = delete;
		texture_info_type &operator=(texture_info_type &&) = default;

		iterator_type<texture_type> index;
		unsigned_integer_type texcoord;
		json extensions, extras;
	};

	struct normal_texture_info_type {
		normal_texture_info_type(const normal_texture_info_type &) = delete;
		normal_texture_info_type(normal_texture_info_type &&) = default;
		normal_texture_info_type &operator=(const normal_texture_info_type &) = delete;
		normal_texture_info_type &operator=(normal_texture_info_type &&) = default;

		iterator_type<texture_type> index;
		unsigned_integer_type texcoord;
		std::optional<decimal_type> scale;
		json extensions, extras;
	};

	struct occlusion_texture_info_type {
		occlusion_texture_info_type(const occlusion_texture_info_type &) = delete;
		occlusion_texture_info_type(occlusion_texture_info_type &&) = default;
		occlusion_texture_info_type &operator=(const occlusion_texture_info_type &) = delete;
		occlusion_texture_info_type &operator=(occlusion_texture_info_type &&) = default;

		iterator_type<texture_type> index;
		unsigned_integer_type texcoord;
		std::optional<decimal_type> strength;
		json extensions, extras;
	};

	struct material_type {
		material_type(const material_type &) = delete;
		material_type(material_type &&) = default;
		material_type &operator=(const material_type &) = delete;
		material_type &operator=(material_type &&) = default;

		struct pbr_metallic_roughness_type {
			std::optional<glm::vec4> base_color_factor;
			std::optional<texture_info_type> base_color_texture;
			std::optional<decimal_type> metallic_factor, roughness_factor;
			std::optional<texture_info_type> metallic_roughness_texture;
			json extensions, extras;
		};
		enum alpha_mode_type {
			OPAQUE,
			MASK,
			BLEND,
		};

		std::optional<std::string> name;
		json extensions, extras;

		pbr_metallic_roughness_type pbr_metallic_roughness;
		std::optional<normal_texture_info_type> normal_texture;
		std::optional<occlusion_texture_info_type> occlusion_texture;
		std::optional<texture_info_type> emissive_texture;
		std::optional<glm::vec3> emissive_factor;
		alpha_mode_type alpha_mode;
		std::optional<decimal_type> alpha_cutoff;
		bool double_sided;
	};

	enum attribute_type {
		POSITION,
		NORMAL,
		TANGENT,
		TEXCOORD_0,
		TEXCOORD_1,
		COLOR_0,
		JOINTS_0,
		WEIGHTS_0,
	};

	struct primitive_type {
		primitive_type(const primitive_type &) = delete;
		primitive_type(primitive_type &&) = default;
		primitive_type &operator=(const primitive_type &) = delete;
		primitive_type &operator=(primitive_type &&) = default;

		enum mode_type {
			POINTS = 0,
			LINES,
			LINE_LOOP,
			LINE_STRIP,
			TRIANGLES,
			TRIANGLE_STRIP,
			TRIANGLE_FAN,
		};

		enum morph_target_attribute_type {
			POSITION = attribute_type::POSITION,
			NORMAL = attribute_type::NORMAL,
			TANGENT = attribute_type::TANGENT,
		};

		map_type<attribute_type, iterator_type<accessor_type>> attributes;
		std::optional<iterator_type<accessor_type>> indices;
		std::optional<iterator_type<material_type>> material;
		mode_type mode;
		map_type<morph_target_attribute_type, iterator_type<accessor_type>> targets;
		json extensions, extras;
	};

	struct mesh_type {
		mesh_type(const mesh_type &) = delete;
		mesh_type(mesh_type &&) = default;
		mesh_type &operator=(const mesh_type &) = delete;
		mesh_type &operator=(mesh_type &&) = default;

		container_type<primitive_type> primitives;
		std::optional<container_type<decimal_type>> weights;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct translation_rotation_scale_type {
		std::optional<glm::vec3> translation;
		std::optional<glm::quat> rotation;
		std::optional<glm::vec3> scale;
	};

	struct node_type;

	struct skin_type {
		skin_type(const skin_type &) = delete;
		skin_type(skin_type &&) = default;
		skin_type &operator=(const skin_type &) = delete;
		skin_type &operator=(skin_type &&) = default;

		std::optional<iterator_type<accessor_type>> inverse_bind_matrices;
		std::optional<iterator_type<node_type>> skeleton;
		container_type<iterator_type<node_type>> joints;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct node_type {
		node_type(const node_type &) = delete;
		node_type(node_type &&) = default;
		node_type &operator=(const node_type &) = delete;
		node_type &operator=(node_type &&) = default;

		std::optional<iterator_type<camera_type>> camera;
		container_type<iterator_type<node_type>> children;
		std::optional<iterator_type<skin_type>> skin;
		std::variant<glm::mat4, translation_rotation_scale_type> transformation;
		std::optional<iterator_type<mesh_type>> mesh;
		std::optional<container_type<decimal_type>> weights;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct animation_sampler_type {
		animation_sampler_type(const animation_sampler_type &) = delete;
		animation_sampler_type(animation_sampler_type &&) = default;
		animation_sampler_type &operator=(const animation_sampler_type &) = delete;
		animation_sampler_type &operator=(animation_sampler_type &&) = default;

		enum interpolation_type {
			LINEAR,
			STEP,
			CATMULLROMSPLINE,
			CUBICSPLINE,
		};
		iterator_type<accessor_type> input;
		interpolation_type interpolation;
		iterator_type<accessor_type> output;
		json extensions, extras;
	};

	struct channel_type {
		channel_type(const channel_type &) = delete;
		channel_type(channel_type &&) = default;
		channel_type &operator=(const channel_type &) = delete;
		channel_type &operator=(channel_type &&) = default;

		struct target_type {
			target_type(const target_type &) = delete;
			target_type(target_type &&) = default;
			target_type &operator=(const target_type &) = delete;
			target_type &operator=(target_type &&) = default;

			enum path_type {
				translation,
				rotation,
				scale,
				weights,
			};

			std::optional<iterator_type<node_type>> node;
			path_type path;
			json extensions, extras;
		};

		iterator_type<animation_sampler_type> sampler;
		target_type target;
		json extensions, extras;
	};

	struct animation_type {
		animation_type(const animation_type &) = delete;
		animation_type(animation_type &&) = default;
		animation_type &operator=(const animation_type &) = delete;
		animation_type &operator=(animation_type &&) = default;

		container_type<channel_type> channels;
		container_type<animation_sampler_type> samplers;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct scene_type {
		scene_type(const scene_type &) = delete;
		scene_type(scene_type &&) = default;
		scene_type &operator=(const scene_type &) = delete;
		scene_type &operator=(scene_type &&) = default;

		std::optional<container_type<iterator_type<node_type>>> nodes;
		std::optional<std::string> name;
		json extensions, extras;
	};

	struct model_type {
		model_type(const model_type &) = delete;
		model_type(model_type &&) = default;
		model_type &operator=(const model_type &) = delete;
		model_type &operator=(model_type &&) = default;

		container_type<std::string> extensions_used, extensions_required;
		container_type<accessor_type> accessors;
		container_type<animation_type> animations;
		asset_type asset;
		container_type<buffer_type> buffers;
		container_type<buffer_view_type> buffer_views;
		container_type<camera_type> cameras;
		container_type<image_type> images;
		container_type<material_type> materials;
		container_type<mesh_type> meshes;
		container_type<node_type> nodes;
		container_type<sampler_type> samplers;
		std::optional<iterator_type<scene_type>> scene;
		container_type<scene_type> scenes;
		container_type<skin_type> skins;
		container_type<texture_type> textures;
		json extensions, extras;
	};

} // namespace gltf

#endif // _GLTF_TYPES_H_
