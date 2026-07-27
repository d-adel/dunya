#include "main.ih"

int main() {
  try {
    Application application;
    application.start();
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  return 0;
}
