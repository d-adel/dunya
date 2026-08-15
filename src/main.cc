#include "main.ih"

int main(int argc, char** argv) {
  try {
    Application application;
    application.start(StartupOptions(std::span(argv, argc)));
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
