#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace {

std::string slurp(const std::filesystem::path& path) {
  std::ifstream file(path);

  INFO("path " << path.string());
  REQUIRE(file.is_open());

  std::ostringstream text;
  text << file.rdbuf();

  return text.str();
}

std::set<std::string> namesMatching(
  const std::string& text,
  const std::regex& pattern
) {
  std::set<std::string> names;

  for (auto at = std::sregex_iterator(text.begin(), text.end(), pattern);
       at != std::sregex_iterator();
       ++at) {
    names.insert((*at)[1].str());
  }

  return names;
}

std::set<std::string> declared() {
  const std::string header =
    slurp(std::filesystem::path(DUNYA_CAPI_DIR) / "dunya_c.h");

  return namesMatching(
    header,
    std::regex(R"(DUNYA_C_API[^;{]*?\b(dunya_[a-z0-9_]+)\s*\()")
  );
}

std::set<std::string> defined() {
  const std::string source =
    slurp(std::filesystem::path(DUNYA_CAPI_DIR) / "dunya_c.cc");

  return namesMatching(
    source,
    std::regex(R"(^[A-Za-z_][^;\n]*?\b(dunya_[a-z0-9_]+)\s*\()")
  );
}

}

TEST_CASE("every exported entry point is declared in the C header", "[capi]") {
  const std::set<std::string> inHeader = declared();
  const std::set<std::string> inSource = defined();

  REQUIRE_FALSE(inHeader.empty());
  REQUIRE_FALSE(inSource.empty());

  for (const std::string& name : inSource) {
    INFO("defined in dunya_c.cc but not declared in dunya_c.h: " << name);
    REQUIRE(inHeader.count(name) == 1);
  }
}

TEST_CASE("every declared entry point is defined", "[capi]") {
  const std::set<std::string> inHeader = declared();
  const std::set<std::string> inSource = defined();

  for (const std::string& name : inHeader) {
    INFO("declared in dunya_c.h but not defined in dunya_c.cc: " << name);
    REQUIRE(inSource.count(name) == 1);
  }
}
