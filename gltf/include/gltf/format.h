#ifndef _GLTF_FORMAT_H_
#define _GLTF_FORMAT_H_

#include <inttypes.h>
#include <json.hpp>
#include <sstream>
#include <string>
#include <optional>

using json = nlohmann::json;

namespace gltf {

	struct format_type {
		json json;
		std::optional<std::string> binary;
	};

	template<typename Stream>
	format_type parse_format(Stream &&stream) {
		char p(stream.peek());
		if (stream.peek() == 'g') {
			uint32_t header[5];
			stream.read(reinterpret_cast<char*>(header), sizeof(header));

			if (header[0] != 0x46546C67) {
				throw std::runtime_error("Invalid magic");
			}
			else if (header[1] != 2) {
				throw std::runtime_error("Invalid version");
			}
			else if (header[4] != 0x4E4F534A) {
				throw std::runtime_error("Expected json chunk");
			}

			std::string json(header[3], '\0');
			stream.read(json.data(), json.length());

			std::optional<std::string> binary;

			{
				uint32_t header[2];
				if (stream.read(reinterpret_cast<char*>(header), sizeof(header))) {

					if (header[1] != 0x004E4942) {
						throw std::runtime_error("Expected bin chunk");
					}

					std::string bin(header[0], '\0');
					stream.read(bin.data(), bin.length());
					binary = bin;
				}
			}

			return format_type{ json::parse(json), binary };
		}
		else {
#if 0
			std::ostringstream buffer;
			stream >> buffer.rdbuf();
			return format_type{ json::parse(buffer.str()) };
#else
			json j;
			stream >> j;
			return format_type{ std::move(j) };
#endif
		}
	}

} // namespace gltf

#endif // _GLTF_FORMAT_H_
