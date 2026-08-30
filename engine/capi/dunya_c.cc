#include "dunya_c.ih"

namespace {

thread_local std::string g_lastError;

void clearError() {
  g_lastError.clear();
}

void recordError(const char* what) {
  g_lastError = what;
}

dunya::capi::Session* asSession(DunyaSession* session) {
  return reinterpret_cast<dunya::capi::Session*>(session);
}

const dunya::capi::Session* asSession(const DunyaSession* session) {
  return reinterpret_cast<const dunya::capi::Session*>(session);
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
      dunya::objectmodel::liveEntities(asSession(session)->world());

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
      asSession(session)->world(),
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
          asSession(session)->world(),
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

const char* dunya_last_error(void) {
  return g_lastError.c_str();
}
}
