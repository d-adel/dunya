#pragma once

#include <dunya/field/sampled/sampled.h>

#include <memory>

namespace dunya::objectmodel {

// The CPU lattice an object reads, held by handle rather than by value so
// objects with the same geometry hold one between them. A crate is 1.2 MB and
// a level is a thousand identical crates.
//
// The handle is what makes copy on write possible and what makes the field's
// address stable without a storage trait: the lattice lives on the heap, so
// nothing moves it when the pool packs. A collision shape borrows that address,
// and a write that finds the lattice shared makes a private copy first - which
// changes the address, which is what tells the shape to rebuild.
struct SharedField {
  std::shared_ptr<dunya::field::SampledField> field;
};

}  // namespace dunya::objectmodel
