#include <collada_parser.h>
#include <COLLADAFW.h>
#include <COLLADAFWIWriter.h>
#include <COLLADASaxFWLIError.h>
#include <COLLADASaxFWLIErrorHandler.h>
#include <COLLADASaxFWLLoader.h>
#include <functional>
#include <glm/gtc/type_ptr.hpp>

namespace collada_parser {

	namespace writer_temporaries {

		struct node_type {
			node_type(const node_type &) = delete;
			node_type(node_type &&) = default;
			node_type &operator=(const node_type &) = delete;
			node_type &operator=(node_type &&) = default;
			node_type() = default;

			std::string name;
			container_type<collada_parser::node_type::transformation_type> transformations;
			container_type<COLLADAFW::UniqueId> meshes;
			container_type<node_type> child_nodes;
		};

	} // writer_temporaries

	struct collada_writer_type : public COLLADAFW::IWriter {

		std::map<COLLADAFW::UniqueId, polygon_mesh_type> polygon_meshes;
		std::map<COLLADAFW::UniqueId, writer_temporaries::node_type> root_nodes;
		std::optional<COLLADAFW::UniqueId> scene;

		std::variant<collada_type, std::invalid_argument> result;

		/** This method will be called if an error in the loading process occurred and the loader cannot
		continue to to load. The writer should undo all operations that have been performed.
		@param errorMessage A message containing informations about the error that occurred.
		*/
		void cancel(const COLLADAFW::String& errorMessage) {
			result = std::invalid_argument(errorMessage);
		}

		/** This is the method called. The writer hast to prepare to receive data.*/
		void start() {}

		static node_type convert_node(
			writer_temporaries::node_type &&node,
			const std::map<COLLADAFW::UniqueId, iterator_type<polygon_mesh_type>> &polygon_mesh_iterator_map) {
			container_type<node_type> child_nodes;
			child_nodes.reserve(node.child_nodes.size());
			std::transform(
				std::make_move_iterator(std::begin(node.child_nodes)),
				std::make_move_iterator(std::end(node.child_nodes)),
				std::back_inserter(child_nodes),
				std::bind(&convert_node, std::placeholders::_1, std::cref(polygon_mesh_iterator_map)));
			container_type<iterator_type<polygon_mesh_type>> meshes;
			meshes.reserve(node.meshes.size());
			std::transform(
				std::cbegin(node.meshes),
				std::cend(node.meshes),
				std::back_inserter(meshes),
				[&](const auto &mesh) {
				return polygon_mesh_iterator_map.at(mesh);
			});
			return node_type{
				std::move(node.name),
				std::move(node.transformations),
				std::move(meshes),
				std::move(child_nodes),
			};
		}

		/** This method is called after the last write* method. No other methods will be called after this.*/
		void finish() {
			container_type<polygon_mesh_type> polygon_meshes;
			std::map<COLLADAFW::UniqueId, iterator_type<polygon_mesh_type>> polygon_mesh_iterator_map;
			polygon_meshes.reserve(this->polygon_meshes.size());
			for (auto &&polygon_mesh : this->polygon_meshes) {
				polygon_meshes.push_back(std::move(polygon_mesh.second));
				polygon_mesh_iterator_map.insert(
					std::make_pair(std::move(polygon_mesh.first), std::end(polygon_meshes) - 1));
			}

			collada_type collada{
				std::move(polygon_meshes),
			};

			if (scene) {
				auto &root_node(root_nodes.at(*scene));
				collada.scene = convert_node(std::move(root_node), polygon_mesh_iterator_map);
			}

			result = std::move(collada);
		}

		/** When this method is called, the writer must write the global document asset.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeGlobalAsset(const COLLADAFW::FileInfo* asset) {
			// TODO(gardell): Assert file format is something we can use (right-hand coords and scale)
			return true;
		}

		/** When this method is called, the writer must write the scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeScene(const COLLADAFW::Scene* scene) {
			this->scene = scene->getInstanceVisualScene()->getInstanciatedObjectId();
			return true;
		}

		container_type<node_type::transformation_type> transformations(
			const COLLADAFW::TransformationPointerArray& transformations) {
			container_type<node_type::transformation_type> result_transformations;
			result_transformations.reserve(transformations.getCount());
			for (size_t transformation_index(0); transformation_index < transformations.getCount(); ++transformation_index) {
				const auto &transformation(*transformations[transformation_index]);
				switch (transformation.getTransformationType()) {
				case COLLADAFW::Transformation::MATRIX:
				{
					const auto &matrix(static_cast<const COLLADAFW::Matrix &>(transformation).getMatrix());
					result_transformations.push_back(node_type::matrix_type{
						// TODO(gardell): Transpose?
						glm::make_mat4(matrix.transpose()[0])
					});
				}
				break;
				case COLLADAFW::Transformation::TRANSLATE:
				{
					const auto &translate(static_cast<const COLLADAFW::Translate &>(transformation).getTranslation());
					result_transformations.push_back(node_type::translate_type{
						glm::vec3(translate.x, translate.y, translate.z)
					});
				}
				break;
				case COLLADAFW::Transformation::ROTATE:
				{
					const auto &rotate(static_cast<const COLLADAFW::Rotate &>(transformation));
					result_transformations.push_back(node_type::rotate_type{
						// TODO(gardell): Degrees or radians
						glm::angleAxis(
							float(rotate.getRotationAngle()),
							glm::vec3(
								rotate.getRotationAxis().x,
								rotate.getRotationAxis().y,
								rotate.getRotationAxis().z))
					});
				}
				break;
				case COLLADAFW::Transformation::SCALE:
				{
					const auto &scale(static_cast<const COLLADAFW::Scale &>(transformation).getScale());
					result_transformations.push_back(node_type::scale_type{
						glm::vec3(scale.x, scale.y, scale.z)
					});
				}
				break;
				case COLLADAFW::Transformation::LOOKAT:
				{
					const auto &lookat(static_cast<const COLLADAFW::Lookat &>(transformation));
					throw std::invalid_argument("lookat not implemented");
				}
				break;
				case COLLADAFW::Transformation::SKEW:
				{
					const auto &skew(static_cast<const COLLADAFW::Skew &>(transformation));
					throw std::invalid_argument("skew not implemented");
				}
				break;
				}
			}
			return std::move(result_transformations);
		}

		container_type<COLLADAFW::UniqueId> meshes(const COLLADAFW::InstanceGeometryPointerArray& instance_geometries) {
			container_type<COLLADAFW::UniqueId> meshes;
			meshes.reserve(instance_geometries.getCount());
			for (size_t instance_geometry(0); instance_geometry < instance_geometries.getCount(); ++instance_geometry) {
				meshes.push_back(instance_geometries[instance_geometry]->getInstanciatedObjectId());
			}
			return std::move(meshes);
		}

		writer_temporaries::node_type parse_visual_scene_node(const COLLADAFW::Node &node) {
			writer_temporaries::node_type result_node{
				node.getName(),
				transformations(node.getTransformations()),
				meshes(node.getInstanceGeometries())
			};
			const auto &child_nodes(node.getChildNodes());
			result_node.child_nodes.reserve(child_nodes.getCount());
			for (size_t node_index = 0; node_index < child_nodes.getCount(); ++node_index) {
				result_node.child_nodes.push_back(parse_visual_scene_node(*child_nodes[node_index]));
			}

			return std::move(result_node);
		}

		/** When this method is called, the writer must write the entire visual scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeVisualScene(const COLLADAFW::VisualScene* visualScene) {
			const auto &root_nodes(visualScene->getRootNodes());
			writer_temporaries::node_type node{
				visualScene->getName()
			};
			node.child_nodes.reserve(root_nodes.getCount());
			for (size_t node_index = 0; node_index < root_nodes.getCount(); ++node_index) {
				node.child_nodes.push_back(parse_visual_scene_node(*root_nodes[node_index]));
			}
			this->root_nodes.insert(std::make_pair(visualScene->getUniqueId(), std::move(node)));
			return true;
		}

		/** When this method is called, the writer must handle all nodes contained in the
		library nodes.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeLibraryNodes(const COLLADAFW::LibraryNodes* libraryNodes) {
			return true;
		}

		/** When this method is called, the writer must write the geometry.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeGeometry(const COLLADAFW::Geometry* geometry) {
			if (geometry->getType() == COLLADAFW::Geometry::GEO_TYPE_MESH || COLLADAFW::Geometry::GEO_TYPE_CONVEX_MESH) {
				std::vector<glm::vec3> result_vertices;
				std::vector<polygon_mesh_type::polygon_type> result_polygons;

				const auto &mesh(*static_cast<const COLLADAFW::Mesh *>(geometry));
				const auto &primitives(mesh.getMeshPrimitives());
				for (size_t primitive_index(0); primitive_index < primitives.getCount(); ++primitive_index) {
					const auto &primitive(*primitives[primitive_index]);
					switch (primitive.getPrimitiveType()) {
					case COLLADAFW::MeshPrimitive::POLYGONS:
					case COLLADAFW::MeshPrimitive::TRIANGLE_FANS:
					case COLLADAFW::MeshPrimitive::POLYLIST: // TODO(gardell): Is this the same as POLYGONS in the below interface?
					{
						const auto &position_indices(primitive.getPositionIndices());
						const size_t face_count(primitive.getFaceCount());
						result_polygons.reserve(face_count);
						for (size_t face_index = 0, position_index = 0; face_index < face_count; ++face_index) {
							const auto face_vertex_count(primitive.getGroupedVerticesVertexCount(face_index));
							polygon_mesh_type::polygon_type polygon;
							polygon.indices.reserve(face_vertex_count);
							for (size_t index = 0; index < face_vertex_count; ++index) {
								polygon.indices.push_back(position_indices[position_index + index]);
							}
							position_index += face_vertex_count;
							result_polygons.push_back(std::move(polygon));
						}
					}
					break;
					case COLLADAFW::MeshPrimitive::TRIANGLES:
					{
						const auto &position_indices(primitive.getPositionIndices());
						const size_t face_count(primitive.getFaceCount());
						for (size_t face_index = 0; face_index < face_count; ++face_index) {
							const auto position_index = face_index * 3;
							const auto face_vertex_count(3);
							polygon_mesh_type::polygon_type polygon;
							polygon.indices.reserve(face_vertex_count);
							for (size_t index = 0; index < face_vertex_count; ++index) {
								polygon.indices.push_back(position_indices[position_index + index]);
							}
						}
					}
					break;
					// TODO(gardell): Support triangle fans and strips
					case COLLADAFW::MeshPrimitive::TRIANGLE_STRIPS:
						throw std::invalid_argument("TRIANGLE_STRIPS not supported");
					default:
						// Don't support POINTS
						continue;
					}
					switch (mesh.getPositions().getType()) {
					case COLLADAFW::FloatOrDoubleArray::DATA_TYPE_FLOAT:
					{
						const auto &positions(*mesh.getPositions().getFloatValues());
						const auto positions_count(positions.getCount());
						assert(positions_count % 3 == 0);
						result_vertices.reserve(positions_count / 3);
						for (size_t position_index = 0; position_index < positions_count; position_index += 3) {
							result_vertices.push_back(
								glm::vec3(
									positions[position_index + 0],
									positions[position_index + 1],
									positions[position_index + 2]
								));
						}
					}
					break;
					case COLLADAFW::FloatOrDoubleArray::DATA_TYPE_DOUBLE:
					{
						// TODO(gardell): Identical to the above, collapse into a generic function
						const auto &positions(*mesh.getPositions().getDoubleValues());
						const auto positions_count(positions.getCount());
						assert(positions_count % 3 == 0);
						result_vertices.reserve(positions_count / 3);
						for (size_t position_index = 0; position_index < positions_count; position_index += 3) {
							result_vertices.push_back(
								glm::vec3(
									positions[position_index + 0],
									positions[position_index + 1],
									positions[position_index + 2]
								));
						}
					}
					break;
					}
				}

				polygon_meshes.insert(std::make_pair(geometry->getUniqueId(), std::move(polygon_mesh_type{
					geometry->getName(),
					std::move(result_vertices),
					std::move(result_polygons),
				})));
			}
			return true;
		}

		/** When this method is called, the writer must write the material.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeMaterial(const COLLADAFW::Material* material) {
			return true;
		}

		/** When this method is called, the writer must write the effect.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeEffect(const COLLADAFW::Effect* effect) {
			return true;
		}

		/** When this method is called, the writer must write the camera.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeCamera(const COLLADAFW::Camera* camera) {
			return true;
		}

		/** When this method is called, the writer must write the image.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeImage(const COLLADAFW::Image* image) {
			return true;
		}

		/** When this method is called, the writer must write the light.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeLight(const COLLADAFW::Light* light) {
			return true;
		}

		/** When this method is called, the writer must write the Animation.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeAnimation(const COLLADAFW::Animation* animation) {
			return true;
		}

		/** When this method is called, the writer must write the AnimationList.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeAnimationList(const COLLADAFW::AnimationList* animationList) {
			return true;
		}

		/** When this method is called, the writer must write the skin controller data.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeSkinControllerData(const COLLADAFW::SkinControllerData* skinControllerData) {
			return true;
		}

		/** When this method is called, the writer must write the controller.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeController(const COLLADAFW::Controller* controller) {
			return true;
		}

		/** When this method is called, the writer must write the formulas. All the formulas of the entire
		COLLADA file are contained in @a formulas.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeFormulas(const COLLADAFW::Formulas* formulas) {
			return true;
		}

		/** When this method is called, the writer must write the kinematics scene.
		@return The writer should return true, if writing succeeded, false otherwise.*/
		bool writeKinematicsScene(const COLLADAFW::KinematicsScene* kinematicsScene) {
			return true;
		}
	};

	struct sax_error_handler_type : public COLLADASaxFWL::IErrorHandler {
		bool handleError(const COLLADASaxFWL::IError* error) {

			return true;
		}
	};

	collada_type parse(const std::string &filename) {
		sax_error_handler_type sax_parser_error_handler;

		COLLADASaxFWL::Loader sax_loader(&sax_parser_error_handler);

		collada_writer_type collada_writer;
		COLLADAFW::Root root(&sax_loader, &collada_writer);

		root.loadDocument(filename);

		if (std::holds_alternative<collada_type>(collada_writer.result)) {
			return std::get<collada_type>(std::move(collada_writer.result));
		}
		else {
			throw std::get<std::invalid_argument>(std::move(collada_writer.result));
		}
	}

} // namespace collada_parser
