#pragma once

#include <dunya/objectmodel/trait/transient/transient.h>
#include <dunya/objectmodel/component/bakedvolume/bakedvolume.h>
#include <dunya/objectmodel/component/deformed/deformed.h>
#include <dunya/objectmodel/component/renderpose/renderpose.h>
#include <dunya/objectmodel/component/rigidbody/rigidbody.h>
#include <dunya/objectmodel/component/sharedfield/sharedfield.h>
#include <dunya/objectmodel/sdfprimitivestore/sdfprimitivestore.h>

namespace dunya::objectmodel {

template<Transient... Ts>
struct TransientList {
  template<typename Fn>
  static void each(Fn&& fn) {
    (fn.template operator()<Ts>(), ...);
  }
};

using TransientComponents = TransientList<
  BakedVolume,
  Deformed,
  RenderPose,
  RigidBody,
  SdfPrimitiveRange,
  SharedSdf>;

}
