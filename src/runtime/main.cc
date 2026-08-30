#include <application/application.h>
#include <startupoptions/startupoptions.h>

#include <cstdlib>
#include <span>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
  try {
    const StartupOptions options{std::span(argv, argc)};

    Application application(options);

    return application.start(options);
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';

    return 1;
  }
}
