#include "startupoptions.h"

#include <stdexcept>

namespace {

// Reads the value that follows a flag, refusing rather than assuming when it is
// absent. The same shape for every option that takes one, so a new flag cannot
// invent its own way of being wrong.
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

}  // namespace

StartupOptions::StartupOptions(std::span<char*> arguments) {
  // Element zero is the executable. Anything unrecognised is refused rather
  // than ignored: a mistyped flag would otherwise run happily and measure the
  // opposite of what was asked, which is worse than not launching.
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
    } else if (argument == "--carves") {
      const std::string count = valueFor(arguments, i, argument);

      if (count.find_first_not_of("0123456789") != std::string::npos) {
        throw std::runtime_error("--carves needs a count, got: " + count);
      }

      carves = static_cast<uint32_t>(std::stoul(count));
    } else {
      throw std::runtime_error(
        "Unknown argument: " + argument
        + "\nUsage: DunyaRenderer [--analytic] [--carves N] [--verify-bake]"
          " [--screenshot PATH] [--golden PATH]"
      );
    }
  }
}
