#ifndef _GLTF_PARSER_H_
#define _GLTF_PARSER_H_

#include <gltf/types.h>
#include <filesystem>

namespace gltf {
#ifdef _MSC_VER
	namespace fs = std::experimental::filesystem::v1;
#else
	namespace fs = std::filesystem;
#endif

	model_type parse(const format_type &format);

	struct data_view_type {
		std::string_view data;
		uri_type::mime_type_type mime_type;

		auto cbegin() const {
			return std::cbegin(data);
		}

		auto cend() const {
			return std::cend(data);
		}

		auto begin() const {
			return std::cbegin(data);
		}

		auto end() const {
			return std::cend(data);
		}
	};

	std::variant<data_view_type, std::string> open(
		const fs::path &wd,
		const uri_type &uri,
		off_t offset = 0,
		const std::optional<size_t> &length = std::optional<size_t>());

	// TODO(gardell): Create pretty type
	typedef std::variant<data_view_type, std::string_view, std::string> opened_type;
	opened_type open(
		const fs::path &wd,
		const format_type &,
		const buffer_type &,
		off_t offset = 0,
		const std::optional<size_t> &length = std::optional<size_t>());
	opened_type open(
		const fs::path &wd,
		const format_type &,
		const image_type &);

} // namespace gltf

#endif // _GLTF_PARSER_H_
