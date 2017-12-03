#include <filesystem>
#include <gltf/parser.h>
#include <gltf/base64.h>
#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
namespace fs = std::experimental::filesystem::v1;
#else
namespace fs = std::filesystem;
#endif

TEST(GltfJson, ExternalUri) {
	gltf::uri_type uri("external");

	ASSERT_EQ(
		std::get<gltf::uri_type::external_type>(uri.value).path,
		"external"
	);
}

TEST(GltfJson, ExternalUriWithDataPrefix) {
	gltf::uri_type uri("data:external");

	ASSERT_EQ(
		std::get<gltf::uri_type::external_type>(uri.value).path,
		"data:external"
	);
}

TEST(GltfJson, ExternalUriWithDataSeparatedPrefix) {
	gltf::uri_type uri("data:external;");

	ASSERT_EQ(
		std::get<gltf::uri_type::external_type>(uri.value).path,
		"data:external;"
	);
}

TEST(GltfJson, ExternalUriWithDataMisingTrailingComma) {
	gltf::uri_type uri("data:external;base32");

	ASSERT_EQ(
		std::get<gltf::uri_type::external_type>(uri.value).path,
		"data:external;base32"
	);
}


TEST(GltfJson, ExternalUriWithDataUnknownEncoding) {
	gltf::uri_type uri("data:external;base32,");

	ASSERT_EQ(
		std::get<gltf::uri_type::external_type>(uri.value).path,
		"data:external;base32,"
	);
}

TEST(GltfJson, DataUriWithUnknownMime) {
	try {
		gltf::uri_type("data:external;base64,");
	} catch (const std::invalid_argument &	) {
		return;
	}
	FAIL();
}

TEST(GltfJson, DataUri) {
	const char data[] = "Hello World!";

	std::ostringstream sstream;
	sstream << "data:image/jpeg;base64," << base64::encode(data);
	gltf::uri_type uri(sstream.str());

	ASSERT_EQ(
		std::get<gltf::uri_type::data_type>(uri.value).mime_type,
		gltf::uri_type::image_jpeg
	);
	ASSERT_EQ(
		std::get<gltf::uri_type::data_type>(uri.value).value,
		data
	);
}

TEST(GltfSamples, Samples) {
	for (const auto &directory1 : fs::directory_iterator("../gltf-sample-models-src/2.0")) {
		for (const auto &directory2 : fs::directory_iterator(directory1)) {
			for (const auto &file : fs::directory_iterator(directory2)) {
				auto path(file.path().string());
				auto gltf(path.rfind(".gltf") == path.length() - 5);
				auto glb(path.rfind(".glb") == path.length() - 4);
				if ((gltf ||glb)
					// We don't support extensions yet.
					&& path.find("glTF-MaterialsCommon") == std::string::npos
					&& path.find("glTF-techniqueWebGL") == std::string::npos) {
					std::cout << "Loading file " << path << std::endl;
					const gltf::model_type model(
						gltf::parse(gltf::parse_format(std::ifstream(file.path().string(), std::ios::binary | std::ios_base::in))));
					std::cout
						<< "copyright: " << model.asset.copyright.value_or("")
						<< "generator: " << model.asset.generator.value_or("")
						<< "min_version: " << model.asset.min_version.value_or("")
						<< "version: " << model.asset.version
						<< std::endl;
				}
			}
		}
	}
}