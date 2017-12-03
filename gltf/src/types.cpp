#include <gltf/base64.h>
#include <gltf/types.h>

namespace gltf {

	namespace {
		uri_type::mime_type_type uri_mime_type(const std::string &mime_type) {
			if (mime_type == "application/octet-stream") {
				return uri_type::application_octet_stream;
			}
			else if (mime_type == "image/jpeg") {
				return uri_type::image_jpeg;
			}
			else if (mime_type == "image/png") {
				return uri_type::image_png;
			}
			else {
				throw std::invalid_argument("uri mime type");
			}
		}
	}

	uri_type::uri_type(std::string &&value) {
		const char start[] = "data:";
		size_t mime_separator, encoding_separator;
		if (value.compare(0, sizeof(start) - 1, start) == 0
			&& (mime_separator = value.find(';', sizeof(start) - 1)) != std::string::npos
			&& (encoding_separator = value.find(',', mime_separator)) != std::string::npos
			&& std::string_view(value.c_str() + mime_separator + 1, encoding_separator - (mime_separator + 1)) == "base64") {
			this->value = data_type{
				base64::decode(std::string_view(value.c_str() + encoding_separator + 1, value.length() - (encoding_separator + 1))),
				uri_mime_type(
					value.substr(
						sizeof(start) - 1,
						mime_separator - (sizeof(start) - 1)
					)
				),
			};
		}
		else {
			this->value = external_type{
				value
			};
		}
	}

} // namespace gltf
