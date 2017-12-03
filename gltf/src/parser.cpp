#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <gltf/conversions.h>
#include <gltf/format.h>
#include <gltf/optional.h>
#include <gltf/parser.h>
#include <json.hpp>
#include <stdexcept>
#include <string_view>
#include <utility>

using json = nlohmann::json;

namespace gltf {

	template<typename T, std::size_t... I>
	T json_array_to_glm_type(const json &json, std::index_sequence<I...>) {
		if (json.size() < sizeof...(I)) {
			throw std::invalid_argument("not enough fields for glm type");
		} else {
			return T(typename T::value_type(std::cbegin(json)[I])...);
		}
	}

	template<std::size_t I>
	glm::vec<I, float> json_array_to_vec(const json &j) {
		return json_array_to_glm_type<glm::vec<I, float>>(j, std::make_index_sequence<I>{});
	}

	template<std::size_t C, std::size_t R>
	glm::mat<C, R, float> json_array_to_mat(const json &j) {
		return json_array_to_glm_type<glm::mat<C, R, float>>(j, std::make_index_sequence<C * R>{});
	}

	template<typename Container>
	glm::quat json_array_to_quat(const json &j) {
		return json_array_to_glm_type<glm::quat>(j, std::index_sequence<3, 0, 1, 2>{});
	}

	template<typename F>
	auto transform_json_array_to_container(const json &array, F &&f) {
		typedef decltype(f(std::declval<const json &>())) result_type;
		container_type<result_type> container;
		container.reserve(array.size());
		std::transform(
			std::cbegin(array),
			std::cend(array),
			std::back_inserter(container),
			std::forward<F>(f));
		return container;
	}

	template<typename It, typename... Its, typename F>
	auto zip_foreach(
			F &&f,
			It it, It end,
			Its&&... its) {
		for (; it != end; ++it) {
			f(*it, (*its++)...);
		}
	}

	template<typename T>
	iterator_type<T> index_to_iterator(
			const container_type<T> &container,
			typename container_type<T>::difference_type index) {
		if (typename container_type<T>::size_type(index) < container.size()) {
			return container.cbegin() + index;
		} else {
			throw std::out_of_range("container"); 
		}
	}

	template<typename T>
	std::optional<iterator_type<T>> optional_index_to_iterator(
		const json &json,
		const char *field,
		const container_type<T> &container) {
		return map(
			optional_cast<typename container_type<T>::difference_type>(json, field),
			std::bind(index_to_iterator<T>, std::cref(container), std::placeholders::_1)
		);
	}

	model_type parse(const format_type &format) {
		container_type<std::string>
			extensions_used(
				format.json.value("extensionsUsed", json::array())
					.get<std::vector<std::string>>()),
			extensions_required(
				format.json.value("extensionsRequired", json::array())
					.get<std::vector<std::string>>());

		container_type<buffer_type> buffers(map(
			optional_ref(format.json, "buffers"),
			[&](const json &buffers) {
			return transform_json_array_to_container(buffers, [&](const json &buffer) {
				return buffer_type{
					map(optional_ref(buffer, "uri"), [](const json &uri) {
						return uri_type(uri.get<std::string>());
					}),
					buffer["byteLength"],
					optional_cast<std::string>(buffer, "name"),
					buffer.value("extensions", json::object()),
					buffer.value("extras", json()),
				};
			});
		}).value_or(container_type<buffer_type>()));

		container_type<buffer_view_type> buffer_views(map(
			optional_ref(format.json, "bufferViews"),
			[&](const json &buffer_views) {
			return transform_json_array_to_container(buffer_views, [&](const json &buffer_view) {
				return buffer_view_type{
					index_to_iterator(buffers, buffer_view["buffer"]),
					optional_cast<off_t>(buffer_view, "byteOffset").value_or(off_t(0)),
					buffer_view["byteLength"],
					optional_cast<off_t>(buffer_view, "byteStride"),
					map(optional_cast<unsigned_integer_type>(buffer_view, "target"), buffer_view_target_from_json),
					optional_cast<std::string>(buffer_view, "name"),
					buffer_view.value("extensions", json::object()),
					buffer_view.value("extras", json()),
				};
			});
		}).value_or(container_type<buffer_view_type>()));

		container_type<accessor_type> accessors(map(
			optional_ref(format.json, "accessors"),
			[&](const json &accessors) {
				return transform_json_array_to_container(accessors, [&](const json &accessor) {
					std::optional<iterator_type<buffer_view_type>> buffer_view(
						optional_index_to_iterator(accessor, "bufferView", buffer_views)
					);

					return accessor_type{
						buffer_view,
						accessor.value("byteOffset", integer_type(0)),
						component_type_from_json(accessor["componentType"]),
						bool(accessor.value("normalized", false)),
						size_t(accessor["count"]),
						accessor_type_from_json(accessor["type"]),
						map(optional_cast<json>(accessor, "max"), number_container_from_json),
						map(optional_cast<json>(accessor, "min"), number_container_from_json),
						accessor.value("sparse", json::object()),
						optional_cast<std::string>(accessor, "name"),
						accessor.value("extensions", json::object()),
						accessor.value("extras", json()),
					};
				});
			}).value_or(container_type<accessor_type>()));

		container_type<camera_type> cameras(map(
			optional_ref(format.json, "cameras"),
			[&](const json &cameras) {
			return transform_json_array_to_container(cameras, [&](const json &camera) {
				if (camera["type"] == "perspective") {
					const auto &perspective(camera["perspective"]);
					return camera_type{
						camera_type::perspective_type{
						map(
							optional_ref(perspective, "aspectRatio"),
							[](const json &value) { return number_type(value); }
						),
						perspective["yfov"],
						map(
							optional_ref(perspective, "zfar"),
							[](const json &value) { return number_type(value); }
						),
						perspective["znear"],
						perspective.value("extensions", json::object()),
						perspective.value("extras", json()),
					},
					optional_cast<std::string>(camera, "name"),
					camera.value("extensions", json::object()),
					camera.value("extras", json()),
					};
				}
				else if (camera["type"] == "orthographic") {
					const auto &orthographic(camera["orthographic"]);
					return camera_type{
						camera_type::orthographic_type{
						orthographic["xmag"],
						orthographic["ymag"],
						orthographic["zfar"],
						orthographic["znear"],
						orthographic.value("extensions", json::object()),
						orthographic.value("extras", json()),
					},
					optional_cast<std::string>(camera, "name"),
					camera.value("extensions", json::object()),
					camera.value("extras", json()),
					};
				}
				else {
					throw std::invalid_argument("camera type");
				}
			});
		}).value_or(container_type<camera_type>()));

		container_type<sampler_type> samplers(map(
			optional_ref(format.json, "samplers"),
			[&](const json &samplers) {
			return transform_json_array_to_container(samplers, [&](const json &sampler) {
				return sampler_type{
					map(
						optional_cast<unsigned_integer_type>(sampler, "magFilter"),
						sampler_mag_filter_from_json
					),
					map(
						optional_cast<unsigned_integer_type>(sampler, "minFilter"),
						sampler_min_filter_from_json
					),
					map(
						optional_cast<unsigned_integer_type>(sampler, "wrapS"),
						sampler_wrap_from_json
					).value_or(sampler_type::REPEAT),
					map(
						optional_cast<unsigned_integer_type>(sampler, "wrapT"),
						sampler_wrap_from_json
					).value_or(sampler_type::REPEAT),
					optional_cast<std::string>(sampler, "name"),
					sampler.value("extensions", json::object()),
					sampler.value("extras", json()),
				};
			});
		}).value_or(container_type<sampler_type>()));

		container_type<image_type> images(map(
			optional_ref(format.json, "images"),
			[&](const json &images) {
			return transform_json_array_to_container(images, [&](const json &image) {
				auto uri(map(
					optional_cast<std::string>(image, "uri"),
					[](std::string &&string) { return uri_type{ std::forward<std::string>(string) }; }));
				auto buffer_view(optional_index_to_iterator(image, "bufferView", buffer_views));
				auto mime(map(optional_cast<std::string>(image, "mimeType"), image_mime_type_from_json));
				auto name(optional_cast<std::string>(image, "name"));
				auto extensions(image.value("extensions", json::object()));
				auto extras(image.value("extras", json()));
				if (uri) {
					return image_type{
						uri.value(),
						mime,
						name,
						extensions,
						extras,
					};
				}
				else if (buffer_view) {
					if (!mime) {
						throw std::invalid_argument("mimeType required");
					}
					else {
						return image_type{
							buffer_view.value(),
							mime,
							name,
							extensions,
							extras,
						};
					}
				}
				else {
					throw std::invalid_argument("uri or bufferView");
				}
			});
		}).value_or(container_type<image_type>()));

		container_type<texture_type> textures(map(
			optional_ref(format.json, "textures"),
			[&](const json &textures) {
			return transform_json_array_to_container(textures, [&](const json &texture) {
				return texture_type{
					optional_index_to_iterator(texture, "sampler", samplers),
					optional_index_to_iterator(texture, "source", images),
					optional_cast<std::string>(texture, "name"),
					texture.value("extensions", json::object()),
					texture.value("extras", json()),
				};
			});
		}).value_or(container_type<texture_type>()));

		container_type<material_type> materials(map(
			optional_ref(format.json, "materials"),
			[&](const json &materials) {
			return transform_json_array_to_container(materials, [&](const json &material) {
				auto pbr_metallic_roughness(map(optional_ref(material, "pbrMetallicRoughness"),
					[&](const auto &pbr_metallic_roughness) {
						return material_type::pbr_metallic_roughness_type{
							map(
								optional_ref(pbr_metallic_roughness, "baseColorFactor"),
								json_array_to_vec<4>
							),
							map(
								optional_ref(pbr_metallic_roughness, "baseColorTexture"),
								[&](const json &texture) {
									return texture_info_type{
										index_to_iterator(textures, texture["index"]),
										optional_cast<unsigned_integer_type>(texture, "texcoord")
										.value_or(0),
										texture.value("extensions", json::object()),
										texture.value("extras", json()),
									};
								}
							),
							optional_cast<decimal_type>(pbr_metallic_roughness, "metallicFactor"),
							optional_cast<decimal_type>(pbr_metallic_roughness, "roughnessFactor"),
							map(
								optional_ref(pbr_metallic_roughness, "metallicRoughnessTexture"),
								[&](const json &texture) {
									return texture_info_type{
										index_to_iterator(textures, texture["index"]),
										optional_cast<unsigned_integer_type>(texture, "texcoord")
										.value_or(0),
										texture.value("extensions", json::object()),
										texture.value("extras", json()),
									};
								}
							),
							pbr_metallic_roughness.get().value("extensions", json::object()),
							pbr_metallic_roughness.get().value("extras", json()),
						};
					}));
				return material_type{
					optional_cast<std::string>(material, "name"),
					material.value("extensions", json::object()),
					material.value("extras", json()),
					pbr_metallic_roughness ? std::move(pbr_metallic_roughness).value() : material_type::pbr_metallic_roughness_type{},
					map(
						optional_ref(material, "normalTexture"),
						[&](const json &texture) {
							return normal_texture_info_type{
								index_to_iterator(textures, texture["index"]),
								optional_cast<unsigned_integer_type>(texture, "texcoord")
									.value_or(0),
								optional_cast<decimal_type>(texture, "scale"),
								texture.value("extensions", json::object()),
								texture.value("extras", json()),
							};
						}
					),
					map(
						optional_ref(material, "occlusionTexture"),
						[&](const json &texture) {
							return occlusion_texture_info_type{
								index_to_iterator(textures, texture["index"]),
								optional_cast<unsigned_integer_type>(texture, "texcoord")
									.value_or(0),
								optional_cast<decimal_type>(texture, "strength"),
								texture.value("extensions", json::object()),
								texture.value("extras", json()),
							};
						}
					),
					map(
						optional_ref(material, "emissiveTexture"),
						[&](const json &texture) {
							return texture_info_type{
								index_to_iterator(textures, texture["index"]),
								optional_cast<unsigned_integer_type>(texture, "texcoord")
									.value_or(0),
								texture.value("extensions", json::object()),
								texture.value("extras", json()),
							};
						}
					),
					map(
						optional_ref(material, "emissiveFactor"),
						json_array_to_vec<3>),
					map(
						optional_ref(material, "alphaMode"),
						material_alpha_mode_from_json
					).value_or(material_type::OPAQUE),
					optional_cast<decimal_type>(material, "alphaCutoff"),
					optional_cast<bool>(material, "doubleSided").value_or(false),
				};
			});
		}).value_or(container_type<material_type>()));

		container_type<mesh_type> meshes(map(
			optional_ref(format.json, "meshes"),
			[&](const json &meshes) {
			return transform_json_array_to_container(meshes, [&](const json &mesh) {
				container_type<primitive_type> primitives(transform_json_array_to_container(
					mesh["primitives"],
					[&](const json &primitive) {
						const auto &attributes_json(primitive["attributes"]);
						map_type<attribute_type, iterator_type<accessor_type>> attributes;
						for (auto it(std::cbegin(attributes_json)); it != std::cend(attributes_json); ++it) {
							attributes.emplace(std::make_pair(
								attribute_from_json(it.key()),
								index_to_iterator(accessors, it.value())));
						}
						auto targets(map(
							optional_ref(primitive, "targets"),
							[&](const json &targets_json) {
								map_type<primitive_type::morph_target_attribute_type, iterator_type<accessor_type>> targets;
								for (auto it(std::cbegin(targets_json)); it != std::cend(targets_json); ++it) {
									targets.emplace(std::make_pair(
										morph_target_attribute_from_json(it.key()),
										index_to_iterator(accessors, it.value())
									));
								}
								return targets;
							}));
						return primitive_type{
							attributes,
							optional_index_to_iterator(primitive, "indices", accessors),
							optional_index_to_iterator(primitive, "material", materials),
							optional_cast<primitive_type::mode_type>(primitive, "mode").value_or(primitive_type::TRIANGLES),
							std::move(targets).value_or(map_type<primitive_type::morph_target_attribute_type, iterator_type<accessor_type>>()),
							mesh.value("extensions", json::object()),
							mesh.value("extras", json()),
						};
					}
				));
				return mesh_type{
					std::move(primitives),
					optional_cast<container_type<decimal_type>>(mesh, "weights"),
					optional_cast<std::string>(mesh, "name"),
					mesh.value("extensions", json::object()),
					mesh.value("extras", json()),
				};
			});
		}).value_or(container_type<mesh_type>()));

		container_type<skin_type> skins(map(
			optional_ref(format.json, "skins"),
			[&](const json &skins) {
			return transform_json_array_to_container(skins, [&](const json &skin) {
				return skin_type{
					optional_index_to_iterator(skin, "inverseBindMatrices", accessors),
					std::optional<iterator_type<node_type>>(),
					container_type<iterator_type<node_type>>(),
					optional_cast<std::string>(skin, "name"),
					skin.value("extensions", json::object()),
					skin.value("extras", json()),
				};
			});
		}).value_or(container_type<skin_type>()));

		container_type<node_type> nodes(map(
			optional_ref(format.json, "nodes"),
			[&](const json &nodes) {
			return transform_json_array_to_container(nodes, [&](const json &node) {

				auto matrix(map(
					optional_ref(node, "matrix"),
					json_array_to_mat<4, 4>));
				auto trs(translation_rotation_scale_type{
					map(
						optional_ref(node, "translation"),
						json_array_to_vec<3>),
					map(
						optional_ref(node, "rotation"),
						json_array_to_quat<json>),
					map(
						optional_ref(node, "scale"),
						json_array_to_vec<3>),
				});
				return node_type{
					optional_index_to_iterator(node, "camera", cameras),
					container_type<iterator_type<node_type>>(),
					optional_index_to_iterator(node, "skin", skins),
					matrix
						? std::variant<glm::mat4, translation_rotation_scale_type>(*matrix)
						: trs,
					optional_index_to_iterator(node, "mesh", meshes),
					optional_cast<container_type<decimal_type>>(node, "weights"),
					optional_cast<std::string>(node, "name"),
					node.value("extensions", json::object()),
					node.value("extras", json()),
				};
			});
		}).value_or(container_type<node_type>()));

		map(
			optional_ref(format.json, "nodes"),
			[&](const json &nodes_json) {
			zip_foreach(
				[&](const json &node_json, node_type &node) {
				node.children = map(
					optional_ref(node_json, "children"),
					[&](const json &children) {
						return transform_json_array_to_container(
							node_json["children"],
							std::bind(
								index_to_iterator<node_type>,
								std::cref(nodes),
								std::placeholders::_1
							)
						);
					}).value_or(container_type<iterator_type<node_type>>());
				},
				std::cbegin(nodes_json),
				std::cend(nodes_json),
				std::begin(nodes));
		}
		);

		map(
			optional_ref(format.json, "skins"),
			[&](const json &skins_json) {
				zip_foreach(
					[&](const json &skin_json, skin_type &skin) {
						skin.skeleton = optional_index_to_iterator(skin_json, "skeleton", nodes);
						skin.joints = transform_json_array_to_container(
							skin_json["joints"],
							std::bind(
								index_to_iterator<node_type>,
								std::cref(nodes),
								std::placeholders::_1
							)
						);
					},
					std::cbegin(skins_json),
					std::cend(skins_json),
					std::begin(skins));
			}
		);

		container_type<animation_type> animations(map(
			optional_ref(format.json, "animations"),
			[&](const json &animations) {
			return transform_json_array_to_container(animations, [&](const json &animation) {
				container_type<animation_sampler_type> samplers(
					transform_json_array_to_container(
						animation["samplers"],
						[&](const json &sampler) {
					return animation_sampler_type{
						index_to_iterator(accessors, sampler["input"]),
						map(
							optional_cast<std::string>(sampler, "interpolation"),
							animation_sampler_interpolation_from_json)
						.value_or(animation_sampler_type::LINEAR),
						index_to_iterator(accessors, sampler["output"]),
						sampler.value("extensions", json::object()),
						sampler.value("extras", json()),
					};
				}));

				container_type<channel_type> channels(transform_json_array_to_container(
					animation["channels"],
					[&](const auto &channel) {
					const auto &target(channel["target"]);
					return channel_type{
						index_to_iterator(samplers, channel["sampler"]),
						channel_type::target_type{
							optional_index_to_iterator(target, "node", nodes),
							channel_target_path_from_json(target["path"])
						},
						channel.value("extensions", json::object()),
						channel.value("extras", json()),
					};
				}
				));

				return animation_type{
					std::move(channels),
					std::move(samplers),
					optional_cast<std::string>(animation, "name"),
					animation.value("extensions", json::object()),
					animation.value("extras", json()),
				};
			});
		}).value_or(container_type<animation_type>()));

		container_type<scene_type> scenes(map(
			optional_ref(format.json, "scenes"),
			[&](const json &scenes) {
			return transform_json_array_to_container(scenes, [&](const json &scene) {
				return scene_type{
					map(
						optional_ref(scene, "nodes"),
						[&](const json &node) {
							return transform_json_array_to_container(
								node,
								std::bind(
									index_to_iterator<node_type>,
									std::cref(nodes),
									std::placeholders::_1)
							);
						}
					),
					optional_cast<std::string>(scene, "name"),
					scene.value("extensions", json::object()),
					scene.value("extras", json()),
				};
			});
		}).value_or(container_type<scene_type>()));

		const auto &asset(format.json["asset"]);

		auto scene(optional_index_to_iterator(format.json, "scene", scenes));

		return model_type{
			std::move(extensions_used),
			std::move(extensions_required),
			std::move(accessors),
			std::move(animations),
			asset_type{
				optional_cast<std::string>(asset, "copyright"),
				optional_cast<std::string>(asset, "generator"),
				asset["version"],
				optional_cast<std::string>(asset, "minVersion"),
				asset.value("extensions", json::object()),
				asset.value("extras", json()),
			},
			std::move(buffers),
			std::move(buffer_views),
			std::move(cameras),
			std::move(images),
			std::move(materials),
			std::move(meshes),
			std::move(nodes),
			std::move(samplers),
			std::move(scene),
			std::move(scenes),
			std::move(skins),
			std::move(textures),
			asset.value("extensions", json::object()),
			asset.value("extras", json()),
		};
	}

	std::variant<data_view_type, std::string> open(
			const fs::path &wd,
			const uri_type &uri,
			off_t offset,
			const std::optional<size_t> &length) {
		if (auto value = std::get_if<uri_type::data_type>(&uri.value)) {
			return data_view_type{
				std::string_view(
					value->value.data() + offset,
					length ? length.value() : (value->value.length() - offset)),
				value->mime_type,
			};
		}
		else if (auto value = std::get_if<uri_type::external_type>(&uri.value)) {
			std::ostringstream buffer;
			std::ifstream stream(wd / value->path, std::ios::binary | std::ios::in);
			stream.exceptions(std::ios::badbit | std::ios::failbit);
			stream.seekg(offset, std::ios::beg);
			if (length) {
				// TODO(gardell): This intermediate copy uses a lot of memory and is slow
				std::string tmp(*length, 0);
				stream.read(tmp.data(), tmp.length());
				buffer.write(tmp.data(), tmp.length());
			} else {
				stream >> buffer.rdbuf();
			}
			return buffer.str();
		}
		else {
			throw std::invalid_argument("invalid uri");
		}
	}

	opened_type open(
			const fs::path &wd,
			const format_type &format,
			const buffer_type &buffer,
			off_t offset,
			const std::optional<size_t> &length) {
		if (buffer.uri) {
			return std::visit(
				[](auto &&value) { return std::variant<data_view_type, std::string_view, std::string>(std::move(value)); },
				open(wd, *buffer.uri, offset, length));
		} else {
			return std::string_view(format.binary->data(), format.binary->length());
		}
	}

	opened_type open(
			const fs::path &wd,
			const format_type &format,
			const image_type &image) {
		if (auto uri = std::get_if<gltf::uri_type>(&image.uri_buffer_view)) {
			return std::visit(
				[](auto &&value) { return std::variant<data_view_type, std::string_view, std::string>(std::move(value)); },
				open(wd, *uri));
		} else if (auto buffer_view = std::get_if<gltf::iterator_type<gltf::buffer_view_type>>(&image.uri_buffer_view)) {
			return open(wd, format, *(*buffer_view)->buffer, (*buffer_view)->byte_offset, (*buffer_view)->byte_length);
		} else {
			throw std::invalid_argument("invalid image resource");
		}
	}

} // namespace gltf
