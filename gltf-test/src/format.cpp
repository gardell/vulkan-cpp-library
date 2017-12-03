#include <gltf/parser.h>
#include <gtest/gtest.h>
#include <string_view>

TEST(GltfJson, Simple) {
	gltf::parse_format(std::istringstream("[]"));
}

TEST(GltfBinary, Minimal) {
	auto json(std::string_view("{}"));
	const uint32_t headers[] = { 0x46546C67, 2, 0, uint32_t(json.length()), 0x4E4F534A };
	std::string data;
	data.append(reinterpret_cast<const char *>(headers), reinterpret_cast<const char *>(headers) + sizeof(headers))
		.append(json);
	auto result(gltf::parse_format(std::istringstream(data)));
}
