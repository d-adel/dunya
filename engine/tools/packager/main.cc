#include <dunya/editor/package/package.h>

#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  try {
    const std::span<char*> arguments(argv, argc);

    if (arguments.size() < 5) {
      std::cerr << "Usage: DunyaPackager RUNTIME PROJECT OUTPUT WORLD...\n";

      return 1;
    }

    dunya::editor::PackageSpec spec{};
    spec.runtimeExecutable = arguments[1];
    spec.projectRoot = arguments[2];
    spec.output = arguments[3];

    for (size_t at = 4; at < arguments.size(); ++at) {
      spec.worlds.emplace_back(arguments[at]);
    }

    std::string failure;

    if (!dunya::editor::packageProject(spec, failure)) {
      std::cerr << failure << '\n';

      return 1;
    }

    std::cout << dunya::editor::packagedExecutable(spec).string() << '\n';

    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
