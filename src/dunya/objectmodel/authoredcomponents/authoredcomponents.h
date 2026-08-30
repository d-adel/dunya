#pragma once

#include <dunya/objectmodel/authored/authored.h>
#include <dunya/objectmodel/deformable/deformable.h>
#include <dunya/objectmodel/massscale/massscale.h>
#include <dunya/objectmodel/material/material.h>
#include <dunya/objectmodel/mesh/mesh.h>
#include <dunya/objectmodel/pose/pose.h>
#include <dunya/objectmodel/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/staticbody/staticbody.h>

namespace dunya::objectmodel {

template<Authored... Ts>
struct ComponentList {
  template<typename Fn>
  static void each(Fn&& fn) {
    (fn.template operator()<Ts>(), ...);
  }
};

using AuthoredComponents = ComponentList<
  Pose,
  SdfGrid,
  MassScale,
  StaticBody,
  Deformable,
  Mesh,
  Material>;

}
