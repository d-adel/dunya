#pragma once

#include <dunya/objectmodel/world/world.h>

namespace dunya::objectmodel {

// Copies an authored world into a runtime one, every entity at the id it had.
// BakedVolume deliberately does not cross: its absence is what makes the frame
// loop allocate a volume of the runtime's own.
void instantiateWorld(const World& source, World& destination);

}  // namespace dunya::objectmodel
