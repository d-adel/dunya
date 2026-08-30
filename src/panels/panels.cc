#include "panels.ih"

namespace panels {

void dunya(
  const dunya::objectmodel::World& world,
  const dunya::runtime::Deformation& deformation,
  bool playing,
  double frameMs
) {
  ImGui::Text(
    "%.1f fps   %.2f ms",
    frameMs > 0.0 ? 1000.0 / frameMs : 0.0,
    frameMs
  );
  ImGui::Separator();

  const entt::registry& registry = world.registry();

  size_t resident = 0;
  size_t deformable = 0;

  std::unordered_set<const dunya::field::SampledField*> counted;

  for (const dunya::objectmodel::Entity entity : world.fields()) {
    if (!registry.all_of<dunya::objectmodel::Deformable>(entity)) {
      continue;
    }

    ++deformable;

    if (const auto* field = world.sampledField(entity)) {
      if (counted.insert(field).second) {
        resident += field->distances.size() * sizeof(float)
                    + field->materials.size() * sizeof(uint8_t);
      }
    }
  }

  ImGui::Text("craters carved   %u", deformation.cratersApplied());
  ImGui::Text("deformable       %zu", deformable);
  ImGui::Separator();

  ImGui::Text("primitives       %zu", world.pool().size());
  ImGui::Text(
    "lattice          %.1f MiB",
    double(resident) / (1024.0 * 1024.0)
  );

  ImGui::Separator();

  ImGui::TextUnformatted(
    playing ? "click to fire   F5 stops   G resets"
            : "F5 plays   alt+click carves"
  );
  ImGui::TextUnformatted("hold right mouse to look, WASD/QE to fly");
}

void damage(
  dunya::runtime::Deformation& deformation,
  dunya::physics::ImpactListener* impacts
) {
  dunya::runtime::Deformation::Damage& knobs = deformation.damage();

  if (
    ImGui::SliderFloat("Threshold", &knobs.threshold, 0.1f, 20.0f, "%.1f m/s")
    && impacts != nullptr
  ) {
    impacts->setThreshold(knobs.threshold);
  }

  ImGui::SliderFloat(
    "Depth per impulse",
    &knobs.depthPerImpulse,
    0.0f,
    0.002f,
    "%.5f m"
  );

  ImGui::SliderFloat("Min depth", &knobs.minimumDepth, 0.0f, 0.2f, "%.3f m");
  ImGui::SliderFloat("Max depth", &knobs.maximumDepth, 0.0f, 1.0f, "%.3f m");
  ImGui::SliderFloat("Width", &knobs.radiusPerDepth, 1.0f, 6.0f, "%.2f x");
  ImGui::SliderFloat("Widest", &knobs.widestFraction, 0.02f, 0.5f, "%.2f");

  int perFrame = int(knobs.perFrame);
  if (ImGui::SliderInt("Craters per frame", &perFrame, 1, 16)) {
    knobs.perFrame = uint32_t(perFrame);
  }

  ImGui::Text("Craters  %u", deformation.cratersApplied());
}

void shot(
  Scene::Projectile& settings,
  size_t balls,
  size_t maxBalls,
  const ShotActions& actions
) {
  ImGui::SliderFloat("Speed", &settings.speed, 1.0f, 80.0f, "%.1f m/s");
  ImGui::SliderFloat("Mass", &settings.mass, 1.0f, 3000.0f, "%.0f kg");
  ImGui::SliderFloat3("Target", &settings.aimAt.x, -6.0f, 6.0f);
  ImGui::Text("Balls  %zu / %zu", balls, maxBalls);

  if (ImGui::Button("Fire") && actions.fire) {
    actions.fire();
  }

  ImGui::SameLine();

  if (ImGui::Button("Reset wall") && actions.resetWall) {
    actions.resetWall();
  }
}

void frame(
  double frameMs,
  VkExtent2D extent,
  size_t primitives,
  bool analytic
) {
  ImGui::Text(
    "%.2f ms  %.0f fps",
    frameMs,
    frameMs > 0.0 ? 1000.0 / frameMs : 0.0
  );
  ImGui::Text("%ux%u", extent.width, extent.height);
  ImGui::Text("%zu primitives", primitives);
  ImGui::Text("%s", analytic ? "analytic" : "sampled");
}

void march(dunya::renderer::MarchParams& march) {
  ImGui::SliderFloat(
    "epsilon",
    &march.epsilon,
    0.0001f,
    0.01f,
    "%.5f",
    ImGuiSliderFlags_Logarithmic
  );
  ImGui::SliderFloat("gradient", &march.gradientEpsilon, 0.001f, 0.1f, "%.4f");

  ImGui::SliderFloat("omega", &march.omega, 1.0f, 2.0f);

  ImGui::SliderFloat("max distance", &march.maxDistance, 10.0f, 500.0f);
  ImGui::SliderFloat("shadow distance", &march.shadowMaxDistance, 1.0f, 100.0f);
  ImGui::SliderFloat("shadow sharpness", &march.shadowSharpness, 1.0f, 64.0f);

  int iterations = static_cast<int>(march.maxIterations);
  if (ImGui::SliderInt("max iterations", &iterations, 32, 2000)) {
    march.maxIterations = static_cast<uint32_t>(iterations);
  }
}

}  // namespace panels
