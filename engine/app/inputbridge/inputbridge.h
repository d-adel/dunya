#pragma once

#include <dunya/systems/input/input.h>

namespace dunya::app {

[[nodiscard]] dunya::systems::Key keyFromGlfw(int key);

[[nodiscard]] dunya::systems::MouseButton buttonFromGlfw(int button);

}
