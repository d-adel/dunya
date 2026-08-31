#pragma once

#include <dunya/objectmodel/trait/authored/authored.h>
#include <dunya/objectmodel/component/deformable/deformable.h>
#include <dunya/objectmodel/component/directionallight/directionallight.h>
#include <dunya/objectmodel/component/lens/lens.h>
#include <dunya/objectmodel/component/massscale/massscale.h>
#include <dunya/objectmodel/component/material/material.h>
#include <dunya/objectmodel/component/mesh/mesh.h>
#include <dunya/objectmodel/component/pose/pose.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>
#include <dunya/objectmodel/component/environment/environment.h>
#include <dunya/objectmodel/component/staticbody/staticbody.h>

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
  Lens,
  MassScale,
  StaticBody,
  Deformable,
  Mesh,
  Material,
  DirectionalLight,
  Environment>;

}
