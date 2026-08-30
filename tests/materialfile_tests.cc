#include <catch2/catch_test_macros.hpp>

#include <dunya/serialize/materialfile/materialfile.h>

using dunya::serialize::MATERIAL_VERSION;
using dunya::serialize::readMaterial;
using dunya::serialize::StoredMaterial;
using dunya::serialize::writeMaterial;

TEST_CASE("a material survives a round trip through text", "[materialfile]") {
  StoredMaterial authored{};

  authored.baseColor = glm::vec4(0.5f, 0.25f, 0.125f, 1.0f);
  authored.emissive = glm::vec4(0.1f, 0.2f, 0.3f, 1.0f);
  authored.metallic = 0.75f;
  authored.roughness = 0.4f;
  authored.normalScale = 2.0f;
  authored.occlusionStrength = 0.6f;
  authored.alphaCutoff = 0.25f;
  authored.flags = 3u;
  authored.baseColorTexture.texture = 0xA1B2C3D4E5F60718ULL;
  authored.baseColorTexture.sampler = dunya::core::SAMPLER_NEAREST_CLAMP;
  authored.normalTexture.texture = 0x1807F6E5D4C3B2A1ULL;

  const std::string text = writeMaterial(authored);
  REQUIRE(!text.empty());

  StoredMaterial read{};
  REQUIRE(readMaterial(text, read));

  REQUIRE(read.baseColor == authored.baseColor);
  REQUIRE(read.emissive == authored.emissive);
  REQUIRE(read.metallic == authored.metallic);
  REQUIRE(read.roughness == authored.roughness);
  REQUIRE(read.normalScale == authored.normalScale);
  REQUIRE(read.occlusionStrength == authored.occlusionStrength);
  REQUIRE(read.alphaCutoff == authored.alphaCutoff);
  REQUIRE(read.flags == authored.flags);
  REQUIRE(read.baseColorTexture.texture == authored.baseColorTexture.texture);
  REQUIRE(read.baseColorTexture.sampler == authored.baseColorTexture.sampler);
  REQUIRE(read.normalTexture.texture == authored.normalTexture.texture);
}

TEST_CASE("a material file states only what differs", "[materialfile]") {
  StoredMaterial read{};

  read.roughness = 0.0f;
  read.alphaCutoff = 0.0f;

  REQUIRE(readMaterial(R"({"baseColor": [0.5, 0.0, 0.3, 1.0]})", read));

  REQUIRE(read.baseColor == glm::vec4(0.5f, 0.0f, 0.3f, 1.0f));
  REQUIRE(read.roughness == 1.0f);
  REQUIRE(read.alphaCutoff == 0.5f);
  REQUIRE(read.metallic == 0.0f);
  REQUIRE(read.baseColorTexture.texture == dunya::core::INVALID_ASSET);
  REQUIRE(read.baseColorTexture.sampler == dunya::core::SAMPLER_LINEAR_REPEAT);
  REQUIRE(read.version == MATERIAL_VERSION);
}

TEST_CASE("a material from another version refuses to load", "[materialfile]") {
  StoredMaterial read{};

  REQUIRE(!readMaterial(R"({"version": 9999})", read));
  REQUIRE(!readMaterial("not json at all", read));
}
