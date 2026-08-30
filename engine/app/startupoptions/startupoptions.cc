#include "startupoptions.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace {

std::string valueFor(
  std::span<char*> arguments,
  size_t& at,
  const std::string& flag
) {
  if (at + 1 >= arguments.size()) {
    throw std::runtime_error(flag + " needs a value");
  }

  return arguments[++at];
}

}

StartupOptions::StartupOptions(std::span<char*> arguments) {
  for (size_t i = 1; i < arguments.size(); ++i) {
    const std::string argument = arguments[i];

    if (argument == "--analytic") {
      analytic = true;
    } else if (argument == "--verify-bake") {
      verifyBake = true;
    } else if (argument == "--screenshot") {
      screenshot = valueFor(arguments, i, argument);
    } else if (argument == "--golden") {
      golden = valueFor(arguments, i, argument);
    } else if (argument == "--project") {
      project = valueFor(arguments, i, argument);
    } else if (argument == "--world") {
      world = valueFor(arguments, i, argument);
    } else if (argument == "--export-project") {
      exportProject = valueFor(arguments, i, argument);
    } else if (argument == "--dent-log") {
      dentLog = valueFor(arguments, i, argument);
    } else if (argument == "--dents") {
      const std::string count = valueFor(arguments, i, argument);

      if (count.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("--dents needs a count, got: " + count);
      }

      dents = static_cast<uint32_t>(std::stoul(count));
    } else if (argument == "--carves") {
      const std::string count = valueFor(arguments, i, argument);

      if (count.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("--carves needs a count, got: " + count);
      }

      carves = static_cast<uint32_t>(std::stoul(count));
    } else if (argument == "--demo") {
      const std::string count = valueFor(arguments, i, argument);

      if (count.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("--demo needs a frame count, got: " + count);
      }

      demo = static_cast<uint32_t>(std::stoul(count));
    } else if (argument == "--capture") {
      capture = valueFor(arguments, i, argument);
    } else if (argument == "--demo-rate") {
      demoRate = std::stof(valueFor(arguments, i, argument));
    } else {
      throw std::runtime_error(
        "Unknown argument: " + argument
        + "\nUsage: dunya [--analytic] [--carves N] [--verify-bake]"
          " [--screenshot PATH] [--golden PATH] [--dents N] [--dent-log PATH]"
          " [--demo FRAMES] [--demo-rate PER_SEC]"
          " [--capture DIR] [--project DIR] [--world NAME]"
          " [--export-project DIR]"
      );
    }
  }
}
