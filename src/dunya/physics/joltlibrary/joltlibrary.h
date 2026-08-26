#pragma once

#include <Jolt/Jolt.h>

namespace dunya::physics {

class JoltLibrary {
public:
  JoltLibrary();
  ~JoltLibrary();

  JoltLibrary(const JoltLibrary&) = delete;
  JoltLibrary& operator=(const JoltLibrary&) = delete;
  JoltLibrary(JoltLibrary&&) = delete;
  JoltLibrary& operator=(JoltLibrary&&) = delete;
};

}  // namespace dunya::physics
