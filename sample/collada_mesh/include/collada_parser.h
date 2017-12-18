#ifndef _COLLADA_PARSER_H_
#define _COLLADA_PARSER_H_

#include <cinttypes>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace collada_parser {

	template<typename T>
	using container_type = std::vector<T>;
	template<typename T>
	using iterator_type = typename container_type<T>::const_iterator;

	struct polygon_mesh_type {
		polygon_mesh_type(const polygon_mesh_type &) = delete;
		polygon_mesh_type(polygon_mesh_type &&) = default;
		polygon_mesh_type &operator=(const polygon_mesh_type &) = delete;
		polygon_mesh_type &operator=(polygon_mesh_type &&) = default;

		struct polygon_type {
			container_type<uint32_t> indices;
		};
		std::string name;
		container_type<glm::vec3> vertices;
		container_type<polygon_type> polygons;
	};

	struct node_type {
		node_type(const node_type &) = delete;
		node_type(node_type &&) = default;
		node_type &operator=(const node_type &) = delete;
		node_type &operator=(node_type &&) = default;

		struct lookat_type {
			glm::vec3 object, eye, up;
		};
		struct matrix_type {
			glm::mat4 value;
		};
		struct rotate_type {
			glm::quat value;
		};
		struct translate_type {
			glm::vec3 value;
		};
		struct scale_type {
			glm::quat value;
		};
		struct skew_type {
			float angle;
			glm::vec3 rotation_axis, translation;
		};
		std::string name;
		typedef std::variant<lookat_type, matrix_type, rotate_type, translate_type, scale_type, skew_type> transformation_type;
		container_type<transformation_type> transformations;
		container_type<iterator_type<polygon_mesh_type>> meshes;
		container_type<node_type> child_nodes;
	};

	struct collada_type {
		container_type<polygon_mesh_type> polygon_meshes;
		std::optional<node_type> scene;
	};

	collada_type parse(const std::string &filename);

} // namespace collada_parser

#endif // _COLLADA_PARSER_H_
