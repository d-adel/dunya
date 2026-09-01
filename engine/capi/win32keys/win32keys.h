#pragma once

#include <dunya/systems/input/input.h>

#include <cstdint>

namespace dunya::capi {

[[nodiscard]] dunya::systems::Key keyFromWin32(uint32_t virtualKey);

}
