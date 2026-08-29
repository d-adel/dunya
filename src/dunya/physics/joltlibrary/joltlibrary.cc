#include "joltlibrary.ih"

namespace {

void trace(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::vprintf(format, args);
  va_end(args);

  std::putchar('\n');
  std::fflush(stdout);
}

#ifdef JPH_ENABLE_ASSERTS

bool assertFailed(
  const char* expression,
  const char* message,
  const char* file,
  JPH::uint line
) {
  std::fprintf(
    stderr,
    "%s:%u: (%s) %s\n",
    file,
    line,
    expression,
    message != nullptr ? message : ""
  );

  return true;
}

#endif

}  // namespace

namespace dunya::physics {

JoltLibrary::JoltLibrary() {
  JPH::RegisterDefaultAllocator();

  JPH::Trace = trace;

#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailed = assertFailed;
#endif

  JPH::Factory::sInstance = new JPH::Factory();

  JPH::RegisterTypes();

  // Strictly after RegisterTypes: the decorator shapes overwrite every User
  // slot on their way in, so registering first would be silently undone.
  registerFieldShape();
}

JoltLibrary::~JoltLibrary() {
  JPH::UnregisterTypes();

  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;

  JPH::Trace = nullptr;

#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailed = nullptr;
#endif
}

}  // namespace dunya::physics
