#include "main.ih"

int main(int argc, char** argv) {
  try {
    const StartupOptions options{std::span(argv, argc)};

    Application application(
      options,
      ImGuiDebugUi::factory(),
      Application::ViewSource::ViewportCamera
    );

    return application.start(options);
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
