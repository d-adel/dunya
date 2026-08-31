#pragma once

#include <dunya/serialize/glmmeta/glmmeta.h>

#include <dunya/field/field.h>
#include <dunya/objectmodel/component/sdfgrid/sdfgrid.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>

template<>
struct glz::meta<dunya::field::Primitive> {
  using T = dunya::field::Primitive;

  static constexpr size_t MATRIX_VALUES = 16;

  static constexpr auto readInverse =
    [](T& primitive, const std::array<float, MATRIX_VALUES>& values) {
      std::copy(
        values.begin(),
        values.end(),
        glm::value_ptr(primitive.inverseModel)
      );
    };

  static constexpr auto writeInverse = [](const T& primitive) {
    std::array<float, MATRIX_VALUES> values{};

    std::copy_n(
      glm::value_ptr(primitive.inverseModel),
      values.size(),
      values.begin()
    );

    return values;
  };

  static constexpr auto value = glz::object(
    "inverseModel",
    glz::custom<readInverse, writeInverse>,
    "shape",
    &T::shape,
    "shapeConfig",
    &T::shapeConfig,
    "bounds",
    &T::bounds
  );
};

template<>
struct glz::meta<dunya::objectmodel::SdfGrid> {
  using T = dunya::objectmodel::SdfGrid;

  static constexpr auto value =
    glz::object("resolution", &T::resolution, "margin", &T::margin);
};
