#include "materialfile.ih"

namespace dunya::serialize {

std::string writeMaterial(const StoredMaterial& stored) {
  std::string text;

  if (glz::write<glz::opts{.prettify = true}>(stored, text)) {
    return {};
  }

  return text;
}

bool readMaterial(std::string_view text, StoredMaterial& stored) {
  StoredMaterial read{};

  if (glz::read<glz::opts{.error_on_unknown_keys = false}>(read, text)) {
    return false;
  }

  if (read.version > MATERIAL_VERSION) {
    return false;
  }

  stored = read;

  return true;
}

}
