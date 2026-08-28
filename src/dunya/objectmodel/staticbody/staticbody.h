#pragma once

namespace dunya::objectmodel {

// Presence is the state: an entity carrying this is simulated but never moved,
// the same shape as BakedVolume meaning "owns a pool slot" by existing.
struct StaticBody {};

// No SelfContained opt-in, and not an oversight: a tag holds nothing, so patch
// and replace have nothing to write. It is added and removed, never assigned.

}  // namespace dunya::objectmodel
