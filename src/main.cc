#include "main.ih"

int main(int argc, char** argv) {
  try {
    // Parsed before the device and the scene exist, so a mistyped flag costs a
    // message rather than a full startup.
    const StartupOptions options{std::span(argv, argc)};

    Application application;

    // Not always zero: --golden turns this into a test, and a drifted image has
    // to reach the shell as a failing status or CTest cannot see it.
    return application.start(options);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
