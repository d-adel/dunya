#include "dunya_c.ih"

#include <dunya/objectmodel/worldquery/worldquery.h>

namespace {

thread_local std::string g_lastError;

void clearError() {
  g_lastError.clear();
}

void recordError(const char* what) {
  g_lastError = what;
}

template<typename Fn>
int32_t guard(Fn&& body) {
  clearError();

  try {
    body();

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure across the boundary");
  }

  return 1;
}

dunya::capi::Session* asSession(DunyaSession* session) {
  return reinterpret_cast<dunya::capi::Session*>(session);
}

const dunya::capi::Session* asSession(const DunyaSession* session) {
  return reinterpret_cast<const dunya::capi::Session*>(session);
}

std::vector<std::string> splitLines(const char* text) {
  std::vector<std::string> lines;

  std::string current;

  for (const char* at = text; *at != 0; ++at) {
    if (*at == 0x0a) {
      if (!current.empty()) {
        lines.push_back(current);
      }

      current.clear();

      continue;
    }

    current.push_back(*at);
  }

  if (!current.empty()) {
    lines.push_back(current);
  }

  return lines;
}

void fill(
  const std::string& text,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  *length = static_cast<uint32_t>(text.size());

  if (capacity == 0) {
    return;
  }

  const auto written = std::min<size_t>(capacity - 1, text.size());

  std::memcpy(buffer, text.data(), written);

  buffer[written] = '\0';
}

}

extern "C" {

DunyaSession* dunya_session_create(
  void* windowHandle,
  const char* projectRoot,
  const char* world
) {
  clearError();

  try {
    auto windowSystem =
      std::make_unique<dunya::capi::Win32WindowSystem>(windowHandle);

    auto session = std::make_unique<dunya::capi::Session>(
      std::move(windowSystem),
      projectRoot == nullptr ? "projects/demo" : projectRoot,
      world == nullptr ? "main" : world
    );

    return reinterpret_cast<DunyaSession*>(session.release());
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while creating the session");
  }

  return nullptr;
}

void dunya_session_destroy(DunyaSession* session) {
  clearError();

  try {
    delete asSession(session);
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while destroying the session");
  }
}

int32_t dunya_session_resize(DunyaSession* session) {
  clearError();

  if (session == nullptr) {
    recordError("dunya_session_resize was given no session");

    return 1;
  }

  try {
    asSession(session)->resize();

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while resizing the session");
  }

  return 1;
}

int32_t dunya_session_retarget(DunyaSession* session, void* windowHandle) {
  clearError();

  if (session == nullptr) {
    recordError("dunya_session_retarget was given no session");

    return 1;
  }

  try {
    asSession(session)->retarget(
      std::make_unique<dunya::capi::Win32WindowSystem>(windowHandle)
    );

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while retargeting the session");
  }

  return 1;
}

int32_t dunya_session_render(DunyaSession* session) {
  clearError();

  if (session == nullptr) {
    recordError("dunya_session_render was given no session");

    return 1;
  }

  try {
    asSession(session)->render();

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while rendering the session");
  }

  return 1;
}

int32_t dunya_session_extent(
  const DunyaSession* session,
  uint32_t* width,
  uint32_t* height
) {
  clearError();

  if (session == nullptr || width == nullptr || height == nullptr) {
    recordError("dunya_session_extent was given a null argument");

    return 1;
  }

  const VkExtent2D extent = asSession(session)->extent();

  *width = extent.width;
  *height = extent.height;

  return 0;
}

int32_t dunya_session_entities(
  const DunyaSession* session,
  uint32_t* entities,
  uint32_t capacity,
  uint32_t* count
) {
  clearError();

  if (session == nullptr || count == nullptr) {
    recordError("dunya_session_entities was given a null argument");

    return 1;
  }

  if (capacity > 0 && entities == nullptr) {
    recordError("dunya_session_entities was given no buffer");

    return 1;
  }

  try {
    const auto live =
      dunya::objectmodel::liveEntities(asSession(session)->activeWorld());

    *count = static_cast<uint32_t>(live.size());

    const auto written = std::min<size_t>(capacity, live.size());

    for (size_t index = 0; index < written; ++index) {
      entities[index] = static_cast<uint32_t>(live[index]);
    }

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while listing entities");
  }

  return 1;
}

int32_t dunya_session_entity_components(
  const DunyaSession* session,
  uint32_t entity,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (session == nullptr || length == nullptr) {
    recordError("dunya_session_entity_components was given a null argument");

    return 1;
  }

  if (capacity > 0 && buffer == nullptr) {
    recordError("dunya_session_entity_components was given no buffer");

    return 1;
  }

  try {
    const auto names = dunya::objectmodel::componentNames(
      asSession(session)->activeWorld(),
      static_cast<dunya::objectmodel::Entity>(entity)
    );

    std::string joined;

    for (const auto& name : names) {
      if (!joined.empty()) {
        joined.push_back('\n');
      }

      joined.append(name);
    }

    fill(joined, buffer, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while listing components");
  }

  return 1;
}

int32_t dunya_session_component(
  const DunyaSession* session,
  uint32_t entity,
  const char* component,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (session == nullptr || component == nullptr || length == nullptr) {
    recordError("dunya_session_component was given a null argument");

    return 1;
  }

  if (capacity > 0 && buffer == nullptr) {
    recordError("dunya_session_component was given no buffer");

    return 1;
  }

  try {
    std::string json;

    if (!dunya::serialize::readComponent(
          asSession(session)->activeWorld(),
          static_cast<dunya::objectmodel::Entity>(entity),
          component,
          json
        )) {
      recordError("that entity has no such authored component");

      return 1;
    }

    fill(json, buffer, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  } catch (...) {
    recordError("Unknown failure while reading a component");
  }

  return 1;
}

void dunya_set_log_sink(DunyaLogSink sink) {
  dunya::script::setLogSink(sink);
}

const void* dunya_api(void) {
  return &dunya::script::api();
}

void* dunya_session_schedule(DunyaSession* session) {
  if (session == nullptr) {
    return nullptr;
  }

  return &asSession(session)->schedule();
}

void* dunya_session_world(DunyaSession* session) {
  if (session == nullptr) {
    return nullptr;
  }

  return &asSession(session)->world();
}

int32_t dunya_project_create(const char* root, const char* name) {
  return guard([&] {
    if (root == nullptr || name == nullptr) {
      throw std::runtime_error("A project needs a root and a name");
    }

    std::optional<dunya::serialize::Project> made =
      dunya::serialize::Project::create(root, name);

    if (!made.has_value()) {
      throw std::runtime_error("The project could not be created");
    }

    dunya::serialize::StoredMaterial material{};
    material.baseColor = glm::vec4(0.62f, 0.60f, 0.58f, 1.0f);
    material.roughness = 0.85f;

    const std::filesystem::path staged =
      std::filesystem::temp_directory_path() / "default.mat.json";

    if (!dunya::serialize::writeText(
          staged,
          dunya::serialize::writeMaterial(material)
        )) {
      throw std::runtime_error("The default material could not be written");
    }

    const dunya::core::AssetId minted = made->importAsset(staged, "material");

    std::error_code ignored;
    std::filesystem::remove(staged, ignored);

    if (minted == dunya::core::INVALID_ASSET) {
      throw std::runtime_error("The default material could not be registered");
    }

    if (!made->save()) {
      throw std::runtime_error("The project manifest could not be saved");
    }

    if (!made->saveWorld("main", dunya::serialize::StoredWorld{})) {
      throw std::runtime_error("The project has no world");
    }
  });
}

void dunya_session_camera_orbit(DunyaSession* session, float yaw, float pitch) {
  if (session != nullptr) {
    asSession(session)->orbitCamera(yaw, pitch);
  }
}

void dunya_session_camera_pan(DunyaSession* session, float x, float y) {
  if (session != nullptr) {
    asSession(session)->panCamera(x, y);
  }
}

void dunya_session_camera_zoom(DunyaSession* session, float delta) {
  if (session != nullptr) {
    asSession(session)->zoomCamera(delta);
  }
}

void dunya_session_camera_focus(DunyaSession* session, uint32_t entity) {
  if (session == nullptr) {
    return;
  }

  asSession(session)->focusCamera(
    entity == UINT32_MAX
      ? dunya::objectmodel::INVALID_ENTITY
      : static_cast<dunya::objectmodel::Entity>(entt::entity{entity})
  );
}

void dunya_session_align_to_scene_camera(DunyaSession* session) {
  if (session != nullptr) {
    asSession(session)->alignToSceneCamera();
  }
}

uint32_t dunya_session_pick(DunyaSession* session, float x, float y) {
  if (session == nullptr) {
    return UINT32_MAX;
  }

  const dunya::objectmodel::Entity found = asSession(session)->pick(x, y);

  return found == dunya::objectmodel::INVALID_ENTITY
           ? UINT32_MAX
           : static_cast<uint32_t>(entt::to_integral(found));
}

void dunya_session_set_key(
  DunyaSession* session,
  uint32_t virtualKey,
  int32_t down
) {
  if (session != nullptr) {
    asSession(session)->setKey(
      static_cast<uint32_t>(dunya::capi::keyFromWin32(virtualKey)),
      down != 0
    );
  }
}

void dunya_session_set_mouse_button(
  DunyaSession* session,
  uint32_t button,
  int32_t down
) {
  if (session != nullptr) {
    asSession(session)->setMouseButton(button, down != 0);
  }
}

void dunya_session_set_cursor(DunyaSession* session, float x, float y) {
  if (session != nullptr) {
    asSession(session)->setCursor(x, y);
  }
}

uint32_t dunya_session_main_camera(const DunyaSession* session) {
  if (session == nullptr) {
    return UINT32_MAX;
  }

  const dunya::objectmodel::Entity eye =
    dunya::objectmodel::mainCamera(asSession(session)->activeWorld());

  return eye == dunya::objectmodel::INVALID_ENTITY
           ? UINT32_MAX
           : static_cast<uint32_t>(entt::to_integral(eye));
}

int32_t dunya_session_set_main_camera(DunyaSession* session, uint32_t entity) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    if (!asSession(session)->world().setMainCamera(
          static_cast<dunya::objectmodel::Entity>(entity)
        )) {
      throw std::runtime_error("That entity has no camera to make main");
    }
  });
}

uint32_t dunya_session_create_light(DunyaSession* session) {
  if (session == nullptr) {
    return UINT32_MAX;
  }

  return static_cast<uint32_t>(
    entt::to_integral(asSession(session)->createLight())
  );
}

uint32_t dunya_session_create_environment(DunyaSession* session) {
  if (session == nullptr) {
    return UINT32_MAX;
  }

  return static_cast<uint32_t>(
    entt::to_integral(asSession(session)->createEnvironment())
  );
}

uint32_t dunya_session_create_camera(
  DunyaSession* session,
  const float* position,
  const float* target,
  float verticalFov
) {
  if (session == nullptr || position == nullptr || target == nullptr) {
    return UINT32_MAX;
  }

  auto* live = asSession(session);

  const dunya::objectmodel::Entity entity = live->createCamera(
    glm::vec3(position[0], position[1], position[2]),
    glm::vec3(target[0], target[1], target[2]),
    verticalFov
  );

  return static_cast<uint32_t>(entt::to_integral(entity));
}

uint32_t dunya_session_create_sdf(
  DunyaSession* session,
  const float* position,
  const float* rotation,
  const uint32_t* resolution,
  float margin
) {
  if (
    session == nullptr || position == nullptr || rotation == nullptr
    || resolution == nullptr
  ) {
    return UINT32_MAX;
  }

  auto* live = asSession(session);

  dunya::objectmodel::Pose pose{};
  pose.position = glm::vec3(position[0], position[1], position[2]);
  pose.rotation = glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]);

  const dunya::objectmodel::Entity entity = live->createSdf(
    pose,
    glm::uvec3(resolution[0], resolution[1], resolution[2]),
    margin
  );

  return static_cast<uint32_t>(entt::to_integral(entity));
}

int32_t dunya_session_add_primitive(
  DunyaSession* session,
  uint32_t entity,
  uint64_t material,
  const void* edit
) {
  return guard([&] {
    if (session == nullptr || edit == nullptr) {
      throw std::runtime_error("A primitive needs a session and an edit");
    }

    auto* live = asSession(session);

    const auto subject =
      static_cast<dunya::objectmodel::Entity>(entt::entity{entity});

    dunya::script::SdfEditDescriptor described =
      *static_cast<const dunya::script::SdfEditDescriptor*>(edit);

    const uint32_t index = live->materialIndex(material);

    if (index == dunya::core::UNBOUND_ASSET) {
      throw std::runtime_error("The material is not in this project");
    }

    described.material = index;

    if (!live->addPrimitive(
          subject,
          dunya::script::primitiveFor(live->world(), subject, described)
        )) {
      throw std::runtime_error("The primitive was refused");
    }
  });
}

int32_t dunya_session_set_static(DunyaSession* session, uint32_t entity) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    asSession(session)->setStatic(
      static_cast<dunya::objectmodel::Entity>(entt::entity{entity})
    );
  });
}

int32_t dunya_session_set_deformable(DunyaSession* session, uint32_t entity) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    asSession(session)->setDeformable(
      static_cast<dunya::objectmodel::Entity>(entt::entity{entity})
    );
  });
}

int32_t dunya_session_destroy_entity(DunyaSession* session, uint32_t entity) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    if (!asSession(session)->destroyEntity(
          static_cast<dunya::objectmodel::Entity>(entt::entity{entity})
        )) {
      throw std::runtime_error("The entity could not be destroyed");
    }
  });
}

int32_t dunya_session_record(DunyaSession* session, const char* label) {
  return guard([&] {
    if (session == nullptr || label == nullptr) {
      throw std::runtime_error("dunya_session_record was given no session");
    }

    asSession(session)->record(label);
  });
}

int32_t dunya_session_undo(DunyaSession* session) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    static_cast<void>(asSession(session)->undo());
  });
}

int32_t dunya_session_redo(DunyaSession* session) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    static_cast<void>(asSession(session)->redo());
  });
}

int32_t dunya_session_undo_label(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  return guard([&] {
    if (session == nullptr || length == nullptr) {
      throw std::runtime_error("dunya_session_undo_label was given no session");
    }

    if (capacity > 0 && buffer == nullptr) {
      throw std::runtime_error("dunya_session_undo_label was given no buffer");
    }

    const std::optional<std::string_view> label =
      asSession(session)->undoLabel();

    fill(
      std::string{label.value_or(std::string_view{})},
      buffer,
      capacity,
      length
    );
  });
}

int32_t dunya_session_redo_label(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  return guard([&] {
    if (session == nullptr || length == nullptr) {
      throw std::runtime_error("dunya_session_redo_label was given no session");
    }

    if (capacity > 0 && buffer == nullptr) {
      throw std::runtime_error("dunya_session_redo_label was given no buffer");
    }

    const std::optional<std::string_view> label =
      asSession(session)->redoLabel();

    fill(
      std::string{label.value_or(std::string_view{})},
      buffer,
      capacity,
      length
    );
  });
}

int32_t dunya_session_save(DunyaSession* session) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    if (!asSession(session)->save()) {
      throw std::runtime_error("The world could not be saved");
    }
  });
}

int32_t dunya_session_play(DunyaSession* session) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    asSession(session)->play();
  });
}

int32_t dunya_session_stop(DunyaSession* session) {
  return guard([&] {
    if (session == nullptr) {
      throw std::runtime_error("No session");
    }

    asSession(session)->stop();
  });
}

int32_t dunya_session_playing(const DunyaSession* session) {
  return session != nullptr && asSession(session)->playing() ? 1 : 0;
}

uint32_t dunya_session_material_count(const DunyaSession* session) {
  return session == nullptr
           ? 0u
           : static_cast<uint32_t>(asSession(session)->materialCount());
}

uint64_t dunya_session_material_at(
  const DunyaSession* session,
  uint32_t index
) {
  return session == nullptr ? 0u : asSession(session)->materialAt(index);
}

uint64_t dunya_session_add_material(
  DunyaSession* session,
  const float* baseColor,
  float metallic,
  float roughness
) {
  if (session == nullptr || baseColor == nullptr) {
    return 0u;
  }

  auto* live = asSession(session);

  try {
    return live->addMaterial(
      glm::vec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]),
      metallic,
      roughness
    );
  } catch (const std::exception& failure) {
    g_lastError = failure.what();

    return 0u;
  }
}

void dunya_session_view_settings(
  const DunyaSession* session,
  DunyaViewSettings* settings
) {
  if (session == nullptr || settings == nullptr) {
    return;
  }

  const dunya::view::Viewport port = asSession(session)->viewSettings();

  settings->gridVisible = port.gridVisible ? 1 : 0;
  settings->supersample = port.supersample;
  settings->drawMode = static_cast<int32_t>(port.mode);
  settings->fieldRepresentation = port.fieldRepresentation;
}

void dunya_session_set_view_settings(
  DunyaSession* session,
  const DunyaViewSettings* settings
) {
  if (session == nullptr || settings == nullptr) {
    return;
  }

  dunya::view::Viewport port = asSession(session)->viewSettings();

  port.gridVisible = settings->gridVisible != 0;
  port.supersample = settings->supersample;
  port.mode = static_cast<dunya::view::DrawMode>(settings->drawMode);
  port.fieldRepresentation = settings->fieldRepresentation;

  asSession(session)->setViewSettings(port);
}

int32_t dunya_session_open_world(DunyaSession* session, const char* name) {
  return guard([&] {
    if (session == nullptr || name == nullptr) {
      throw std::runtime_error("Opening a world needs a session and a name");
    }

    if (!asSession(session)->openWorld(name)) {
      throw std::runtime_error(std::string("No world named ") + name);
    }
  });
}

int32_t dunya_session_new_world(DunyaSession* session, const char* name) {
  return guard([&] {
    if (session == nullptr || name == nullptr) {
      throw std::runtime_error("A new world needs a session and a name");
    }

    if (!asSession(session)->newWorld(name)) {
      throw std::runtime_error(std::string("Could not create ") + name);
    }
  });
}

int32_t dunya_session_save_as(DunyaSession* session, const char* name) {
  return guard([&] {
    if (session == nullptr || name == nullptr) {
      throw std::runtime_error("Saving needs a session and a name");
    }

    if (!asSession(session)->saveAs(name)) {
      throw std::runtime_error(std::string("Could not save as ") + name);
    }
  });
}

int32_t dunya_session_package(
  DunyaSession* session,
  const char* playerExecutable,
  const char* output,
  const char* worlds,
  char* executable,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (
    session == nullptr || playerExecutable == nullptr || output == nullptr
    || worlds == nullptr || length == nullptr
  ) {
    recordError("dunya_session_package was given a null argument");

    return 1;
  }

  try {
    std::string packaged;

    if (!asSession(session)->package(
          playerExecutable,
          output,
          splitLines(worlds),
          packaged
        )) {
      recordError(packaged.c_str());

      return 1;
    }

    fill(packaged, executable, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());

    return 1;
  }
}

int32_t dunya_session_worlds(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (session == nullptr || length == nullptr) {
    recordError("dunya_session_worlds was given a null argument");

    return 1;
  }

  try {
    fill(asSession(session)->worldNames(), buffer, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  }

  return 1;
}

int32_t dunya_session_assets(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (session == nullptr || length == nullptr) {
    recordError("dunya_session_assets was given a null argument");

    return 1;
  }

  try {
    fill(asSession(session)->assetLines(), buffer, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  }

  return 1;
}

int32_t dunya_session_current_world(
  const DunyaSession* session,
  char* buffer,
  uint32_t capacity,
  uint32_t* length
) {
  clearError();

  if (session == nullptr || length == nullptr) {
    recordError("dunya_session_current_world was given a null argument");

    return 1;
  }

  try {
    fill(asSession(session)->worldName(), buffer, capacity, length);

    return 0;
  } catch (const std::exception& error) {
    recordError(error.what());
  }

  return 1;
}

uint64_t dunya_session_import_asset(
  DunyaSession* session,
  const char* file,
  const char* type
) {
  if (session == nullptr || file == nullptr || type == nullptr) {
    return 0u;
  }

  try {
    return asSession(session)->importAsset(file, type);
  } catch (const std::exception& failure) {
    g_lastError = failure.what();

    return 0u;
  }
}

const char* dunya_last_error(void) {
  return g_lastError.c_str();
}
}
